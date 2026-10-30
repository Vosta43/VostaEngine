#pragma once

#include <string>
#include <cstdint>
#include "Core/Core.h"
#include "Asset/TextureFormat.h"

namespace ve {

	struct TextureResource;

	// What a shader may do with a texture bound to an image unit.
	enum class ImageAccess {
		Read,
		Write,
		ReadWrite
	};

	// GL's filter modes. The four mipmap variants only take effect when the
	// texture was allocated with a mip chain (SamplerDesc::mipmaps or an explicit
	// level count); otherwise they fall back to their non-mip counterpart.
	enum class TextureFilter : uint8_t {
		LINEAR,
		NEAREST,
		NEAREST_MIPMAP_NEAREST,
		LINEAR_MIPMAP_NEAREST,
		NEAREST_MIPMAP_LINEAR,
		LINEAR_MIPMAP_LINEAR
	};

	enum class TextureWrap : uint8_t {
		ClampToEdge,
		Repeat,
		MirrorRepeat
	};

	// Sampling state fixed on the texture at creation. Integer formats ignore the
	// filters and always sample NEAREST, since GL has no linear filter for them.
	struct SamplerDesc {
		TextureFilter minFilter = TextureFilter::LINEAR;
		TextureFilter magFilter = TextureFilter::LINEAR;
		TextureWrap wrapS = TextureWrap::ClampToEdge;
		TextureWrap wrapT = TextureWrap::ClampToEdge;
		TextureWrap wrapR = TextureWrap::ClampToEdge;
		// Allocates the full mip chain and samples across levels with the min
		// filter. The chain itself is only written by genMipmaps(), so callers
		// upload level 0 first and then generate.
		bool mipmaps = false;
	};

	class VE_API Texture {
	public:
		virtual ~Texture() = default;

		virtual uint32_t getWidth() const = 0;
		virtual uint32_t getHeight() const = 0;

		virtual void setData(void* data, uint32_t size) = 0;
		// Activate (bind) this texture to the currently active texture unit.
		// The caller is responsible for setting the active unit via the shader or slot manager.
		virtual void bind(uint32_t slot) const = 0;

		// Binds this texture to an image unit, which a shader addresses with an
		// explicit ivec instead of a sampling coordinate: no filtering, no mip
		// selection, and -- with a write access -- imageStore / imageAtomic*.
		// level / layered / layer pick the level and the array slices to expose.
		virtual void bindImage(uint32_t unit, ImageAccess access, int level = 0,
			bool layered = false, int layer = 0) const = 0;
		// Detaches whatever is bound to the image unit.
		virtual void unbindImage(uint32_t unit) const = 0;

		virtual uint32_t getRendererID() const = 0;

		// The format this texture was created with, or NONE when the class does
		// not track one.
		virtual TextureFormat getFormat() const { return TextureFormat::NONE; }

		virtual const std::string& getTextureFilePath() const = 0;
	};

	class VE_API Texture2D : public Texture {
	public:
		// Factory method: loads and constructs a platform-specific 2D texture from the given file path. Returns null on failure;
		// the caller must check the returned Ref before use.
		// The underlying format is inferred from the file extension (e.g., PNG, JPEG, TGA).
		static Ref<Texture2D> create(const std::string& path);

		static Ref<TextureResource> load(const std::string& path);

		static Ref<Texture2D> upload(Ref<TextureResource> textureResource);

		// Stand-in stored while an async load is in flight: a 1x1 neutral white
		// texture, so binding it never samples garbage.
		static Ref<Texture2D> createPlaceholder();

		static Ref<Texture2D> create(Ref<TextureResource> resource);
		// Factory: creates an uninitialized 2D texture with the given dimensions.
		// The texture data is uninitialized (garbage) — use this for render targets,
		// post-processing pools, or when the caller will upload pixel data separately.
		static Ref<Texture2D> create(uint32_t width, uint32_t height);

