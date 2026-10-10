#pragma once

#include <string>
#include "Core/Core.h"
#include "Asset/TextureResource.h"

namespace ve {
	class VE_API Texture {
	public:
		virtual ~Texture() = default;

		virtual uint32_t getWidth() const = 0;
		virtual uint32_t getHeight() const = 0;

		virtual void setData(void* data, uint32_t size) = 0;
		// Activate (bind) this texture to the currently active texture unit.
		// The caller is responsible for setting the active unit via the shader or slot manager.
		virtual void bind(uint32_t slot) const = 0;

		virtual uint32_t getRendererID() const = 0;

		virtual const std::string& getTextureFilePath() const = 0;
	};

	class VE_API Texture2D : public Texture {
	public:
		// Factory method: loads and constructs a platform-specific 2D texture from the given file path. Returns null on failure;
		// the caller must check the returned Ref before use.
		// The underlying format is inferred from the file extension (e.g., PNG, JPEG, TGA).
		static Ref<Texture2D> create(const std::string& path);

		static Ref<Texture2D> create(Ref<TextureResource> resource);
		// Factory: creates an uninitialized 2D texture with the given dimensions.
		// The texture data is uninitialized (garbage) — use this for render targets,
		// post-processing pools, or when the caller will upload pixel data separately.
		static Ref<Texture2D> create(uint32_t width, uint32_t height);

		static Ref<Texture2D> create(uint32_t rendererID, uint32_t width, uint32_t height);
		// Creates an empty 2D texture with the given format (RGBA, RGB, R8, RG16F, etc.).
		static Ref<Texture2D> create(uint32_t width, uint32_t height, TextureFormat format);

		// Uploads a w×h block of tightly-packed pixels into the (x, y) corner of the
		// texture, leaving the rest untouched. Lets a dynamic atlas grow a glyph at a
		// time instead of re-uploading the whole surface.
		virtual void setSubData(const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;
	};

	class VE_API TextureCubeMap : public Texture {
	public:
		static Ref<TextureCubeMap> create(const std::string& path);
		static Ref<TextureCubeMap> create(uint32_t faceSize);
		static Ref<TextureCubeMap> create(uint32_t faceSize, uint32_t mipLevels);
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
