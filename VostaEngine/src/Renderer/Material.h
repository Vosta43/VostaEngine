#pragma once

#include "Texture.h"
#include "Core/AssetHandle.h"
#include "Renderer/Shader.h"
#include "Scene/Archive.h"

#include <glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ve {

	// Texture binding produced by material-graph compilation.
	// Each TextureSamplerNode writes one entry.
	struct GraphTextureBinding {
		std::string uniformName;   // e.g. "u_MatTex_0"
		AssetHandle textureHandle;
	};

	// Which GL sampler a texture binding targets, so the pass binds it to the right
	// texture type instead of assuming 2D.
	enum class TextureTarget { Texture2D, Texture2DArray };

	// One named texture slot a material wants bound for a draw.
	struct MaterialTextureBinding {
		std::string   uniformName;
		AssetHandle   textureHandle;
		uint32_t      unit = 0;
		TextureTarget target = TextureTarget::Texture2D;
	};

	// One named scalar/vector uniform a material wants set for a draw.
	template<typename T>
	struct MaterialUniformBinding {
		std::string name;
		T value;
	};

	// Everything a material wants bound for a single draw. The geometry pass
	// asks each material to fill this and does no lookups of its own, so it never
	// needs to know which concrete kind of material it is drawing.
	struct MaterialBindingSet {
		std::vector<MaterialTextureBinding> textures;
		std::vector<MaterialUniformBinding<int>> ints;
		std::vector<MaterialUniformBinding<float>> floats;
		std::vector<MaterialUniformBinding<glm::vec2>> vec2s;
		std::vector<MaterialUniformBinding<glm::vec3>> vec3s;
	};

	// Abstract surface material. Concrete kinds (SingleMaterial, LayeredMaterial)
	// differ only in the shader they bring and the bindings they produce; the
	// geometry pass depends on this interface alone.
	class VE_API Material {
	public:
		virtual ~Material() = default;

		// Shader the geometry pass binds. Null means "use the pass's default".
		virtual Ref<Shader> getShader() = 0;

		// Shader that samples the frame's runtime virtual texture in place of
		// evaluating this material's own surface, or null when this material has no
		// RVT path (or has it switched off). The geometry pass prefers it whenever
		// the frame has an RVT, and otherwise falls back to getShader(). Filling the
		// RVT is a separate job -- the bake pass always uses getShader().
		virtual Ref<Shader> getRVTShader() { return nullptr; }

		// Fill this material's per-draw bindings. The caller owns the set and
		// reuses it across draws, so implementations clear-then-append.
		virtual void fillBindings(MaterialBindingSet& out) const = 0;

		// On-disk kind tag: the first token of the .veasset file.
		virtual const char* typeTag() const = 0;

		// Asset-relative path this material was loaded from. The resource registry
		// owns this identity, so create() re-stamps it after loading; the copy
		// stored in the file body is not trusted (it goes stale on rename/move).
		virtual std::string assetPath() const { return {}; }
		virtual void setAssetPath(const std::string&) {}

		// Bumped whenever the material's authored content changes in a way that
		// affects generated code (a recompile, a reload from disk). Consumers that
		// build a shader from this material fold the number into their cache key, so
		// editing it invalidates their shader without any change to its handle.
		uint32_t revision() const { return m_revision; }

		// serialize() writes typeTag() first; deserialize() reads it back.
		virtual void serialize(Archive& ar) const = 0;
		virtual void deserialize(Archive& ar) = 0;

		// Factory: reads the file's type tag and builds the matching kind.
		// Returns null when the tag names no known kind.
		static Ref<Material> create(const std::string& path);

		// Reload the material registered at `path` from disk, in place, so every
		// holder of its handle (a drawn mesh, the open editor) sees the new content
		// without re-fetching. A compiled material rebuilds its shader (deserialize
		// recompiles). No-op when nothing is registered at `path`. Returns true when
		// a reload happened.
		static bool refreshFromFile(const std::string& path);

	protected:
		// Concrete kinds call this when their authored content changes (see revision()).
		void bumpRevision() { ++m_revision; }

	private:
		uint32_t m_revision = 0;
	};

}
