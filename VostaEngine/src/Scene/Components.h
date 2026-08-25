#pragma once

#include <glm.hpp>
#include <gtc/type_ptr.hpp>

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Archive.h"
#include "Core/AssetHandle.h"
#include "Core/ResourceManager.h"
#include "Core/Reflection.h"
#include "Renderer/Light.h"
#include "Renderer/Atmosphere.h"
#include "Renderer/Clouds.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Material.h"
#include "Core/ComponentRegistry.h"

namespace ve {
	
	VESTRUCT(NameComponent)
	struct NameComponent {
		VEPROPERTY(NameComponent,std::string,name,"Name","type=input")
		std::string name;
		NameComponent()
			:name("entity") {

		}
		NameComponent(const std::string name)
			:name(name) {

		}

		void serialize(Archive& ar) const {
			ar << name;
		}

		void deserialize(Archive& ar) {
			ar >> name;
		}

	};
	VECOMPONENT(NameComponent, "Name", "Core")

	VESTRUCT(TransformComponent)
	struct TransformComponent {
		VEPROPERTY(TransformComponent, glm::mat4, transform, "transform", "type=drag")
		glm::mat4 transform;

		TransformComponent() = default;
		TransformComponent(const glm::mat4& transform)
			:transform(transform) { 
		
		}

		void serialize(Archive& ar) const {
			float matrix[16];
			memcpy(matrix, glm::value_ptr(transform), sizeof(matrix));
			for (int i = 0; i < 16; ++i) {
				ar << matrix[i];
			}
		}

		void deserialize(Archive& ar) {
			float matrix[16];
			for (int i = 0; i < 16; ++i) {
				ar >> matrix[i];
			}
			transform = glm::make_mat4(matrix);
		}

	};
	VECOMPONENT(TransformComponent, "Transform", "Core")
	// DEBUG
	VESTRUCT(Abilities)
	struct Abilities{
		VEPROPERTY(Abilities, float, atk, "atk", "type=drag,minValue=0,maxValue=10")
		float atk;
	
		VEPROPERTY(Abilities, float, defense, "defense", "type=drag,minValue=0,maxValue=10")
		float defense;

	};
	// DEBUG
	VESTRUCT(Player)
	struct Player {
		VEPROPERTY(Player, float, health, "health", "type=drag,minValue=0,maxValue=100")
		float health;

		VEPROPERTY(Player, std::string, name, "name", "type=input")
		std::string name;

		VEPROPERTY(Player, Abilities, abilities, "abilities", "")
		Abilities abilities;
		
		Player()
			:health(100), name("Vosta") {

		}
	};

	VESTRUCT(LightComponent)
	struct LightComponent {
		AssetHandle lightHandle;

		VEPROPERTY(LightComponent,Light,light,"light","type=drag")
		Light light;
		LightComponent() = default;

		LightComponent(AssetHandle lightHandle)
			:lightHandle(lightHandle){

		}

		void serialize(Archive& ar) const {
			light.serialize(ar);
		}

		void deserialize(Archive& ar) {

			light.deserialize(ar);
		}
	};
	VECOMPONENT(LightComponent, "Light", "Rendering")

	VESTRUCT(SubmeshEntry)
		struct SubmeshEntry {
		std::string name;
		AssetHandle materialHandle;

		void serialize(Archive& ar) const {
			ar << name;
			std::string matPath = ResourceManager::getPath<Material>(materialHandle);
			ar << matPath;
		}

		void deserialize(Archive& ar) {
			ar >> name;
			std::string matPath;
			ar >> matPath;
			if (!matPath.empty()) {
				materialHandle = ResourceManager::store<Material>(matPath);
			}
		}
	};

	VESTRUCT(StaticMeshComponent)
	struct StaticMeshComponent {
		VEPROPERTY(StaticMeshComponent, AssetHandle, staticMeshHandle, "static mesh", "type=mesh")
		AssetHandle staticMeshHandle;
		VEPROPERTY(StaticMeshComponent, AssetHandle, materialHandle, "material", "type=material")
		AssetHandle materialHandle;

		std::vector<SubmeshEntry> submeshEntries;

		StaticMeshComponent() = default;
	
		StaticMeshComponent(AssetHandle staticMeshHandle)
			:staticMeshHandle(staticMeshHandle) {

		}

		StaticMeshComponent(AssetHandle staticMeshHandle, AssetHandle materialHandle)
			:staticMeshHandle(staticMeshHandle),materialHandle(materialHandle) {

		}

		void serialize(Archive& ar) const {
			std::string meshPath = ResourceManager::getPath<StaticMesh>(staticMeshHandle);
			ar << meshPath;

			std::string matPath = ResourceManager::getPath<Material>(materialHandle);
			ar << matPath;

			int32_t submeshCount = static_cast<int32_t>(submeshEntries.size());
			ar << submeshCount;
			for (const auto& entry : submeshEntries) {
				entry.serialize(ar);
			}
		}

