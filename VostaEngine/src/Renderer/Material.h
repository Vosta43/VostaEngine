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

	// One named texture slot a material wants bound for a draw.
	struct MaterialTextureBinding {
		std::string uniformName;
		AssetHandle textureHandle;
		uint32_t    unit = 0;
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

		// serialize() writes typeTag() first; deserialize() reads it back.
		virtual void serialize(Archive& ar) const = 0;
		virtual void deserialize(Archive& ar) = 0;

		// Factory: reads the file's type tag and builds the matching kind.
		// Returns null when the tag names no known kind.
		static Ref<Material> create(const std::string& path);
	};

}
