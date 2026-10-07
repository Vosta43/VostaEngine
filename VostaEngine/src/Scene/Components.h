#pragma once

#include <glm.hpp>
#include <gtc/type_ptr.hpp>

#include "Core/Core.h"
#include "Core/Json.h"
#include "Renderer/Texture.h"
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

	// Asset handles persist as their registered path string and are re-resolved
	// through ResourceManager on load. An empty path means "no asset", which is
	// distinct from a path that failed to resolve (logged as a warning).
	template<typename T>
	inline void writeAssetPath(JsonWriter& w, const char* key, AssetHandle handle) {
		w.set(key, handle.isValid() ? ResourceManager::getPath<T>(handle) : std::string());
	}

	template<typename T>
	inline AssetHandle readAssetPath(const JsonReader& r, const char* key) {
		const std::string path = r.getString(key, std::string());
		if (path.empty())
			return INVALID_ASSET_HANDLE;
		AssetHandle handle = ResourceManager::store<T>(path);
		if (!handle.isValid())
			VE_CORE_WARN_PRINT("Scene load: could not resolve asset '%s'", path.c_str());
		return handle;
	}

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

		void serialize(JsonWriter& w) const {
			w.set("name", name);
		}

		void deserialize(const JsonReader& r) {
			name = r.getString("name", name);
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

		void serialize(JsonWriter& w) const {
			w.set("transform", transform);
		}

		void deserialize(const JsonReader& r) {
			transform = r.getMat4("transform", transform);
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

		void serialize(JsonWriter& w) const {
			w.beginObject("light");
			light.serialize(w);
			w.end();
		}

		void deserialize(const JsonReader& r) {
			light.deserialize(r.child("light"));
		}
	};
	VECOMPONENT(LightComponent, "Light", "Rendering")

	VESTRUCT(SubmeshEntry)
		struct SubmeshEntry {
		std::string name;
		AssetHandle materialHandle;

		void serialize(JsonWriter& w) const {
			w.set("name", name);
			writeAssetPath<Material>(w, "material", materialHandle);
		}

		void deserialize(const JsonReader& r) {
			name = r.getString("name", std::string());
			materialHandle = readAssetPath<Material>(r, "material");
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

		void serialize(JsonWriter& w) const {
			writeAssetPath<StaticMesh>(w, "staticMesh", staticMeshHandle);
			writeAssetPath<Material>(w, "material", materialHandle);

			w.beginArray("submeshes", submeshEntries.size());
			for (const auto& entry : submeshEntries) {
				w.beginObject();
				entry.serialize(w);
				w.end();
			}
			w.end();
		}

		void deserialize(const JsonReader& r) {
			staticMeshHandle = readAssetPath<StaticMesh>(r, "staticMesh");
			materialHandle = readAssetPath<Material>(r, "material");

			const size_t submeshCount = r.arraySize("submeshes");
			submeshEntries.resize(submeshCount);
			for (size_t i = 0; i < submeshCount; ++i) {
				submeshEntries[i].deserialize(r.at("submeshes", i));
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
		void serialize(JsonWriter& w) const {
			writeAssetPath<TextureCubeMap>(w, "texture", textureCubeMapHandle);
		}

		void deserialize(const JsonReader& r) {
			textureCubeMapHandle = readAssetPath<TextureCubeMap>(r, "texture");
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


		void serialize(JsonWriter& w) const {
			w.set("size", size);
			writeAssetPath<Texture2D>(w, "texture", textureHandle);
		}

		void deserialize(const JsonReader& r) {
			size = r.getVec2("size", size);
			textureHandle = readAssetPath<Texture2D>(r, "texture");
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

		void serialize(JsonWriter& w) const {
			w.beginObject("atmosphere");
			atmosphere.serialize(w);
			w.end();

			w.beginObject("clouds");
			clouds.serialize(w);
			w.end();
		}

		void deserialize(const JsonReader& r) {
			atmosphere.deserialize(r.child("atmosphere"));
			clouds.deserialize(r.child("clouds"));
		}
	};
	VECOMPONENT(AtmosphereComponent, "Atmosphere", "Environment")

	// Runtime quadtree LOD object; not serialized (rebuilt from the heightmap).
	class QuadTreeTerrain;

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

		// --- Quadtree LOD ---
		// Max quadtree depth. The deepest level samples the heightmap at its
		// native texel stride; every chunk keeps `segments` divisions per edge.
		// Only meaningful when the quadtree terrain system is wired up.
		VEPROPERTY(TerrainComponent, int, maxDepth, "Max LOD Depth", "type=drag,speed=1,minValue=1,maxValue=12")
		int maxDepth = 6;

		VEPROPERTY(TerrainComponent, int, segments, "Chunk Segments", "type=drag,speed=1,minValue=4,maxValue=64")
		int segments = 32;

		// Subdivide a node while dist(camera, node) < nodeWorldSize * lodDetail.
		// Higher = more subdivision = more detail at the same distance.
		VEPROPERTY(TerrainComponent, float, lodDetail, "LOD Detail", "type=drag,minValue=1,maxValue=128")
		float lodDetail = 16.0f;

		// Chunks farther than this (world units) from the camera are culled.
		VEPROPERTY(TerrainComponent, float, renderDistance, "LOD Render Distance", "type=drag,minValue=100,maxValue=100000")
		float renderDistance = 10000.0f;

		// Generated mesh handle — registered with ResourceManager on build
		AssetHandle generatedMeshHandle;
		// Runtime quadtree LOD object owning the shared mesh + per-chunk ranges.
		// Null until the Generate button builds it; never serialized.
		Ref<QuadTreeTerrain> quadtree;
		bool bDirty = true;

		TerrainComponent() = default;

		void serialize(JsonWriter& w) const {
			writeAssetPath<Texture2D>(w, "heightMap", heightMapHandle);
			writeAssetPath<Material>(w, "material", terrainMaterialHandle);

			w.set("tileSize", tileSize);
			w.set("heightScale", heightScale);
			w.set("maxDepth", maxDepth);
			w.set("segments", segments);
			w.set("lodDetail", lodDetail);
			w.set("renderDistance", renderDistance);
		}

		void deserialize(const JsonReader& r) {
			heightMapHandle = readAssetPath<Texture2D>(r, "heightMap");
			terrainMaterialHandle = readAssetPath<Material>(r, "material");

			tileSize = r.getFloat("tileSize", tileSize);
			heightScale = r.getFloat("heightScale", heightScale);
			maxDepth = r.getInt("maxDepth", maxDepth);
			segments = r.getInt("segments", segments);
			lodDetail = r.getFloat("lodDetail", lodDetail);
			renderDistance = r.getFloat("renderDistance", renderDistance);

			bDirty = true;
		}

	};
	VECOMPONENT(TerrainComponent, "Terrain", "Environment")


}