		void deserialize(Archive& ar) {
			std::string meshPath;
			ar >> meshPath;
			if (!meshPath.empty()) {
				staticMeshHandle = ResourceManager::store<StaticMesh>(meshPath);
			}

			std::string matPath;
			ar >> matPath;
			if (!matPath.empty()) {
				materialHandle = ResourceManager::store<Material>(matPath);
			}

			int32_t submeshCount = 0;
			ar >> submeshCount;
			submeshEntries.resize(submeshCount);
			for (int32_t i = 0; i < submeshCount; ++i) {
				submeshEntries[i].deserialize(ar);
			}
		}

	};
	VECOMPONENT(StaticMeshComponent, "Static Mesh", "Rendering")

	VESTRUCT(SkyBoxComponent)
	struct SkyBoxComponent {
		VEPROPERTY(SkyBoxComponent, AssetHandle, textureCubeMapHandle, "textureCubeMapHandle","type=skyboxtexture")
		AssetHandle textureCubeMapHandle;

		SkyBoxComponent() = default;

		SkyBoxComponent(AssetHandle textureCubeMapHandle)
			:textureCubeMapHandle(textureCubeMapHandle) {

		}
		void serialize(Archive& ar) const {
			std::string texturePath = ResourceManager::getPath<TextureCubeMap>(textureCubeMapHandle);
			ar << texturePath;
		}

		void deserialize(Archive& ar) {
			std::string texturePath;
			ar >> texturePath;

			if (!texturePath.empty()) {
				textureCubeMapHandle = ResourceManager::store<TextureCubeMap>(texturePath);
			}
		}
	};
	VECOMPONENT(SkyBoxComponent, "Skybox", "Rendering")

	VESTRUCT(SpriteRendererComponent)
	struct SpriteRendererComponent {
		
		VEPROPERTY(SpriteRendererComponent, AssetHandle, textureHandle, "textureHandle", "type=texture")
		AssetHandle textureHandle;
		glm::vec2 size;
		
		SpriteRendererComponent() = default;
		SpriteRendererComponent(const glm::vec2& size, AssetHandle textureHandle)
			: size(size), textureHandle(textureHandle) {
		}


		void serialize(Archive& ar) const {
			ar << size.x << size.y;

			std::string texturePath = ResourceManager::getPath<Texture2D>(textureHandle);
			// = texture ? texture->getTextureFilePath() : "";
			ar << texturePath;
		}

		void deserialize(Archive& ar) {
			ar >> size.x >> size.y;

			std::string texturePath;
			ar >> texturePath;

			if (!texturePath.empty()) {
				// Inside ResourceManager::store, Ref<T> resource = T::create(path) can call the create function
				// so that can create and store Into ResourceManager at once
				textureHandle = ResourceManager::store<Texture2D>(texturePath);
			}
		}

	};
	VECOMPONENT(SpriteRendererComponent, "Sprite", "Rendering")

	VESTRUCT(AtmosphereComponent)
	struct AtmosphereComponent {
		VEPROPERTY(AtmosphereComponent, AtmosphereParams, atmosphere, "atmosphere", "type=drag")
		AtmosphereParams atmosphere;

		VEPROPERTY(AtmosphereComponent, CloudParams, clouds, "clouds", "type=drag")
		CloudParams clouds;

		AtmosphereComponent() = default;

		void serialize(Archive& ar) const {
			atmosphere.serialize(ar);
			clouds.serialize(ar);
		}

		void deserialize(Archive& ar) {
			atmosphere.deserialize(ar);
			clouds.deserialize(ar);
		}
	};
	VECOMPONENT(AtmosphereComponent, "Atmosphere", "Environment")

	VESTRUCT(TerrainComponent)
	struct TerrainComponent {

		VEPROPERTY(TerrainComponent, AssetHandle, heightMapHandle, "Height Map", "type=texture")
		AssetHandle heightMapHandle;

		VEPROPERTY(TerrainComponent, AssetHandle, terrainMaterialHandle, "Material", "type=material")
		AssetHandle terrainMaterialHandle;

		VEPROPERTY(TerrainComponent, float, tileSize, "Tile Size", "type=drag,minValue=0.1,maxValue=100.0")
		float tileSize = 1.0f;

		VEPROPERTY(TerrainComponent, float, heightScale, "Height Scale", "type=drag,minValue=0.01,maxValue=100.0")
		float heightScale = 1.0f;

		// Generated mesh handle — registered with ResourceManager on build
		AssetHandle generatedMeshHandle;
		bool bDirty = true;

		TerrainComponent() = default;

		void serialize(Archive& ar) const {
			std::string heightMapPath = ResourceManager::getPath<Texture2D>(heightMapHandle);
			ar << heightMapPath;

			std::string matPath = ResourceManager::getPath<Material>(terrainMaterialHandle);
			ar << matPath;

			ar << tileSize;
			ar << heightScale;
		}

		void deserialize(Archive& ar) {
			std::string heightMapPath;
			ar >> heightMapPath;
			if (!heightMapPath.empty()) {
				heightMapHandle = ResourceManager::store<Texture2D>(heightMapPath);
			}

			std::string matPath;
			ar >> matPath;
			if (!matPath.empty()) {
				terrainMaterialHandle = ResourceManager::store<Material>(matPath);
			}

			ar >> tileSize;
			ar >> heightScale;

			bDirty = true;
		}

	};
	VECOMPONENT(TerrainComponent, "Terrain", "Environment")


}