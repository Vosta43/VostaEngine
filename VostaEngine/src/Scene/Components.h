#pragma once

#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/quaternion.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Core/Core.h"
#include "Core/Types.h"
#include "Core/Json.h"
#include "Renderer/Texture.h"
#include "Core/AssetHandle.h"
#include "Core/ResourceManager.h"
#include "Core/Reflection.h"
#include "Renderer/Light.h"
#include "Renderer/Atmosphere.h"
#include "Renderer/Clouds.h"
#include "Renderer/VolumetricLight.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialInstance.h"
#include "Core/ComponentRegistry.h"
#include "Scene/Terrain/Terrain.h"
#include "Noise/NoiseGraphResource.h"
#include "Terrain/RuleGraphResource.h"
#include "Gui/Hud/HudAsset.h"

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
		// Authoritative world position. Double under VE_LARGE_WORLD_COORDINATES, so a
		// planet-scale coordinate survives storage; the render path narrows it only
		// after subtracting the frame's origin.
		VEPROPERTY(TransformComponent, ve::Vec3, position, "position", "type=drag")
		ve::Vec3 position{ 0.0 };

		// Orientation and scale stay float32: both are O(1) and carry no precision
		// risk -- only translation grows with the world.
		VEPROPERTY(TransformComponent, glm::quat, rotation, "rotation", "type=drag")
		glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

		VEPROPERTY(TransformComponent, glm::vec3, scale, "scale", "type=drag")
		glm::vec3 scale{ 1.0f };

		TransformComponent() = default;

		// Rotation * scale, in render space. The render path pairs this with the
		// authoritative position minus the frame's origin (see DrawMeshCommand), so
		// no world-magnitude translation ever reaches the GPU.
		glm::mat4 matrixRS() const {
			glm::mat4 m = glm::mat4_cast(rotation);
			m[0] *= scale.x;
			m[1] *= scale.y;
			m[2] *= scale.z;
			return m;
		}

		// Fully composed world matrix, for callers that genuinely work in absolute
		// space (picking, editor overlays). Lossy far from the origin -- not the
		// render path.
		glm::mat4 matrix() const {
			glm::mat4 m = matrixRS();
			m[3] = glm::vec4(glm::vec3(position), 1.0f);
			return m;
		}

		void serialize(JsonWriter& w) const {
			w.set("position", position);
			w.set("rotation", rotation);
			w.set("scale", scale);
		}

		void deserialize(const JsonReader& r) {
			position = ve::Vec3(r.getVec3d("position", glm::dvec3(position)));
			rotation = r.getQuat("rotation", rotation);
			scale = r.getVec3("scale", scale);
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

	// The game camera as scene data. Holds intrinsics only: the pose lives on the
	// entity's TransformComponent (view = inverse(transform)). Per-view render
	// state (TAA jitter history) stays on the runtime Camera, not here.
	VESTRUCT(CameraComponent)
	struct CameraComponent {
		VEPROPERTY(CameraComponent, float, fov, "FOV", "type=drag,minValue=1,maxValue=179")
		float fov = 45.0f;

		VEPROPERTY(CameraComponent, float, nearPlane, "Near Plane", "type=drag,minValue=0.01,maxValue=100")
		float nearPlane = 0.1f;

		VEPROPERTY(CameraComponent, float, farPlane, "Far Plane", "type=drag,minValue=1,maxValue=100000")
		float farPlane = 1200.0f;

		VEPROPERTY(CameraComponent, bool, perspective, "Perspective", "type=bool")
		bool perspective = true;

		// The view renders from the primary camera; extra cameras can coexist.
		VEPROPERTY(CameraComponent, bool, primary, "Primary", "type=bool")
		bool primary = true;

		CameraComponent() = default;

		void serialize(JsonWriter& w) const {
			w.set("fov", fov);
			w.set("near", nearPlane);
			w.set("far", farPlane);
			w.set("perspective", perspective);
			w.set("primary", primary);
		}

		void deserialize(const JsonReader& r) {
			fov = r.getFloat("fov", fov);
			nearPlane = r.getFloat("near", nearPlane);
			farPlane = r.getFloat("far", farPlane);
			perspective = r.getBool("perspective", perspective);
			primary = r.getBool("primary", primary);
		}
	};
	VECOMPONENT(CameraComponent, "Camera", "Rendering")

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

	// Optional sun god-rays. Standalone from the atmosphere: the pass reads the
	// scene's shadow cascades, which themselves only exist when an atmosphere
	// (and thus the sun) is present, so this is a no-op without one.
	VESTRUCT(VolumetricLightComponent)
	struct VolumetricLightComponent {
		VEPROPERTY(VolumetricLightComponent, bool, enabled, "Enabled", "type=bool")
		bool enabled = true;

		VEPROPERTY(VolumetricLightComponent, VolumetricLightParams, params, "params", "type=drag")
		VolumetricLightParams params;

		VolumetricLightComponent() = default;

		void serialize(JsonWriter& w) const {
			w.set("enabled", enabled);
			w.beginObject("params");
			params.serialize(w);
			w.end();
		}

		void deserialize(const JsonReader& r) {
			enabled = r.getBool("enabled", enabled);
			params.deserialize(r.child("params"));
		}
	};
	VECOMPONENT(VolumetricLightComponent, "Volumetric Light", "Environment")

	// Screen-space HUD: one authored Vellum widget tree, referenced as a
	// .veasset and drawn over the scene while a play session is live.
	VESTRUCT(HudComponent)
	struct HudComponent {
		VEPROPERTY(HudComponent, AssetHandle, hudHandle, "HUD", "type=hud")
		AssetHandle hudHandle;

		void serialize(JsonWriter& w) const {
			writeAssetPath<HudAsset>(w, "hud", hudHandle);
		}

		void deserialize(const JsonReader& r) {
			hudHandle = readAssetPath<HudAsset>(r, "hud");
		}
	};
	VECOMPONENT(HudComponent, "HUD", "UI")

	// Runtime quadtree LOD object; not serialized (rebuilt from the heightmap).
	class QuadTreeTerrain;
	// Runtime background-build queue; not serialized (see TerrainBuildQueue.h).
	class TerrainBuildQueue;

	// One owner for a batch of terrain tiles: the shared source, material and LOD
	// parameters live here once, and every tile reads them. Mirrors Unity's
	// TerrainGroup / UE's landscape info object.
	VESTRUCT(TerrainSystemComponent)
	struct TerrainSystemComponent {

		// A noise graph, baked in place at mesh-build time and shared by every tile.
		// When set it takes priority over a tile's own height map.
		VEPROPERTY(TerrainSystemComponent, AssetHandle, noiseGraphHandle, "Noise Graph", "type=noise")
		AssetHandle noiseGraphHandle;

		// Square resolution the noise graph is sampled at; sets the shared coverage
		// in chunks, not the world scale (that stays tileSize).
		VEPROPERTY(TerrainSystemComponent, int, noiseResolution, "Noise Resolution", "type=drag,speed=1,minValue=16,maxValue=4096")
		int noiseResolution = 256;

		VEPROPERTY(TerrainSystemComponent, AssetHandle, materialHandle, "Material", "type=material")
		AssetHandle materialHandle;

		// Optional rule graph that bakes how the shared material's layers are placed:
		// it reads terrain attributes (height/slope/curvature) at each texel and writes
		// the layer weights. Terrain PCG, so it sits here next to the height source
		// rather than on the material. Unset = the layers' own slope/height/noise
		// scalars drive the blend.
		VEPROPERTY(TerrainSystemComponent, AssetHandle, ruleGraphHandle, "Material Rule Graph", "type=terrain_rule")
		AssetHandle ruleGraphHandle;

		// Field units (world metres for a noise field) the globe's whole equirectangular
		// map spans -- a texel's (u, v) reads the same 2D field the chart terrain uses,
		// at globeNoiseScale * (u, v). Zones around the sphere ~= globeNoiseScale * the
		// field's dominant frequency, so this sets band density; the chart terrain
		// samples the field directly in world XZ and ignores this.
		VEPROPERTY(TerrainSystemComponent, float, globeNoiseScale, "Globe Noise Scale", "type=drag,speed=1,minValue=1,maxValue=50000")
		float globeNoiseScale = 200000.0f;

		VEPROPERTY(TerrainSystemComponent, float, tileSize, "Tile Size", "type=drag,minValue=0.1,maxValue=100.0")
		float tileSize = 1.0f;

		VEPROPERTY(TerrainSystemComponent, float, heightScale, "Height Scale", "type=drag,minValue=0.01,maxValue=100.0")
		float heightScale = 1.0f;

		// --- Quadtree LOD, shared by every tile ---
		// The deepest level samples the source at its native stride; every chunk
		// keeps `segments` divisions per edge.
		VEPROPERTY(TerrainSystemComponent, int, maxDepth, "Max LOD Depth", "type=drag,speed=1,minValue=1,maxValue=12")
		int maxDepth = 6;

		VEPROPERTY(TerrainSystemComponent, int, segments, "Chunk Segments", "type=drag,speed=1,minValue=2,maxValue=64")
		int segments = 32;

		// Subdivide a node while dist(camera, node) < nodeWorldSize * lodDetail.
		VEPROPERTY(TerrainSystemComponent, float, lodDetail, "LOD Detail", "type=drag,minValue=1,maxValue=128")
		float lodDetail = 16.0f;

		// Chunks farther than this (world units) from the camera are culled.
		VEPROPERTY(TerrainSystemComponent, float, renderDistance, "LOD Render Distance", "type=drag,minValue=100,maxValue=100000")
		float renderDistance = 10000.0f;

		// --- Distance streaming ---
		// Off: the tile set is whatever the author placed. On: the renderer keeps
		// the tiles inside streamRadiusTiles of the rendering camera's grid cell
		// alive, so the terrain follows the view. Needs a source that spans the
		// world -- the noise graph -- because a streamed tile carries no height map
		// of its own; with only per-tile height maps this does nothing.
		VEPROPERTY(TerrainSystemComponent, bool, bDistanceStreaming, "Distance Streaming", "type=bool")
		bool bDistanceStreaming = false;

		VEPROPERTY(TerrainSystemComponent, int, streamRadiusTiles, "Stream Radius", "type=drag,speed=1,minValue=0,maxValue=32")
		int streamRadiusTiles = 2;

		// Tiles kept past the radius before they are destroyed, so a camera sitting
		// on a grid boundary does not create and destroy tiles every frame.
		VEPROPERTY(TerrainSystemComponent, int, streamUnloadPadTiles, "Stream Unload Pad", "type=drag,speed=1,minValue=0,maxValue=8")
		int streamUnloadPadTiles = 1;

		// Tiles brought in per frame. Since a tile's mesh is generated off the render
		// thread, this is now how fast the window grows, not how hard it hitches.
		VEPROPERTY(TerrainSystemComponent, int, streamBuildBudget, "Stream Build Budget", "type=drag,speed=1,minValue=1,maxValue=16")
		int streamBuildBudget = 1;

		// Painted control maps, persisted as a "<scene>_terrain.veasset" sidecar;
		// the handle keeps the weight buffers out of the .veworld.
		AssetHandle terrainDataHandle;
		// Runtime cache of the above; never serialized.
		Ref<TerrainDataResource> terrainData;

		// Background tile builds, created on the first async build; never serialized.
		Ref<TerrainBuildQueue> builds;

		TerrainSystemComponent() = default;

		void serialize(JsonWriter& w) const {
			writeAssetPath<NoiseGraphResource>(w, "noiseGraph", noiseGraphHandle);
			writeAssetPath<Material>(w, "material", materialHandle);
			writeAssetPath<TerrainRuleGraphResource>(w, "ruleGraph", ruleGraphHandle);
			writeAssetPath<TerrainDataResource>(w, "terrainData", terrainDataHandle);

			w.set("noiseResolution", noiseResolution);
			w.set("globeNoiseScale", globeNoiseScale);
			w.set("tileSize", tileSize);
			w.set("heightScale", heightScale);
			w.set("maxDepth", maxDepth);
			w.set("segments", segments);
			w.set("lodDetail", lodDetail);
			w.set("renderDistance", renderDistance);
			w.set("distanceStreaming", bDistanceStreaming);
			w.set("streamRadiusTiles", streamRadiusTiles);
			w.set("streamUnloadPadTiles", streamUnloadPadTiles);
			w.set("streamBuildBudget", streamBuildBudget);
		}

		void deserialize(const JsonReader& r) {
			noiseGraphHandle = readAssetPath<NoiseGraphResource>(r, "noiseGraph");
			materialHandle = readAssetPath<Material>(r, "material");
			ruleGraphHandle = readAssetPath<TerrainRuleGraphResource>(r, "ruleGraph");
			terrainDataHandle = readAssetPath<TerrainDataResource>(r, "terrainData");

			noiseResolution = r.getInt("noiseResolution", noiseResolution);
			globeNoiseScale = r.getFloat("globeNoiseScale", globeNoiseScale);
			tileSize = r.getFloat("tileSize", tileSize);
			heightScale = r.getFloat("heightScale", heightScale);
			maxDepth = r.getInt("maxDepth", maxDepth);
			segments = r.getInt("segments", segments);
			lodDetail = r.getFloat("lodDetail", lodDetail);
			renderDistance = r.getFloat("renderDistance", renderDistance);
			bDistanceStreaming = r.getBool("distanceStreaming", bDistanceStreaming);
			streamRadiusTiles = r.getInt("streamRadiusTiles", streamRadiusTiles);
			streamUnloadPadTiles = r.getInt("streamUnloadPadTiles", streamUnloadPadTiles);
			streamBuildBudget = r.getInt("streamBuildBudget", streamBuildBudget);
		}

	};
	VECOMPONENT(TerrainSystemComponent, "TerrainSystem", "Environment")

	VESTRUCT(TerrainComponent)
	struct TerrainComponent {

		// Optional per-tile height map. The group's shared noise graph wins when it
		// has one; otherwise each tile bakes its own from this.
		VEPROPERTY(TerrainComponent, AssetHandle, heightMapHandle, "Height Map", "type=texture")
		AssetHandle heightMapHandle;

		// This tile's place in the terrain grid, and its only identity.
		VEPROPERTY(TerrainComponent, glm::ivec2, tileCoord, "Tile Coord", "type=drag,speed=1")
		glm::ivec2 tileCoord = glm::ivec2(0);

		// Generated mesh handle — registered with ResourceManager on build
		AssetHandle generatedMeshHandle;
		// Runtime quadtree LOD object owning the shared mesh + per-chunk ranges.
		// Null until the Generate button builds it; never serialized.
		Ref<QuadTreeTerrain> quadtree;

		// Per-instance material view carrying this terrain's control map. Runtime
		// only — rebuilt by TerrainTile::ensureMaterialInstance; the system's material
		// stays the authored reference, materialInstanceBase is what the view was
		// last built from.
		AssetHandle materialInstanceHandle;
		AssetHandle materialInstanceBase;
		Ref<MaterialInstance> materialInstance;

		// --- Material paint brush ---
		// CPU mirror of the height field the mesh was built from, in raw
		// normalized units (world Y = value * heightScale). Rebuilt alongside the
		// mesh so the brush can raycast against the surface it is drawing on.
		std::vector<float> heightField;
		int heightRes = 0;

		// Painted weights live in the system's TerrainDataResource (keyed by this
		// tile's coordinate); the tile keeps only the GPU map uploaded from it.
		AssetHandle weightMapHandle;
		// Set when the brush mutates this tile's weights; uploads then clears it.
		bool bWeightsDirty = false;

		// Key of the inputs the tile's weights were last baked from: the height
		// field, the rule graph (if any) and the layers' scalar rules. Runtime
		// only: 0 means "no bake yet"; a mismatch triggers a re-bake. The bake
		// overwrites the whole map, so it also wipes painted coverage.
		uint64_t weightBakeKey = 0;

		bool bDirty = true;
		// Mesh-build key; the mesh is rebuilt when the live inputs hash differently.
		uint64_t builtKey = 0;

		// Set on tiles the streamer created because a viewpoint asked for them.
		// Runtime-only, and never serialized: it is what keeps a streamed window
		// out of the saved scene, and out of a play session's cloned scene.
		bool bStreamed = false;

		TerrainComponent() = default;

		void serialize(JsonWriter& w) const {
			writeAssetPath<Texture2D>(w, "heightMap", heightMapHandle);

			w.set("tileX", tileCoord.x);
			w.set("tileZ", tileCoord.y);
		}

		void deserialize(const JsonReader& r) {
			heightMapHandle = readAssetPath<Texture2D>(r, "heightMap");

			tileCoord.x = r.getInt("tileX", 0);
			tileCoord.y = r.getInt("tileZ", 0);

			bDirty = true;
		}

	};
	VECOMPONENT(TerrainComponent, "Terrain", "Environment")

	// Only marks dirty: the scene's TerrainSystem owns the shared source and LOD
	// parameters, and it may not exist yet (or may load after this tile). The
	// renderer builds lazily once the scene has settled.
	VECOMPONENTINIT(TerrainComponent, {
		reg.get<TerrainComponent>(e).bDirty = true;
	})


}