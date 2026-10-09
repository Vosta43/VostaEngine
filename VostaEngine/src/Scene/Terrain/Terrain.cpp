#include "vepch.h"
#include "Terrain.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Scene/Terrain/QuadTreeTerrain.h"
#include "Scene/Terrain/TerrainMeshBuilder.h"
#include "Scene/Terrain/TerrainDataResource.h"
#include "Asset/BuiltinReousrces.h"
#include "Asset/TextureImporter.h"
#include "Asset/TextureResource.h"
#include "Asset/Utils.h"
#include "Core/AssetConfig.h"
#include "Core/Hash.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Core/JobSystem.h"
#include "Noise/NoiseGraphResource.h"
#include "Noise/NoisePlan.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialInstance.h"
#include "Renderer/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ve {

	namespace {

		// Extent/segments of the starting flat plane. Only used until a heightmap
		// takes over, which is why a fixed size is fine.
		constexpr float kFlatWorldSize = 100.0f;
		constexpr int   kFlatSegments = 32;

		// Synthetic resource path for a terrain's generated mesh. One mesh per
		// entity, keyed by id so regeneration replaces it in place.
		std::string terrainMeshPath(uint32_t entityId) {
			return "__terrain/" + std::to_string(entityId);
		}

		void registerMesh(TerrainComponent& comp, uint32_t entityId, const Ref<StaticMesh>& mesh) {
			auto& storage = ResourceManager::getStorage<StaticMesh>();
			if (comp.generatedMeshHandle.isValid())
				storage.remove(comp.generatedMeshHandle);
			comp.generatedMeshHandle = storage.store(terrainMeshPath(entityId), mesh);
		}

		enum class TerrainSourceKind : uint32_t { Flat, HeightMap, Noise };

		// Every input that changes the baked mesh, as a caller-defined struct so the
		// key and the builder read the same fields. Fields unused by a source stay
		// zero, so e.g. noiseResolution never invalidates a heightmap tile.
		struct TerrainBuildInputs {
			TerrainSourceKind sourceKind = TerrainSourceKind::Flat;
			uint64_t sourceId = 0;   // noise content hash, or heightmap handle identity
			int noiseResolution = 0;
			float tileSize = 0.0f;
			float heightScale = 0.0f;
			int maxDepth = 0;
			int segments = 0;
			glm::ivec2 tileCoord = glm::ivec2(0);

			uint64_t key() const {
				uint64_t h = hashString("terrain");
				h = hashValue(h, static_cast<uint32_t>(sourceKind));
				h = hashValue(h, sourceId);
				h = hashValue(h, noiseResolution);
				h = hashValue(h, tileSize);
				h = hashValue(h, heightScale);
				h = hashValue(h, maxDepth);
				h = hashValue(h, segments);
				h = hashValue(h, tileCoord.x);
				h = hashValue(h, tileCoord.y);
				return h;
			}
		};

		TerrainBuildInputs makeBuildInputs(const TerrainComponent& comp, const TerrainSystemComponent& sys) {
			TerrainBuildInputs in;
			in.tileSize = sys.tileSize;
			in.heightScale = sys.heightScale;
			in.maxDepth = sys.maxDepth;
			in.segments = sys.segments;
			in.tileCoord = comp.tileCoord;

			if (sys.noiseGraphHandle.isValid()) {
				in.sourceKind = TerrainSourceKind::Noise;
				if (auto res = ResourceManager::get<NoiseGraphResource>(sys.noiseGraphHandle))
					in.sourceId = res->contentHash;
				in.noiseResolution = std::clamp(sys.noiseResolution, 16, 4096);
			}
			else if (comp.heightMapHandle.isValid()) {
				in.sourceKind = TerrainSourceKind::HeightMap;
				in.sourceId = hashValue(
					hashValue(0, comp.heightMapHandle.index()), comp.heightMapHandle.generation());
			}
			return in;
		}

		int channelCount(TextureFormat format) {
			switch (format) {
				case TextureFormat::R8:
				case TextureFormat::R16F:    return 1;
				case TextureFormat::RG16F:   return 2;
				case TextureFormat::RGB:
				case TextureFormat::RGB16F:  return 3;
				case TextureFormat::RGBA:
				case TextureFormat::RGBA16F: return 4;
				default:                     return 3;
			}
		}

		// Mirror the mesh builder's height extraction into a CPU grid so the brush
		// can raycast the drawn surface. Values are raw (world Y = value *
		// heightScale), indexed [x * res + z] exactly like the builder.
		void populateHeightField(TerrainComponent& comp, const Ref<TextureResource>& tex) {
			const int w = (int)tex->width;
			const int h = (int)tex->height;
			if (w <= 0 || h <= 0) return;

			// The grid is square-shaped everywhere downstream: the brush samples it
			// with a single heightRes stride and the control map maps it with one
			// scalar extent. A rectangular field would index past the end of that
			// stride, so refuse it and leave the terrain unsampled (it still draws).
			if (w != h) {
				VE_CORE_ERROR_PRINT(
					"Terrain: height field must be square, got %dx%d; brush disabled", w, h);
				comp.heightField.clear();
				comp.heightRes = 0;
				return;
			}

			const int channels = channelCount(tex->format);
			const bool useFloat = !tex->floatPixels.empty();

			comp.heightField.assign((size_t)w * h, 0.0f);
			for (int i = 0; i < w; ++i) {
				for (int j = 0; j < h; ++j) {
					const size_t src = (size_t)(i * h + j) * channels;
					comp.heightField[(size_t)i * h + j] = useFloat
						? tex->floatPixels[src]
						: tex->pixels[src] / 255.0f;
				}
			}
			// Square in practice; the raycast samples a square grid.
			comp.heightRes = w;
		}

		// Bilinear height at a terrain-local XZ position, in world units. The tile
		// grid spacing and vertical scale come from the system, so they are passed
		// in rather than read off the (now slimmer) tile.
		float sampleSurfaceLocal(const TerrainComponent& comp, float tileSize, float heightScale,
			float lx, float lz) {
			const int n = comp.heightRes;
			const float ts = tileSize > 0.0f ? tileSize : 1.0f;

			float fx = std::clamp(lx / ts, 0.0f, (float)(n - 1));
			float fz = std::clamp(lz / ts, 0.0f, (float)(n - 1));

			const int x0 = (int)fx, z0 = (int)fz;
			const int x1 = std::min(x0 + 1, n - 1);
			const int z1 = std::min(z0 + 1, n - 1);
			const float tx = fx - x0, tz = fz - z0;

			auto H = [&](int i, int j) { return comp.heightField[(size_t)i * n + j]; };
			const float h0 = glm::mix(H(x0, z0), H(x1, z0), tx);
			const float h1 = glm::mix(H(x0, z1), H(x1, z1), tx);
			return glm::mix(h0, h1, tz) * heightScale;
		}

		// Throw away this terrain's painted control map. Called whenever the height
		// grid underneath it is replaced, where the old weights would otherwise be
		// stretched across a differently-sized world instead of being redrawn.
		void invalidateWeightMap(Scene& scene, TerrainComponent& comp) {
			if (comp.weightMapHandle.isValid()) {
				ResourceManager::getStorage<Texture2D>().remove(comp.weightMapHandle);
				comp.weightMapHandle = INVALID_ASSET_HANDLE;
			}
			// Drop the CPU buffer too, so the packed weights die with the GPU map.
			if (auto data = Terrain::findTerrainData(scene))
				data->erase(comp.tileCoord);
			comp.bWeightsDirty = false;

			// The base's own defaults (1x1 white map, invSize 0) have to stand again.
			// ensureMaterialInstance repopulates the overrides next frame, so dropping
			// them here costs nothing — but leaving them would point the shader at a
			// texture that is no longer registered.
			if (comp.materialInstance)
				comp.materialInstance->clearOverrides();
		}

		// One tile entity at coord, in the state addTile leaves it: placed and
		// marked dirty so the renderer bakes it lazily.
		uint32_t createTileAt(Scene& scene, const glm::ivec2& coord) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Terrain");
			scene.assignComponent<TransformComponent>(entity, glm::mat4(1.0f));
			TerrainComponent& comp = scene.assignComponent<TerrainComponent>(entity);

			// Nothing to copy from a template: the shared source and material live
			// on the system, so a new tile only needs its coordinate.
			comp.tileCoord = coord;
			comp.bDirty = true;
			Terrain::placeTile(scene, entity.m_id);
			return entity.m_id;
		}

		// Drop the resources a tile owns, then the entity. They are keyed by entity
		// id, so nothing else reclaims them; a destroyed tile would otherwise leave
		// its mesh, control map and material instance alive in the manager. Its
		// painted weights go too, so a tile removed and re-added starts clean.
		void destroyTile(Scene& scene, uint32_t id) {
			const TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);
			if (auto data = Terrain::findTerrainData(scene))
				data->erase(comp.tileCoord);
			ResourceManager::getStorage<StaticMesh>().remove(comp.generatedMeshHandle);
			ResourceManager::getStorage<Texture2D>().remove(comp.weightMapHandle);
			ResourceManager::getStorage<Material>().remove(comp.materialInstanceHandle);
			scene.destroyEntity(scene.getEntity(id));
		}

		// Packed grid coordinate, shared with TerrainDataResource so tile keys line
		// up between the resource and the occupancy sets here.
		int64_t packCoord(const glm::ivec2& coord) {
			return TerrainDataResource::pack(coord);
		}

		// Build this tile's control-map texture from the weights in the scene's
		// terrain data. Returns INVALID when there is no height field or no weights
		// for this tile yet, which keeps the mask lazily disengaged when unpainted.
		AssetHandle ensureWeightMap(Scene& scene, TerrainComponent& comp, uint32_t entityId,
			float tileSize) {

			if (comp.weightMapHandle.isValid())
				return comp.weightMapHandle;
			// Nothing to paint over without a height field; the mesh would not have a
			// defined extent for the control map to line up with.
			if (comp.heightRes <= 0 || tileSize <= 0.0f)
				return INVALID_ASSET_HANDLE;

			auto data = Terrain::findTerrainData(scene);
			if (!data)
				return INVALID_ASSET_HANDLE;

			TerrainDataTile* tile = data->find(comp.tileCoord);
			if (!tile || tile->weightRes <= 0 || tile->weightData.empty())
				return INVALID_ASSET_HANDLE;

			// Guard the upload against a truncated/corrupt data file.
			const size_t expected = (size_t)tile->weightRes * tile->weightRes * 3;
			if (tile->weightData.size() != expected)
				return INVALID_ASSET_HANDLE;

			auto tex = Texture2D::create((uint32_t)tile->weightRes, (uint32_t)tile->weightRes,
				TextureFormat::RGB);
			if (!tex)
				return INVALID_ASSET_HANDLE;
			tex->setData(tile->weightData.data(), (uint32_t)tile->weightData.size());

			auto& storage = ResourceManager::getStorage<Texture2D>();
			const std::string key = "__terrain_weights_" + std::to_string(entityId);
			storage.remove(key);
			comp.weightMapHandle = storage.store(key, tex);
			comp.bWeightsDirty = false;

			return comp.weightMapHandle;
		}

	}

	TerrainSystemComponent* Terrain::findSystem(Scene& scene) {
		for (uint32_t id : scene.getRegistry().view<TerrainSystemComponent>())
			return &scene.getRegistry().get<TerrainSystemComponent>(id);
		return nullptr;
	}

	uint32_t Terrain::ensureSystem(Scene& scene) {
		for (uint32_t id : scene.getRegistry().view<TerrainSystemComponent>())
			return id;

		Entity entity = scene.createEntity();
		scene.assignComponent<NameComponent>(entity, "TerrainSystem");
		scene.assignComponent<TerrainSystemComponent>(entity);
		return entity.m_id;
	}

	Ref<TerrainDataResource> Terrain::findTerrainData(Scene& scene) {
		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return nullptr;

		// Resolve from the handle when the scene has one, else keep the in-memory
		// resource a paint session already built.
		if (sys->terrainDataHandle.isValid())
			sys->terrainData = ResourceManager::get<TerrainDataResource>(sys->terrainDataHandle);

		return sys->terrainData;
	}

	Ref<TerrainDataResource> Terrain::ensureTerrainData(Scene& scene) {
		Ref<TerrainDataResource> data = findTerrainData(scene);
		if (data)
			return data;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return nullptr;

		sys->terrainData = CreateRef<TerrainDataResource>();
		return sys->terrainData;
	}

	float Terrain::tileWorldSize(const TerrainComponent& comp, const TerrainSystemComponent& sys) {
		// Before a bake the height field is empty, so fall back to the resolution the
		// noise graph will be sampled at; otherwise a tile that has a source but has
		// not been generated yet reports no extent and cannot be placed or snapped.
		int res = comp.heightRes;
		if (res <= 1 && sys.noiseGraphHandle.isValid())
			res = std::clamp(sys.noiseResolution, 16, 4096);
		return res > 1 ? (float)(res - 1) * sys.tileSize : 0.0f;
	}

	glm::vec2 Terrain::tileOrigin(const TerrainComponent& comp, const TerrainSystemComponent& sys) {
		return glm::vec2(comp.tileCoord) * tileWorldSize(comp, sys);
	}

	AssetHandle Terrain::ensureMaterialInstance(Scene& scene, uint32_t entity,
		const glm::vec2& worldOrigin) {

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return INVALID_ASSET_HANDLE;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		// The system is the authored material reference. Rather than writing a
		// default back into it from a render call, fall back to the built-in one
		// locally so an unpainted scene still draws.
		const AssetHandle baseHandle = sys->materialHandle.isValid()
			? sys->materialHandle : BuiltinResources::getDefaultMaterial();

		// Rebuild only when the base material changed; the control-map bindings
		// below are refreshed on every call so they track a weight map that
		// appears later and a transform that moves.
		const bool upToDate = comp.materialInstanceHandle.isValid() &&
			comp.materialInstanceBase == baseHandle;

		if (!upToDate) {
			auto base = ResourceManager::get<Material>(baseHandle);
			if (!base)
				return INVALID_ASSET_HANDLE;

			// Reuse the same object across rebases so its registered handle stays valid.
			if (!comp.materialInstance)
				comp.materialInstance = CreateRef<MaterialInstance>();

			comp.materialInstance->base = base;
			comp.materialInstance->clearOverrides();

			// Key by entity id, dropping any entry a previous life of this id left
			// behind — store() dedups by path and would otherwise hand back a stale
			// resource that is not this component's instance.
			auto& storage = ResourceManager::getStorage<Material>();
			const std::string key = "__terrain_matinst_" + std::to_string(entity);
			storage.remove(key);
			comp.materialInstanceHandle = storage.store(key, comp.materialInstance);
			comp.materialInstanceBase = baseHandle;
		}

		// Per-instance overrides, refreshed on every call: they have to track a
		// terrain that moves, a Height Scale that changes, and a weight map that
		// only appears once the user paints.
		if (comp.materialInstance) {
			// The layer height rules map world Y into 0..1 by dividing by
			// u_HeightScale. The system owns that value — it is the same one the
			// mesh was built with — so the bands cannot drift away from the surface.
			if (sys->heightScale > 0.0f)
				comp.materialInstance->setFloat("u_HeightScale", sys->heightScale);

			// Control map. Engaged only once the tile has weights (painted this
			// session or loaded from a data file); until then the base's 1x1 white
			// default stands in, so a terrain nobody painted keeps its layers.
			const float span = tileWorldSize(comp, *sys);
			AssetHandle weights = ensureWeightMap(scene, comp, entity, sys->tileSize);
			if (weights.isValid() && span > 0.0f) {
				comp.materialInstance->setTexture("u_Weights", weights, 8);
				comp.materialInstance->setVec2("u_TerrainOrigin", worldOrigin);
				comp.materialInstance->setFloat("u_TerrainInvSize", 1.0f / span);
			}
		}

		return comp.materialInstanceHandle;
	}

	bool Terrain::flushWeightMap(Scene& scene, uint32_t entity) {

		if (!scene.getRegistry().has<TerrainComponent>(entity))
			return false;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (!comp.bWeightsDirty || !comp.weightMapHandle.isValid())
			return false;

		auto data = findTerrainData(scene);
		if (!data)
			return false;
		TerrainDataTile* tile = data->find(comp.tileCoord);
		if (!tile || tile->weightData.empty())
			return false;

		auto tex = ResourceManager::get<Texture2D>(comp.weightMapHandle);
		if (!tex)
			return false;

		tex->setData(tile->weightData.data(), (uint32_t)tile->weightData.size());
		comp.bWeightsDirty = false;
		return true;
	}

	bool Terrain::raycast(Scene& scene, const TerrainComponent& comp, const glm::vec3& origin,
		const glm::vec3& dir, float maxDist, glm::vec3& outPoint) {

		if (comp.heightRes <= 0 || comp.heightField.empty())
			return false;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return false;

		const float dLen = glm::length(dir);
		if (dLen < 1e-6f)
			return false;
		const glm::vec3 d = dir / dLen;

		const float worldSize = tileWorldSize(comp, *sys);
		if (worldSize <= 0.0f)
			return false;

		// Clip the ray to the terrain's XZ footprint so the march stays bounded.
		float tLo = 0.0f, tHi = maxDist / dLen;
		auto slab = [&](float o, float dd) -> bool {
			if (std::abs(dd) < 1e-6f)
				return o >= 0.0f && o <= worldSize;
			float a = (0.0f - o) / dd;
			float b = (worldSize - o) / dd;
			if (a > b) std::swap(a, b);
			tLo = std::max(tLo, a);
			tHi = std::min(tHi, b);
			return tLo <= tHi;
		};
		if (!slab(origin.x, d.x) || !slab(origin.z, d.z) || tHi < 0.0f)
			return false;
		tLo = std::max(tLo, 0.0f);

		const float step = std::max(sys->tileSize, 0.05f);

		float t = tLo;
		glm::vec3 p = origin + d * t;

		// A ray that starts under the surface has no crossing to find here.
		if (p.y - sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, p.x, p.z) <= 0.0f)
			return false;

		for (int guard = 0; t < tHi && guard < 100000; ++guard) {
			const float nt = std::min(t + step, tHi);
			glm::vec3 np = origin + d * nt;
			const float cur = np.y -
				sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, np.x, np.z);

			if (cur <= 0.0f) {
				// Bracketed: bisect to the surface for a stable hit.
				float a = t, b = nt;
				for (int k = 0; k < 24; ++k) {
					const float m = 0.5f * (a + b);
					glm::vec3 pm = origin + d * m;
					const float dm = pm.y -
						sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, pm.x, pm.z);
					if (dm > 0.0f) a = m; else b = m;
				}
				outPoint = origin + d * (0.5f * (a + b));
				return true;
			}

			t = nt;
		}

		return false;
	}

	bool Terrain::raycastScene(Scene& scene, const glm::vec3& origin, const glm::vec3& dir,
		float maxDist, uint32_t& outEntity, glm::vec3& outWorldPoint) {

		if (!findSystem(scene))
			return false;

		bool hit = false;
		float bestDist = 1e30f;

		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			const TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);
			if (comp.heightRes <= 0 || comp.heightField.empty())
				continue;

			// Terrain-local ray: the tile transform is translation-only, as the
			// control-map mapping and the editor brush both assume.
			const glm::vec3 t(scene.getComponent<TransformComponent>(id).transform[3]);
			const glm::vec3 localO = origin - glm::vec3(t.x, 0.0f, t.z);

			glm::vec3 localP;
			if (!raycast(scene, comp, localO, dir, maxDist, localP))
				continue;

			const float d = glm::length(localP - localO);
			if (d < bestDist) {
				bestDist = d;
				outEntity = id;
				outWorldPoint = localP + glm::vec3(t.x, 0.0f, t.z);
			}
			hit = true;
		}

		return hit;
	}

	glm::vec3 Terrain::surfaceNormal(Scene& scene, const TerrainComponent& comp,
		float localX, float localZ) {

		if (comp.heightRes <= 0 || comp.heightField.empty())
			return glm::vec3(0.0f, 1.0f, 0.0f);

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return glm::vec3(0.0f, 1.0f, 0.0f);

		const float e = sys->tileSize > 0.0f ? sys->tileSize : 1.0f;
		const float hL = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX - e, localZ);
		const float hR = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX + e, localZ);
		const float hD = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX, localZ - e);
		const float hU = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX, localZ + e);

		return glm::normalize(glm::vec3(hL - hR, 2.0f * e, hD - hU));
	}

	void Terrain::paintWeight(Scene& scene, uint32_t entity, const glm::vec2& localXZ,
		int layerIndex, float radius, float strength) {

		if (layerIndex < 1 || layerIndex > 3 || radius <= 0.0f)
			return;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		// Nothing to paint over without a height field; bail before allocating a
		// control buffer the tile could never show.
		if (comp.heightRes <= 0 || sys->tileSize <= 0.0f)
			return;

		// The resource owns the weights; create this tile's buffer on first use so
		// ensureWeightMap has something to build the control map from.
		auto data = ensureTerrainData(scene);
		if (!data)
			return;

		TerrainDataTile& tile = data->tile(comp.tileCoord);
		if (tile.weightRes <= 0)
			tile.weightRes = 512;

		const int res = tile.weightRes;
		if (tile.weightData.empty())
			tile.weightData.assign((size_t)res * res * 3, 0);

		// Now the buffer exists, ensureWeightMap can build the control map from it.
		AssetHandle weights = ensureWeightMap(scene, comp, entity, sys->tileSize);
		if (!weights.isValid())
			return;

		const float worldSize = tileWorldSize(comp, *sys);
		if (res <= 0 || worldSize <= 0.0f)
			return;

		const float texPerUnit = (float)res / worldSize;
		// The brush is a world-space ball centred on the surface. The metric below
		// measures each texel's own surface point against that centre, so a stroke
		// reads as a round decal no matter how steep the ground under it is.
		const float centerH = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale,
			localXZ.x, localXZ.y);
		const glm::vec3 center(localXZ.x, centerH, localXZ.y);
		const float rTex = std::max(radius * texPerUnit, 0.5f);

		const int x0 = std::max(0, (int)std::floor(center.x * texPerUnit - rTex));
		const int x1 = std::min(res - 1, (int)std::ceil(center.x * texPerUnit + rTex));
		const int z0 = std::max(0, (int)std::floor(center.z * texPerUnit - rTex));
		const int z1 = std::min(res - 1, (int)std::ceil(center.z * texPerUnit + rTex));

		const int ch = layerIndex - 1;
		const float cap = std::clamp(strength, 0.0f, 1.0f) * 255.0f;
		const float invR = 1.0f / radius;

		for (int z = z0; z <= z1; ++z) {
			for (int x = x0; x <= x1; ++x) {
				const float lx = (x + 0.5f) / texPerUnit;
				const float lz = (z + 0.5f) / texPerUnit;
				const glm::vec3 p(lx,
					sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, lx, lz), lz);
				const float dd = glm::length(p - center) * invR;
				if (dd >= 1.0f)
					continue;

				// White centre fading to black at the rim, squared for a softer foot.
				const float f = 1.0f - dd;
				const uint8_t target = (uint8_t)std::lround(f * f * cap);

				uint8_t& slot = tile.weightData[((size_t)z * res + x) * 3 + ch];
				if (target > slot)
					slot = target;
			}
		}

		comp.bWeightsDirty = true;
		data->bDirty = true;
	}

	void Terrain::paintWeightWorld(Scene& scene, uint32_t referenceTile, const glm::vec3& worldPoint,
		int layerIndex, float radius, float strength) {

		if (layerIndex < 1 || layerIndex > 3 || radius <= 0.0f)
			return;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys || referenceTile == UINT32_MAX ||
			!scene.getRegistry().has<TerrainComponent>(referenceTile))
			return;

		const float worldSize = tileWorldSize(scene.getComponent<TerrainComponent>(referenceTile), *sys);
		if (worldSize <= 0.0f)
			return;

		// Occupancy keyed by tile coordinate, so each candidate cell is one lookup
		// instead of a findTile scan per cell.
		std::unordered_map<int64_t, uint32_t> byCoord;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>())
			byCoord.emplace(packCoord(scene.getComponent<TerrainComponent>(id).tileCoord), id);

		// Every tile the brush ball can touch. Each tile maps its own local (0,0) to
		// a tileWorldSize multiple, so their control-map texels are already aligned
		// in world space; passing the same world point into each tile makes the mask
		// agree across the boundary rather than restarting at every tile edge.
		const glm::vec2 wp(worldPoint.x, worldPoint.z);
		const glm::vec2 loF = (wp - glm::vec2(radius)) / worldSize;
		const glm::vec2 hiF = (wp + glm::vec2(radius)) / worldSize;
		const glm::ivec2 lo((int)std::floor(loF.x), (int)std::floor(loF.y));
		const glm::ivec2 hi((int)std::floor(hiF.x), (int)std::floor(hiF.y));

		for (int y = lo.y; y <= hi.y; ++y) {
			for (int x = lo.x; x <= hi.x; ++x) {
				auto it = byCoord.find(packCoord(glm::ivec2(x, y)));
				if (it == byCoord.end())
					continue;

				const uint32_t tile = it->second;
				const glm::vec2 local = wp - tileOrigin(scene.getComponent<TerrainComponent>(tile), *sys);
				paintWeight(scene, tile, local, layerIndex, radius, strength);
			}
		}
	}

	void Terrain::fillWeight(Scene& scene, int layerIndex) {

		if (layerIndex < 1 || layerIndex > 3)
			return;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return;

		auto data = ensureTerrainData(scene);
		if (!data)
			return;

		const int ch = layerIndex - 1;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);

			// Skip tiles with nothing to paint over (the flat placeholder).
			if (comp.heightRes <= 0)
				continue;

			TerrainDataTile& tile = data->tile(comp.tileCoord);
			if (tile.weightRes <= 0)
				tile.weightRes = 512;
			const int res = tile.weightRes;
			if (tile.weightData.empty())
				tile.weightData.assign((size_t)res * res * 3, 0);

			AssetHandle weights = ensureWeightMap(scene, comp, id, sys->tileSize);
			if (!weights.isValid())
				continue;

			for (size_t i = (size_t)ch; i < tile.weightData.size(); i += 3)
				tile.weightData[i] = 255;

			comp.bWeightsDirty = true;
		}

		data->bDirty = true;
	}

	void Terrain::clearWeight(Scene& scene, int layerIndex) {

		if (layerIndex < 1 || layerIndex > 3)
			return;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return;

		auto data = ensureTerrainData(scene);
		if (!data)
			return;

		const int ch = layerIndex - 1;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);

			// Skip tiles with nothing to paint over (the flat placeholder).
			if (comp.heightRes <= 0)
				continue;

			TerrainDataTile& tile = data->tile(comp.tileCoord);
			if (tile.weightRes <= 0)
				tile.weightRes = 512;
			const int res = tile.weightRes;
			if (tile.weightData.empty())
				tile.weightData.assign((size_t)res * res * 3, 0);

			AssetHandle weights = ensureWeightMap(scene, comp, id, sys->tileSize);
			if (!weights.isValid())
				continue;

			for (size_t i = (size_t)ch; i < tile.weightData.size(); i += 3)
				tile.weightData[i] = 0;

			comp.bWeightsDirty = true;
		}

		data->bDirty = true;
	}

	void Terrain::ensureBuilt(Scene& scene, uint32_t entity) {

		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		// No system means no shared source or material, so the tile stays invisible
		// instead of drawing a placeholder with nothing to shade it.
		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		const uint64_t key = makeBuildInputs(comp, *sys).key();
		// Already drawable and built from the current inputs.
		if (comp.generatedMeshHandle.isValid() && !comp.bDirty && comp.builtKey == key)
			return;

		// Noise graph wins when present, then heightmap, then the flat plane.
		if (sys->noiseGraphHandle.isValid()) {
			rebuildFromNoiseGraph(scene, entity);
			return;
		}

		if (comp.heightMapHandle.isValid()) {
			rebuildFromHeightMap(scene, entity);
			return;
		}

		registerMesh(comp, entity,
			TerrainMeshBuilder::buildFlatMesh(kFlatWorldSize, kFlatSegments));
		// A flat placeholder has no sampled height field: nothing for the brush to
		// raycast, and no extent for a control map to line up with.
		comp.heightField.clear();
		comp.heightRes = 0;
		invalidateWeightMap(scene, comp);
		comp.builtKey = key;
		comp.bDirty = false;
	}

	void Terrain::rebuildFromHeightMap(Scene& scene, uint32_t entity) {

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (!comp.heightMapHandle.isValid())
			return;

		const std::string heightMapPath =
			toAbsolute(ResourceManager::getPath<Texture2D>(comp.heightMapHandle));
		if (heightMapPath.empty())
			return;

		// The heightmap may be a raw image or an already-baked .veasset texture
		// (e.g. one exported from the noise editor), so dispatch on the extension
		// exactly like Texture2D::create does rather than assuming a source image.
		auto textureResource = utils::isEngineAsset(heightMapPath)
			? TextureImporter::deserialize(heightMapPath)
			: TextureImporter::importFromFile(heightMapPath);
		if (!textureResource) {
			VE_CORE_ERROR_PRINT("Terrain: failed to load heightmap %s", heightMapPath.c_str());
			return;
		}

		const int prevRes = comp.heightRes;
		populateHeightField(comp, textureResource);
		// A *change* of grid resolution moves the world span the painted texels
		// cover, so the control map must be redrawn. prevRes == 0 is the first bake
		// (fresh tile or just-loaded scene), not a change -- loaded weights survive.
		if (prevRes > 0 && comp.heightRes != prevRes)
			invalidateWeightMap(scene, comp);

		if (!comp.quadtree)
			comp.quadtree = CreateRef<QuadTreeTerrain>();

		auto mesh = comp.quadtree->build(textureResource, sys->tileSize, sys->heightScale,
			sys->maxDepth, sys->segments, tileOrigin(comp, *sys));
		if (!mesh) {
			VE_CORE_ERROR_PRINT("Terrain: mesh generation failed");
			return;
		}

		registerMesh(comp, entity, mesh);
		comp.builtKey = makeBuildInputs(comp, *sys).key();
		comp.bDirty = false;
		placeTile(scene, entity);
	}

	void Terrain::rebuildFromNoiseGraph(Scene& scene, uint32_t entity) {

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (!sys->noiseGraphHandle.isValid())
			return;

		auto resource = ResourceManager::get<NoiseGraphResource>(sys->noiseGraphHandle);
		if (!resource) {
			VE_CORE_ERROR_PRINT("Terrain: failed to load noise graph");
			return;
		}

		const NoisePlan plan = buildNoisePlan(resource->graph);
		if (!plan.valid()) {
			VE_CORE_ERROR_PRINT("Terrain: noise graph has no Output");
			return;
		}

		const int n = std::clamp(sys->noiseResolution, 16, 4096);

		auto tex = CreateRef<TextureResource>();
		tex->width = (uint32_t)n;
		tex->height = (uint32_t)n;
		tex->format = TextureFormat::R16F;
		tex->floatPixels.assign((size_t)n * n, 0.0f);

		// tileOrigin reads heightRes, which this bake has not set yet, so derive the
		// origin from the resolution being baked or every tile samples from (0,0).
		const glm::vec2 origin = glm::vec2(comp.tileCoord) * ((float)(n - 1) * sys->tileSize);
		const float tileSize = sys->tileSize;
		JobSystem::get().parallelFor(0, n, [&](int i) {
			thread_local NoisePlanScratch scratch;
			for (int j = 0; j < n; ++j)
				tex->floatPixels[(size_t)i * n + j] =
					plan.evaluate(scratch, origin.x + (float)i * tileSize,
						origin.y + (float)j * tileSize);
		}).get();

		const int prevRes = comp.heightRes;
		populateHeightField(comp, tex);
		// See the heightmap path: only a resolution *change* invalidates, so the
		// first bake after load keeps the loaded weights.
		if (prevRes > 0 && comp.heightRes != prevRes)
			invalidateWeightMap(scene, comp);

		if (!comp.quadtree)
			comp.quadtree = CreateRef<QuadTreeTerrain>();

		auto mesh = comp.quadtree->build(tex, sys->tileSize, sys->heightScale,
			sys->maxDepth, sys->segments, tileOrigin(comp, *sys));
		if (!mesh) {
			VE_CORE_ERROR_PRINT("Terrain: mesh generation failed");
			return;
		}

		registerMesh(comp, entity, mesh);
		comp.builtKey = makeBuildInputs(comp, *sys).key();
		comp.bDirty = false;
		placeTile(scene, entity);
	}

	uint32_t Terrain::findTile(Scene& scene, const glm::ivec2& coord) {
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			if (scene.getComponent<TerrainComponent>(id).tileCoord == coord)
				return id;
		}
		return UINT32_MAX;
	}

	glm::ivec2 Terrain::tileStep(TileDir dir) {
		switch (dir) {
			case TileDir::Left:  return glm::ivec2(-1, 0);
			case TileDir::Right: return glm::ivec2( 1, 0);
			case TileDir::Up:    return glm::ivec2( 0, 1);
			case TileDir::Down:  return glm::ivec2( 0,-1);
		}
		return glm::ivec2(0);
	}

	uint32_t Terrain::neighbor(Scene& scene, uint32_t entity, TileDir dir) {
		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return UINT32_MAX;

		return findTile(scene,
			scene.getComponent<TerrainComponent>(entity).tileCoord + tileStep(dir));
	}

	uint32_t Terrain::addTile(Scene& scene, const glm::ivec2& coord) {
		const uint32_t occupant = findTile(scene, coord);
		if (occupant != UINT32_MAX)
			return occupant;

		return createTileAt(scene, coord);
	}

	bool Terrain::removeTile(Scene& scene, const glm::ivec2& coord) {
		const uint32_t id = findTile(scene, coord);
		if (id == UINT32_MAX)
			return false;

		destroyTile(scene, id);
		return true;
	}

	void Terrain::addTiles(Scene& scene, const glm::ivec2& minCoord, const glm::ivec2& maxCoord) {
		const glm::ivec2 lo = glm::min(minCoord, maxCoord);
		const glm::ivec2 hi = glm::max(minCoord, maxCoord);

		// One occupancy pass instead of a findTile scan per cell; the grid editor
		// fills whole rectangles at a time.
		std::unordered_set<int64_t> occupied;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>())
			occupied.insert(packCoord(scene.getComponent<TerrainComponent>(id).tileCoord));

		for (int y = lo.y; y <= hi.y; ++y) {
			for (int x = lo.x; x <= hi.x; ++x) {
				const glm::ivec2 coord(x, y);
				if (occupied.count(packCoord(coord)))
					continue;
				createTileAt(scene, coord);
			}
		}
	}

	void Terrain::removeTiles(Scene& scene, const glm::ivec2& minCoord, const glm::ivec2& maxCoord) {
		const glm::ivec2 lo = glm::min(minCoord, maxCoord);
		const glm::ivec2 hi = glm::max(minCoord, maxCoord);

		// Collect the victims first: destroying an entity reorders the component
		// storage the group() snapshot was taken from.
		std::vector<uint32_t> victims;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			const glm::ivec2 c = scene.getComponent<TerrainComponent>(id).tileCoord;
			if (c.x >= lo.x && c.x <= hi.x && c.y >= lo.y && c.y <= hi.y)
				victims.push_back(id);
		}

		for (uint32_t id : victims)
			destroyTile(scene, id);
	}

	void Terrain::placeTile(Scene& scene, uint32_t entity) {
		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;
		if (!scene.getRegistry().has<TransformComponent>(entity))
			return;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return;

		const TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		const float size = tileWorldSize(comp, *sys);
		if (size <= 0.0f)
			return;

		const glm::vec2 origin = tileOrigin(comp, *sys);
		TransformComponent& tr = scene.getComponent<TransformComponent>(entity);
		tr.transform[3].x = origin.x;
		tr.transform[3].z = origin.y;
	}

	void Terrain::setTileCoord(Scene& scene, uint32_t entity, const glm::ivec2& coord) {
		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (comp.tileCoord == coord)
			return;
		comp.tileCoord = coord;
		placeTile(scene, entity);
	}

	void Terrain::snapToGrid(Scene& scene, uint32_t entity) {
		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;
		if (!scene.getRegistry().has<TransformComponent>(entity))
			return;

		TerrainSystemComponent* sys = findSystem(scene);
		if (!sys)
			return;

		const TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		const float size = tileWorldSize(comp, *sys);
		if (size <= 0.0f)
			return;

		const TransformComponent& tr = scene.getComponent<TransformComponent>(entity);
		setTileCoord(scene, entity, glm::ivec2(
			(int)std::lround(tr.transform[3].x / size),
			(int)std::lround(tr.transform[3].z / size)));
		// Also pull a tile that is inside the right cell back onto the exact grid.
		placeTile(scene, entity);
	}

}
