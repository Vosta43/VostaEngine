#pragma once

#include "Material.h"
#include "Core/Reflection.h"
#include "Renderer/MaterialGraph.h"

#include <string>
#include <vector>

namespace ve {

	// Classic single-surface PBR material: one set of maps and scalar params,
	// optionally replaced by a compiled material graph. On disk it keeps the
	// original "material" tag, so existing .veasset files load unchanged.
	VESTRUCT(SingleMaterial)
	struct VE_API SingleMaterial : public Material {
		VEPROPERTY(SingleMaterial,std::string,name,"Name","type=input")
		std::string name;

		VEPROPERTY(SingleMaterial, AssetHandle, albedoMapHandle, "Albedo", "type=texture")
		AssetHandle albedoMapHandle;
		VEPROPERTY(SingleMaterial, AssetHandle, normalMapHandle, "Normal", "type=texture")
		AssetHandle normalMapHandle;
		VEPROPERTY(SingleMaterial, AssetHandle, metallicMapHandle, "Metallic", "type=texture")
		AssetHandle metallicMapHandle;
		VEPROPERTY(SingleMaterial, AssetHandle, roughnessMapHandle, "Roughness", "type=texture")
		AssetHandle roughnessMapHandle;
		VEPROPERTY(SingleMaterial, AssetHandle, aoMapHandle, "AO", "type=texture")
		AssetHandle aoMapHandle;
		VEPROPERTY(SingleMaterial, AssetHandle, emissiveMapHandle, "Emissive", "type=texture")
		AssetHandle emissiveMapHandle;

		// Default params when no texture map is assigned.
		glm::vec3 albedoColor = glm::vec3(1.0f);
		float metallic = 0.0f;
		float roughness = 0.5f;
		float ao = 1.0f;
		glm::vec3 emissiveColor = glm::vec3(0.0f);

		// --- Material graph ---
		MaterialGraph graph;

		bool isCompiled = false;
		Ref<Shader> customShader;
		std::string compileError;

		// Texture bindings filled by compile() from graph TextureSamplerNodes.
		std::vector<GraphTextureBinding> graphTextures;

		static constexpr const char* kTypeTag = "material";

		bool hasAlbedoMap() const   { return albedoMapHandle.isValid(); }
		bool hasNormalMap() const   { return normalMapHandle.isValid(); }
		bool hasEmissiveMap() const { return emissiveMapHandle.isValid(); }

		// albedo / normal / metallic / roughness / AO occupy units 0..4; graph
		// sampler nodes are assigned the units above them.
		static constexpr uint32_t kFixedTextureUnits = 5;

		Ref<Shader> getShader() override;
		void fillBindings(MaterialBindingSet& out) const override;
		const char* typeTag() const override { return kTypeTag; }
		std::string assetPath() const override { return name; }
		void setAssetPath(const std::string& path) override { name = path; }

		// Compile the material graph into customShader.
		// Returns false (PBR-fallback mode) when the graph is empty
		// (only the MaterialOutputNode, no user-added nodes).
		bool compile();

		void serialize(Archive& ar) const override;
		void deserialize(Archive& ar) override;
	};

}