		static Ref<Texture2D> create(uint32_t rendererID, uint32_t width, uint32_t height);
		// Wraps an existing GL texture without taking ownership; the format must
		// match the storage, since getFormat drives readback and memory accounting.
		static Ref<Texture2D> create(uint32_t rendererID, uint32_t width, uint32_t height,
			TextureFormat format);
		// Creates an empty 2D texture with the given format (RGBA, RGB, R8, RG16F, etc.).
		static Ref<Texture2D> create(uint32_t width, uint32_t height, TextureFormat format);
		// Same, but with a mip chain of `mipLevels` levels allocated up front. Nothing
		// generates them: every level is uploaded by the caller, because a page table's
		// levels are independent data, not a filtered pyramid of level 0.
		static Ref<Texture2D> create(uint32_t width, uint32_t height, TextureFormat format, uint32_t mipLevels);
		// Empty texture with explicit sampling state. With sampler.mipmaps the whole
		// chain is allocated; call genMipmaps() after uploading level 0.
		static Ref<Texture2D> create(uint32_t width, uint32_t height, TextureFormat format,
			const SamplerDesc& sampler);
		static Ref<Texture2D> create(uint32_t width, uint32_t height, TextureFormat format,
			uint32_t mipLevels, const SamplerDesc& sampler);

		// Rebuilds the chain from level 0. The texture must have been allocated with
		// room for it (sampler.mipmaps or an explicit mipLevels).
		virtual void genMipmaps() = 0;

		// Uploads a w×h block of pixels into the (x, y) corner of the texture,
		// leaving the rest untouched. Lets a dynamic atlas grow a glyph at a time
		// instead of re-uploading the whole surface. rowPitchTexels is the stride
		// between source rows; 0 means tightly packed.
		virtual void setSubData(const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
			int32_t rowPitchTexels = 0) = 0;
		// Uploads `data` (that level's tightly-packed pixels) into mip level `mip`.
		// Level 0 is equivalent to setData().
		virtual void setDataLevel(void* data, uint32_t size, uint32_t mip) = 0;
	};

	class VE_API TextureCubeMap : public Texture {
	public:
		static Ref<TextureCubeMap> create(const std::string& path);
		static Ref<TextureCubeMap> create(uint32_t faceSize);
		static Ref<TextureCubeMap> create(uint32_t faceSize, uint32_t mipLevels);
	};

	// An array of same-sized 2D slices, sampled as a whole (sampler2DArray). Each
	// slice is addressed by a filterable coordinate, so a shader can select a slice
	// with a dynamic index — unlike a sampler array, which needs a constant index.
	class VE_API Texture2DArray : public Texture {
	public:
		// Creates an array of `layers` slices, each width x height, in the given
		// format. `data` (if non-null) holds width*height*layers values, one slice
		// after another; upload it with setData otherwise.
		static Ref<Texture2DArray> create(uint32_t width, uint32_t height, uint32_t layers,
			TextureFormat format, const void* data);
		// Same, with explicit sampling state; see Texture2D's SamplerDesc overloads.
		static Ref<Texture2DArray> create(uint32_t width, uint32_t height, uint32_t layers,
			TextureFormat format, const void* data, const SamplerDesc& sampler);

		virtual uint32_t getLayers() const = 0;

		// Uploads `data` (that level's pixels for every slice, slice-major) into
		// mip level `mip`. For hand-built chains: allocate with sampler.mipmaps
		// and upload every level, since a filtered pyramid is wrong for atlases.
		virtual void setDataLevel(void* data, uint32_t size, uint32_t mip) = 0;

		// Rebuilds the chain from level 0 across every layer.
		virtual void genMipmaps() = 0;
	};

	class VE_API Texture3D : public Texture {
	public:
		// Creates a single-channel (R16F) 3D texture from CPU pixel data:
		// `data` holds width*height*depth floats. No mipmaps; linear filter,
		// repeat wrap on all three axes. Used for baked noise volumes.
		static Ref<Texture3D> create(uint32_t width, uint32_t height, uint32_t depth, const float* data);
		// Creates a float 3D texture in the given format (e.g. RGBA16F) from CPU
		// pixel data. No mipmaps; linear filter. Used for precomputed atmospheric
		// scattering LUTs (clamp-to-edge wrap, the default) and multi-channel
		// periodic noise volumes (pass repeatWrap=true so octave stacks stay
		// seamless like the single-channel bake).
		static Ref<Texture3D> create(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, const float* data, bool repeatWrap = false);

		virtual uint32_t getDepth() const = 0;
	};

}
