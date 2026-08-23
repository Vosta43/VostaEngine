#pragma once

#include "Texture.h"
#include "Core/AssetHandle.h"
#include "Core/Reflection.h"
#include "Renderer/Shader.h"
#include "Renderer/MaterialGraph.h"
#include "Scene/Archive.h"

#include <glm.hpp>
#include <string>

namespace ve {

	// Texture binding produced by material-graph compilation.
	// Each TextureSamplerNode writes one entry.
	struct GraphTextureBinding {
		std::string uniformName;   // e.g. "u_MatTex_0"
		AssetHandle textureHandle;
	};

	VESTRUCT(Material)
	struct VE_API Material {
		VEPROPERTY(Material,std::string,name,"Name","type=input")
		std::string name;
		VEPROPERTY(Material, AssetHandle, albedoMapHandle, "Albedo", "type=texture")
		AssetHandle albedoMapHandle;
		VEPROPERTY(Material, AssetHandle, normalMapHandle, "Normal", "type=texture")
		AssetHandle normalMapHandle;
		VEPROPERTY(Material, AssetHandle, metallicMapHandle, "Metallic", "type=texture")
		AssetHandle metallicMapHandle;
		VEPROPERTY(Material, AssetHandle, roughnessMapHandle, "Roughness", "type=texture")
		AssetHandle roughnessMapHandle;
		VEPROPERTY(Material, AssetHandle, aoMapHandle, "AO", "type=texture")
		AssetHandle aoMapHandle;
		VEPROPERTY(Material, AssetHandle, emissiveMapHandle, "Emissive", "type=texture")
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

		bool hasAlbedoMap() const   { return albedoMapHandle.isValid(); }
		bool hasNormalMap() const   { return normalMapHandle.isValid(); }
		bool hasEmissiveMap() const { return emissiveMapHandle.isValid(); }

		Ref<Shader> getShader();

		// Compile the material graph into customShader.
		// Returns false (PBR-fallback mode) when the graph is empty
		// (only the MaterialOutputNode, no user-added nodes).
		bool compile();

		// --- Serialization (.veasset) ---
		// First token written/read is always the string "material".
		void serialize(Archive& ar) const;
		void deserialize(Archive& ar);

		static Ref<Material> create(const std::string& path);
	};

}
