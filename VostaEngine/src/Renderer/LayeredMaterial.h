#pragma once

#include "Material.h"
#include "Core/AssetHandle.h"
#include "Renderer/MaterialLayer.h"

#include <string>
#include <vector>

namespace ve {

	// Terrain / landscape material: a stack of surface layers blended by the
	// shared "terrain_layers" shader. A sibling of SingleMaterial — the geometry
	// pass draws both through the Material interface without knowing which is
	// which.
	class VE_API LayeredMaterial : public Material {
	public:
		// On-disk kind tag, distinct from SingleMaterial's "material".
		static constexpr const char* kTypeTag = "layered_material";
		// Body layout version. 2 = layers are MaterialLayerAsset paths; older files
		// (inline layer bodies) are discarded rather than misread.
		static constexpr int32_t kFormatVersion = 2;
		// ShaderLibrary name of the shader this kind always brings.
		static constexpr const char* kShaderName = "terrain_layers";

		std::string name;
		// One handle per layer, in blend order: position 0 is the always-on base.
		// Each resolves to a shared MaterialLayerAsset, so the same surface can be
		// reused across materials.
		std::vector<AssetHandle> layers;

		// World height that maps to a normalised height of 1.0 for the per-layer
		// height rules; keep it in sync with the terrain's height scale.
		float heightScale = 100.0f;
		// Low-frequency albedo tint that hides the texture's own repeat rhythm
		// across large distances.
		float macroStrength = 0.25f;
		float macroScale = 200.0f;

		Ref<Shader> getShader() override;
		void fillBindings(MaterialBindingSet& out) const override;
		const char* typeTag() const override { return kTypeTag; }
		std::string assetPath() const override { return name; }
		void setAssetPath(const std::string& path) override { name = path; }

		void serialize(Archive& ar) const override;
		void deserialize(Archive& ar) override;

		// Writes a fresh single-layer .veasset to `path` (adding the extension if
		// missing). The caller registers it via ResourceManager::store<Material>.
		static void writeNewAsset(const std::string& path);

	private:
		// Resolved from the ShaderLibrary on first use; the library owns it, so
		// the Ref stays valid for the session.
		Ref<Shader> m_shader;
		bool m_shaderLookupDone = false;
	};

}
