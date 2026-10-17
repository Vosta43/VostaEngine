#pragma once

#include "Material.h"
#include "Core/AssetHandle.h"
#include "Renderer/MaterialLayer.h"

#include <string>
#include <vector>

namespace ve {

	// Terrain / landscape material: a stack of surface layers, each referencing a
	// material whose node graph (or fixed PBR slots) is evaluated per-layer and
	// blended. A sibling of SingleMaterial — the geometry pass draws both through
	// the Material interface without knowing which is which.
	class VE_API LayeredMaterial : public Material {
	public:
		// On-disk kind tag, distinct from SingleMaterial's "material".
		static constexpr const char* kTypeTag = "layered_material";
		// Body layout version. 2 = layers are MaterialLayerAsset paths; older files
		// (inline layer bodies) are discarded rather than misread.
		static constexpr int32_t kFormatVersion = 2;

		std::string name;
		// One handle per layer, in blend order: position 0 is the always-on base.
		// Each resolves to a shared MaterialLayerAsset, so the same surface can be
		// reused across materials.
		std::vector<AssetHandle> layers;

		// World height that maps to a normalised height of 1.0 when a layer's height
		// rule is baked on the CPU; keep it in sync with the terrain's height scale.
		float heightScale = 100.0f;
		// Low-frequency albedo tint that hides the texture's own repeat rhythm
		// across large distances.
		float macroStrength = 0.25f;
		float macroScale = 200.0f;

		// Shade from the terrain's baked RVT instead of running the layer stack per
		// pixel. Off unless the scene has a terrain to bake one from.
		bool sampleRVT = false;

		// Builds (and caches) the generated shader this kind always brings: one GLSL
		// function per layer spliced by LayeredShaderBuilder. Null when the build or
		// the GL compile fails, so the pass falls back to its own default shader
		// instead of binding a broken one.
		Ref<Shader> getShader() override;

		// The RVT-consuming variant; null when sampleRVT is off or the load failed.
		Ref<Shader> getRVTShader() override;
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
		// Key of the last generated shader: layer handles, each resolved material
		// handle and its revision, and whether that layer carries an evaluable graph.
		// Deliberately excludes the rule scalars (tiling, blend) — those travel as
		// uniforms, so editing them must not recompile the shader. The slope/height/
		// noise rules are no longer shader inputs at all; they are baked into the
		// weight array on the CPU.
		std::string structureKey() const;

		Ref<Shader> m_shader;
		// Material-independent: loaded from a fixed resource file, driven entirely by
		// the frame's virtual-grid uniforms.
		Ref<Shader> m_rvtShader;
		std::string m_shaderKey;
		// The content samplers the generated shader declared, replayed by
		// fillBindings so the pass binds exactly what was compiled.
		std::vector<MaterialTextureBinding> m_cachedTextures;
	};

}
