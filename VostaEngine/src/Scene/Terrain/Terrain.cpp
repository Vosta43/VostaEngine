#include "vepch.h"
#include "Terrain.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Scene/Terrain/QuadTreeTerrain.h"
#include "Scene/Terrain/TerrainBuildQueue.h"
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
#include "Noise/NoiseNodes.h"
#include "Noise/NoisePlan.h"
#include "Terrain/RuleGraphResource.h"
#include "Terrain/RulePlan.h"
#include "Renderer/LayeredMaterial.h"
#include "Renderer/LayeredShaderBuilder.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialInstance.h"
#include "Renderer/MaterialLayer.h"
#include "Renderer/MaterialLayerAsset.h"
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

		// Starting flat plane; stands in only until a heightmap takes over.
		constexpr float kFlatWorldSize = 100.0f;
		constexpr int   kFlatSegments = 32;

		// One weight slice per material layer, at the paint/bake texel resolution.
		constexpr int kTerrainWeightRes = 512;

		// Each commit is a full mesh + weight upload, so ready builds are spread
		// over frames instead of landing together.
		constexpr size_t kMaxCommitsPerFrame = 1;

		TerrainSystemComponent* findSystemOf(Scene& scene) {
			for (uint32_t id : scene.getRegistry().view<TerrainSystemComponent>())
				return &scene.getRegistry().get<TerrainSystemComponent>(id);
			return nullptr;
		}

		Ref<TerrainDataResource> findTerrainDataOf(Scene& scene) {
			TerrainSystemComponent* sys = findSystemOf(scene);
			if (!sys)
				return nullptr;

			if (sys->terrainDataHandle.isValid())
				sys->terrainData = ResourceManager::get<TerrainDataResource>(sys->terrainDataHandle);

			return sys->terrainData;
		}

		Ref<TerrainDataResource> ensureTerrainDataOf(Scene& scene) {
			Ref<TerrainDataResource> data = findTerrainDataOf(scene);
			if (data)
				return data;

			TerrainSystemComponent* sys = findSystemOf(scene);
			if (!sys)
				return nullptr;

			sys->terrainData = CreateRef<TerrainDataResource>();
			return sys->terrainData;
		}

		float fract1(float x) { return x - std::floor(x); }

		// CPU mirror of the shader's hash21/valueNoise; the shader's copy serves its
		// macro-variation term alone.
		float hash21(const glm::vec2& p0) {
			glm::vec2 p(fract1(p0.x * 123.34f), fract1(p0.y * 456.21f));
			const float d = p.x * (p.x + 45.32f) + p.y * (p.y + 45.32f);
			p += glm::vec2(d);
			return fract1(p.x * p.y);
		}

		float valueNoise(const glm::vec2& p) {
			const glm::vec2 i(std::floor(p.x), std::floor(p.y));
			const glm::vec2 f(fract1(p.x), fract1(p.y));
			const glm::vec2 ff = f * f * (3.0f - 2.0f * f);
			const float a = hash21(i);
			const float b = hash21(i + glm::vec2(1.0f, 0.0f));
			const float c = hash21(i + glm::vec2(0.0f, 1.0f));
			const float d = hash21(i + glm::vec2(1.0f, 1.0f));
			return glm::mix(glm::mix(a, b, ff.x), glm::mix(c, d, ff.x), ff.y);
		}

		// 1 inside [lo, hi] with soft edges; 0 / 1 as an endpoint means "unbounded".
		float softBand(float x, float lo, float hi, float soft) {
			auto smooth = [](float e0, float e1, float v) {
				const float t = glm::clamp((v - e0) / (e1 - e0), 0.0f, 1.0f);
				return t * t * (3.0f - 2.0f * t);
			};
			const float rising  = (lo <= 0.0f) ? 1.0f : smooth(lo - soft, lo + soft, x);
			const float falling = (hi >= 1.0f) ? 1.0f : (1.0f - smooth(hi - soft, hi + soft, x));
			return rising * falling;
		}

		// Byte offset of one texel inside a slice-major weight buffer.
		size_t weightIndex(int layer, int res, int x, int z) {
			return (size_t)layer * res * res + (size_t)z * res + x;
		}

		std::string terrainMeshPath(uint32_t entityId) {
			return "__terrain/" + std::to_string(entityId);
		}

		void registerMesh(TerrainComponent& comp, uint32_t entityId, const Ref<StaticMesh>& mesh) {
			auto& storage = ResourceManager::getStorage<StaticMesh>();
			if (comp.generatedMeshHandle.isValid())
				storage.remove(comp.generatedMeshHandle);
			comp.generatedMeshHandle = storage.store(terrainMeshPath(entityId), mesh);
			storage.pin(comp.generatedMeshHandle);
		}

		enum class TerrainSourceKind : uint32_t { Flat, HeightMap, Noise };

		// Fields unused by a source stay zero, so e.g. noiseResolution never
		// invalidates a heightmap tile.
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

		// Mirrors the mesh builder's extraction; indexed [x * res + z] like it.
		// Downstream assumes a square grid (one stride, one scalar extent), so a
		// rectangular field is refused and the terrain left unsampled.
		void extractHeightField(std::vector<float>& heightField, int& heightRes,
			const Ref<TextureResource>& tex) {
			const int w = (int)tex->width;
			const int h = (int)tex->height;
			if (w <= 0 || h <= 0) return;

			if (w != h) {
				VE_CORE_ERROR_PRINT(
					"Terrain: height field must be square, got %dx%d; brush disabled", w, h);
				heightField.clear();
				heightRes = 0;
				return;
			}

			const int channels = getChannelCount(tex->format);
			const bool useFloat = !tex->floatPixels.empty();

			heightField.assign((size_t)w * h, 0.0f);
			for (int i = 0; i < w; ++i) {
				for (int j = 0; j < h; ++j) {
					const size_t src = (size_t)(i * h + j) * channels;
					heightField[(size_t)i * h + j] = useFloat
						? tex->floatPixels[src]
						: tex->pixels[src] / 255.0f;
				}
			}
			heightRes = w;
		}

		// Bilinear height at a terrain-local XZ, in world units. Spacing and scale
		// are passed in rather than read off a tile, so a worker can sample a grid
		// it is still building.
		float sampleSurfaceLocal(const float* heightField, int heightRes, float tileSize,
			float heightScale, float lx, float lz) {
			const int n = heightRes;
			const float ts = tileSize > 0.0f ? tileSize : 1.0f;

			float fx = std::clamp(lx / ts, 0.0f, (float)(n - 1));
			float fz = std::clamp(lz / ts, 0.0f, (float)(n - 1));

			const int x0 = (int)fx, z0 = (int)fz;
			const int x1 = std::min(x0 + 1, n - 1);
			const int z1 = std::min(z0 + 1, n - 1);
			const float tx = fx - x0, tz = fz - z0;

			auto H = [&](int i, int j) { return heightField[(size_t)i * n + j]; };
			const float h0 = glm::mix(H(x0, z0), H(x1, z0), tx);
			const float h1 = glm::mix(H(x0, z1), H(x1, z1), tx);
			return glm::mix(h0, h1, tz) * heightScale;
		}

		float sampleSurfaceLocal(const TerrainComponent& comp, float tileSize, float heightScale,
			float lx, float lz) {
			return sampleSurfaceLocal(comp.heightField.data(), comp.heightRes, tileSize,
				heightScale, lx, lz);
		}

		// Called when the height grid is replaced: the old weights would otherwise
		// be stretched across a differently-sized world instead of being redrawn.
		void invalidateWeightMap(Scene& scene, TerrainComponent& comp) {
			if (comp.weightMapHandle.isValid()) {
				ResourceManager::getStorage<Texture2DArray>().remove(comp.weightMapHandle);
				comp.weightMapHandle = INVALID_ASSET_HANDLE;
			}
			if (auto data = findTerrainDataOf(scene))
				data->erase(comp.tileCoord);
			comp.bWeightsDirty = false;
			comp.weightBakeKey = 0;

			// The base's own defaults (all-zero array, invSize 0) have to stand again;
			// ensureMaterialInstance repopulates the overrides next frame.
			if (comp.materialInstance)
				comp.materialInstance->clearOverrides();
		}

		uint32_t createTileAt(Scene& scene, const glm::ivec2& coord, bool streamed = false) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Terrain");
			scene.assignComponent<TransformComponent>(entity);
			TerrainComponent& comp = scene.assignComponent<TerrainComponent>(entity);

			comp.tileCoord = coord;
			comp.bDirty = true;
			comp.bStreamed = streamed;
			TerrainTile(scene, entity.m_id).place();
			return entity.m_id;
		}

		// Its resources are keyed by entity id, so nothing else reclaims them; a
		// destroyed tile would otherwise leave them alive in the manager.
		void destroyTile(Scene& scene, uint32_t id) {
			const TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);
			if (auto data = findTerrainDataOf(scene))
				data->erase(comp.tileCoord);
			ResourceManager::getStorage<StaticMesh>().remove(comp.generatedMeshHandle);
			ResourceManager::getStorage<Texture2DArray>().remove(comp.weightMapHandle);
			ResourceManager::getStorage<Material>().remove(comp.materialInstanceHandle);
			scene.destroyEntity(scene.getEntity(id));
		}

		int64_t packCoord(const glm::ivec2& coord) {
			return TerrainDataResource::pack(coord);
		}

		// The streamed flag is stamped here, where the buffer appears, rather than at
		// save time where the scene is long gone.
		TerrainDataTile& tileFor(TerrainDataResource& data, const TerrainComponent& comp) {
			TerrainDataTile& tile = data.tile(comp.tileCoord);
			tile.bTransient = comp.bStreamed;
			return tile;
		}

		float sourceStride(const TerrainSystemComponent& sys) {
			const int res = std::clamp(sys.noiseResolution, 16, 4096);
			return res > 1 ? (float)(res - 1) * sys.tileSize : 0.0f;
		}

		// Build this tile's weight-array texture from the scene's terrain data.
		// INVALID when there is no height field or no weights yet, which keeps the
		// array lazily disengaged while unbaked.
		AssetHandle ensureWeightMap(Scene& scene, TerrainComponent& comp, uint32_t entityId,
			float tileSize) {

			if (comp.heightRes <= 0 || tileSize <= 0.0f)
				return INVALID_ASSET_HANDLE;

			auto data = findTerrainDataOf(scene);
			if (!data)
				return INVALID_ASSET_HANDLE;

			TerrainDataTile* tile = data->find(comp.tileCoord);
			if (!tile || tile->weightRes <= 0 || tile->layerCount <= 0 || tile->weightData.empty())
				return INVALID_ASSET_HANDLE;

			// A changed resolution or slice count forces a rebuild; reusing the array
			// would write past the end of the GL texture.
			if (comp.weightMapHandle.isValid()) {
				if (auto tex = ResourceManager::get<Texture2DArray>(comp.weightMapHandle))
					if (tex->getWidth() == (uint32_t)tile->weightRes
						&& tex->getHeight() == (uint32_t)tile->weightRes
						&& tex->getLayers() == (uint32_t)tile->layerCount)
						return comp.weightMapHandle;
				ResourceManager::getStorage<Texture2DArray>().remove(comp.weightMapHandle);
				comp.weightMapHandle = INVALID_ASSET_HANDLE;
			}

			// Guard the upload against a truncated/corrupt data file.
			const size_t expected = (size_t)tile->layerCount * tile->weightRes * tile->weightRes;
			if (tile->weightData.size() != expected)
				return INVALID_ASSET_HANDLE;

			auto tex = Texture2DArray::create((uint32_t)tile->weightRes, (uint32_t)tile->weightRes,
				(uint32_t)tile->layerCount, TextureFormat::R8, nullptr);
			if (!tex)
				return INVALID_ASSET_HANDLE;
			tex->setData(tile->weightData.data(), (uint32_t)tile->weightData.size());

			auto& storage = ResourceManager::getStorage<Texture2DArray>();
			const std::string key = "__terrain_weights_" + std::to_string(entityId);
			storage.remove(key);
			comp.weightMapHandle = storage.store(key, tex);
			storage.pin(comp.weightMapHandle);
			comp.bWeightsDirty = false;

			return comp.weightMapHandle;
		}

		// Weight slices the material needs: one per layer, base counted. 1 when no
		// layered material is assigned, so callers can size a buffer without a
		// separate null check.
		int terrainLayerCount(Scene& scene) {
			TerrainSystemComponent* sys = findSystemOf(scene);
			if (!sys)
				return 1;
			auto layered = std::dynamic_pointer_cast<LayeredMaterial>(
				ResourceManager::get<Material>(sys->materialHandle));
			return layered
				? std::clamp((int)layered->layers.size(), 1, kMaxMaterialLayers) : 1;
		}

		// One layer's scalar-rule inputs, snapshotted off its asset so the bake can
		// run on a worker without touching the resource manager or a live asset.
		struct WeightLayerParams {
			bool  present = false;
			bool  useSlope = false;
			float slopeMin = 0.0f, slopeMax = 1.0f;
			bool  useHeight = false;
			float heightMin = 0.0f, heightMax = 1.0f;
			float noiseStrength = 0.0f;
			float noiseScale = 1.0f;
		};

		struct WeightBakeInputs {
			int       weightRes = kTerrainWeightRes;
			int       layerCount = 0;
			float     tileSize = 0.0f;
			float     heightScale = 0.0f;
			float     worldSize = 0.0f;
			glm::vec2 origin{ 0.0f };
			std::vector<WeightLayerParams> layers;
			// The plan holds raw node pointers, so the copy of the graph is what keeps
			// them alive for the job's life.
			RuleGraph graph;
			RulePlan  plan;
			bool      hasRuleGraph = false;
			uint64_t  key = 0;
		};

		// Snapshot everything the weight bake depends on so it can run on a worker.
		// False when the tile has no height field or the rule graph is not valid; the
		// caller then leaves the weights alone and retries later.
		bool resolveWeightBake(Scene& scene, const TerrainComponent& comp,
			const TerrainSystemComponent& sys, uint64_t builtKey, WeightBakeInputs& out) {

			const float worldSize = TerrainTile::tileWorldSize(comp, sys);
			if (worldSize <= 0.0f || comp.heightRes <= 0)
				return false;

			// Honour a resolution the tile already has so a rebuild keeps its shape.
			int weightRes = kTerrainWeightRes;
			if (auto data = findTerrainDataOf(scene))
				if (TerrainDataTile* t = data->find(comp.tileCoord))
					if (t->weightRes > 0)
						weightRes = t->weightRes;

			auto layered = std::dynamic_pointer_cast<LayeredMaterial>(
				ResourceManager::get<Material>(sys.materialHandle));
			const int layerCount = layered
				? std::clamp((int)layered->layers.size(), 1, kMaxMaterialLayers) : 1;

			Ref<TerrainRuleGraphResource> res;
			if (sys.ruleGraphHandle.isValid())
				res = ResourceManager::get<TerrainRuleGraphResource>(sys.ruleGraphHandle);
			const bool hasGraph = res && res->contentHash != 0;
			if (hasGraph) {
				out.graph = res->graph;
				out.plan = buildRulePlan(out.graph);
				if (!out.plan.valid())
					return false;
			}
			out.hasRuleGraph = hasGraph;

			uint64_t key = hashString("terrain_weights");
			key = hashValue(key, builtKey);
			key = hashValue(key, hasGraph ? res->contentHash : 0ull);
			key = hashValue(key, layerCount);

			out.layers.resize(layerCount);
			for (int i = 0; i < layerCount; ++i) {
				const MaterialLayerAsset* asset = layered
					? ResourceManager::get<MaterialLayerAsset>(layered->layers[i]).get() : nullptr;
				WeightLayerParams& p = out.layers[i];
				if (!asset) { key = hashValue(key, 0u); continue; }
				const MaterialLayer& L = asset->layer;
				p.present = true;
				p.useSlope = L.useSlope;
				p.slopeMin = L.slopeMin;
				p.slopeMax = L.slopeMax;
				p.useHeight = L.useHeight;
				p.heightMin = L.heightMin;
				p.heightMax = L.heightMax;
				p.noiseStrength = L.noiseStrength;
				p.noiseScale = L.noiseScale;
				key = hashValue(key, L.material.index());
				key = hashValue(key, L.material.generation());
				key = hashValue(key, L.heightMap.index());
				key = hashValue(key, L.heightMap.generation());
				key = hashValue(key, L.tiling);
				key = hashValue(key, (int)L.blend);
				key = hashValue(key, L.useSlope ? 1 : 0);
				key = hashValue(key, L.slopeMin);
				key = hashValue(key, L.slopeMax);
				key = hashValue(key, L.useHeight ? 1 : 0);
				key = hashValue(key, L.heightMin);
				key = hashValue(key, L.heightMax);
				key = hashValue(key, L.noiseStrength);
				key = hashValue(key, L.noiseScale);
			}

			out.weightRes = weightRes;
			out.layerCount = layerCount;
			out.tileSize = sys.tileSize;
			out.heightScale = sys.heightScale;
			out.worldSize = worldSize;
			out.origin = TerrainTile::tileOrigin(comp, sys);
			out.key = key;
			return true;
		}

		// Fill `out` (slice-major, layer * res * res) from the resolved inputs and a raw
		// height grid. parallel=false on a worker already on the pool, so it does not
		// park its siblings waiting on itself.
		void bakeWeightSlices(std::vector<uint8_t>& out, const WeightBakeInputs& in,
			const float* heightGrid, int heightRes, bool parallel) {

			const int wres = in.weightRes;
			const int layerCount = in.layerCount;
			out.assign((size_t)layerCount * wres * wres, 0);
			if (wres <= 0 || layerCount <= 1 || !heightGrid || heightRes <= 0)
				return;

			const float step = in.worldSize / (float)wres;
			uint8_t* dst = out.data();

			const auto bakeRow = [&](int z) {
				if (in.hasRuleGraph) {
					thread_local RulePlanScratch scratch;
					RuleEvalContext ctx;
					ctx.heightGrid  = heightGrid;
					ctx.heightRes   = heightRes;
					ctx.tileSize    = in.tileSize;
					ctx.heightScale = in.heightScale;
					ctx.worldOrigin = in.origin;
					float w[kMaxMaterialLayers - 1];
					for (int x = 0; x < wres; ++x) {
						ctx.sample = glm::vec2(in.origin.x + (x + 0.5f) * step,
							in.origin.y + (z + 0.5f) * step);
						in.plan.evaluate(scratch, ctx, w, layerCount - 1);
						for (int i = 1; i < layerCount; ++i)
							dst[weightIndex(i, wres, x, z)] =
								(uint8_t)std::lround(std::clamp(w[i - 1], 0.0f, 1.0f) * 255.0f);
					}
					return;
				}

				const float ts = in.tileSize > 0.0f ? in.tileSize : 1.0f;
				const float hs = in.heightScale;
				const float e = ts;
				for (int x = 0; x < wres; ++x) {
					const float lx = (x + 0.5f) * step;
					const float lz = (z + 0.5f) * step;
					const float wy = sampleSurfaceLocal(heightGrid, heightRes, ts, hs, lx, lz);
					const float heightFrac = std::clamp(wy / std::max(hs, 1e-4f), 0.0f, 1.0f);
					const float hL = sampleSurfaceLocal(heightGrid, heightRes, ts, hs, lx - e, lz);
					const float hR = sampleSurfaceLocal(heightGrid, heightRes, ts, hs, lx + e, lz);
					const float hD = sampleSurfaceLocal(heightGrid, heightRes, ts, hs, lx, lz - e);
					const float hU = sampleSurfaceLocal(heightGrid, heightRes, ts, hs, lx, lz + e);
					const glm::vec3 n = glm::normalize(glm::vec3(hL - hR, 2.0f * e, hD - hU));
					const float slope = 1.0f - std::clamp(n.y, 0.0f, 1.0f);
					const glm::vec2 wxz = in.origin + glm::vec2(lx, lz);

					for (int i = 1; i < layerCount; ++i) {
						const WeightLayerParams& L = in.layers[i];
						if (!L.present) continue;

						float s = 1.0f;
						if (L.useSlope)  s *= softBand(slope, L.slopeMin, L.slopeMax, 0.05f);
						if (L.useHeight) s *= softBand(heightFrac, L.heightMin, L.heightMax, 0.05f);
						if (L.noiseStrength > 0.0f) {
							const float nz = valueNoise(wxz / std::max(L.noiseScale, 1e-4f));
							s = std::clamp(s + (nz - 0.5f) * 2.0f * L.noiseStrength, 0.0f, 1.0f);
						}
						dst[weightIndex(i, wres, x, z)] =
							(uint8_t)std::lround(std::clamp(s, 0.0f, 1.0f) * 255.0f);
					}
				}
			};

			if (parallel && JobSystem::get().threadCount() > 1)
				JobSystem::get().parallelFor(0, wres, bakeRow).get();
			else
				for (int z = 0; z < wres; ++z)
					bakeRow(z);
		}

		// Bake a job's weights from the grid it just built, on the worker that built
		// it; the rows stay serial because that worker is already on the pool.
		void bakeIntoProduct(TerrainMeshBuild& out, const WeightBakeInputs& in,
			const float* heightGrid, int heightRes) {
			bakeWeightSlices(out.weightData, in, heightGrid, heightRes, /*parallel=*/false);
			out.weightRes = in.weightRes;
			out.layerCount = in.layerCount;
			out.weightKey = in.key;
		}

		// Pure CPU -- no GL, no scene, no resource manager -- so it may run on a
		// worker thread; the graph behind `plan` has to outlive it.
		void buildNoiseTile(TerrainMeshBuild& out, const NoisePlan& plan, const glm::ivec2& coord,
			int n, float tileSize, float heightScale, int maxDepth, int segments) {

			const glm::vec2 origin = glm::vec2(coord) * ((float)(n - 1) * tileSize);

			auto tex = CreateRef<TextureResource>();
			tex->width = (uint32_t)n;
			tex->height = (uint32_t)n;
			tex->format = TextureFormat::R16F;
			tex->floatPixels.assign((size_t)n * n, 0.0f);

			const auto sampleRow = [&](int i) {
				thread_local NoisePlanScratch scratch;
				for (int j = 0; j < n; ++j)
					tex->floatPixels[(size_t)i * n + j] = plan.evaluate(scratch,
						origin.x + (float)i * tileSize, origin.y + (float)j * tileSize);
			};

			// Fanned out here: on a worker this is the job's only occupancy of the pool.
			if (JobSystem::get().threadCount() > 1)
				JobSystem::get().parallelFor(0, n, sampleRow).get();
			else
				for (int i = 0; i < n; ++i)
					sampleRow(i);

			extractHeightField(out.heights, out.heightRes, tex);
			out.mesh = QuadTreeTerrain::buildResource(tex, tileSize, heightScale,
				maxDepth, segments, origin);
		}

		// Adopt a generated tile into the scene. Render thread only -- it touches GL,
		// the resource manager and the registry. False when the product is stale or
		// unusable, a normal outcome for a queued build.
		bool commitTileBuild(Scene& scene, uint32_t entity, const TerrainSystemComponent& sys,
			uint64_t key, TerrainMeshBuild& product) {

			if (!product.valid() || !scene.getRegistry().has<TerrainComponent>(entity))
				return false;

			TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
			if (makeBuildInputs(comp, sys).key() != key)
				return false;
			if (comp.generatedMeshHandle.isValid() && !comp.bDirty && comp.builtKey == key)
				return false;

			// prevRes == 0 is the first bake, not a change: a loaded scene keeps its
			// painted weights.
			const int prevRes = comp.heightRes;
			comp.heightField = std::move(product.heights);
			comp.heightRes = product.heightRes;
			if (prevRes > 0 && comp.heightRes != prevRes)
				invalidateWeightMap(scene, comp);

			if (!comp.quadtree)
				comp.quadtree = CreateRef<QuadTreeTerrain>();

			auto mesh = comp.quadtree->commit(std::move(product.mesh));
			if (!mesh) {
				VE_CORE_ERROR_PRINT("Terrain: mesh generation failed");
				return false;
			}

			registerMesh(comp, entity, mesh);
			comp.builtKey = key;
			comp.bDirty = false;

			// Landing the worker's weight key makes the next bakeWeights a no-op, so
			// the render thread never re-bakes them.
			if (!product.weightData.empty() && product.weightRes > 0) {
				if (auto data = ensureTerrainDataOf(scene)) {
					TerrainDataTile& tile = tileFor(*data, comp);
					tile.weightRes = product.weightRes;
					tile.layerCount = product.layerCount;
					tile.weightData = std::move(product.weightData);
					data->bDirty = true;
					comp.weightBakeKey = product.weightKey;
					comp.bWeightsDirty = true;
				}
			}

			TerrainTile(scene, entity).place();
			return true;
		}

	}

	TerrainSystemComponent* TerrainSubsystem::system() {
		return findSystemOf(*m_scene);
	}

	uint32_t TerrainSubsystem::ensureSystem() {
		Scene& scene = *m_scene;
		for (uint32_t id : scene.getRegistry().view<TerrainSystemComponent>())
			return id;

		Entity entity = scene.createEntity();
		scene.assignComponent<NameComponent>(entity, "TerrainSystem");
		scene.assignComponent<TerrainSystemComponent>(entity);
		return entity.m_id;
	}

	Ref<TerrainDataResource> TerrainSubsystem::data() {
		return findTerrainDataOf(*m_scene);
	}

	Ref<TerrainDataResource> TerrainSubsystem::ensureData() {
		return ensureTerrainDataOf(*m_scene);
	}

	glm::ivec2 TerrainSubsystem::tileStep(TileDir dir) {
		switch (dir) {
			case TileDir::Left:  return glm::ivec2(-1, 0);
			case TileDir::Right: return glm::ivec2( 1, 0);
			case TileDir::Up:    return glm::ivec2( 0, 1);
			case TileDir::Down:  return glm::ivec2( 0,-1);
		}
		return glm::ivec2(0);
	}

	uint32_t TerrainSubsystem::findTile(const glm::ivec2& coord) {
		Scene& scene = *m_scene;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			if (scene.getComponent<TerrainComponent>(id).tileCoord == coord)
				return id;
		}
		return UINT32_MAX;
	}

	uint32_t TerrainSubsystem::neighbor(uint32_t entity, TileDir dir) {
		Scene& scene = *m_scene;
		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return UINT32_MAX;

		return findTile(
			scene.getComponent<TerrainComponent>(entity).tileCoord + tileStep(dir));
	}

	uint32_t TerrainSubsystem::addTile(const glm::ivec2& coord) {
		const uint32_t occupant = findTile(coord);
		if (occupant != UINT32_MAX)
			return occupant;

		return createTileAt(*m_scene, coord);
	}

	bool TerrainSubsystem::removeTile(const glm::ivec2& coord) {
		const uint32_t id = findTile(coord);
		if (id == UINT32_MAX)
			return false;

		destroyTile(*m_scene, id);
		return true;
	}

	void TerrainSubsystem::addTiles(const glm::ivec2& minCoord, const glm::ivec2& maxCoord) {
		Scene& scene = *m_scene;
		const glm::ivec2 lo = glm::min(minCoord, maxCoord);
		const glm::ivec2 hi = glm::max(minCoord, maxCoord);

		// One occupancy pass instead of a findTile scan per cell.
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

	void TerrainSubsystem::removeTiles(const glm::ivec2& minCoord, const glm::ivec2& maxCoord) {
		Scene& scene = *m_scene;
		const glm::ivec2 lo = glm::min(minCoord, maxCoord);
		const glm::ivec2 hi = glm::max(minCoord, maxCoord);

		// Collect the victims first: destroying an entity reorders the storage the
		// group() snapshot walks.
		std::vector<uint32_t> victims;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			const glm::ivec2 c = scene.getComponent<TerrainComponent>(id).tileCoord;
			if (c.x >= lo.x && c.x <= hi.x && c.y >= lo.y && c.y <= hi.y)
				victims.push_back(id);
		}

		for (uint32_t id : victims)
			destroyTile(scene, id);
	}

	int TerrainSubsystem::streamUpdate(std::unordered_set<int64_t>& activeCoords,
		const glm::vec3& viewpoint, const TerrainSystemComponent& sys) {

		Scene& scene = *m_scene;

		if (!sys.bDistanceStreaming) {
			// Retract any window still out: nobody else knows which tiles this
			// streamer created, so leaving them would strand tiles only it can destroy.
			if (activeCoords.empty())
				return 0;

			std::vector<uint32_t> victims;
			for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
				const TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);
				if (comp.bStreamed && activeCoords.count(packCoord(comp.tileCoord)))
					victims.push_back(id);
			}
			for (uint32_t id : victims)
				destroyTile(scene, id);
			activeCoords.clear();
			return 0;
		}

		// A streamed tile has no height map of its own, so it can only be a window
		// onto a world-spanning source -- today the system's noise graph. Without one
		// the window would be flat placeholders stacked at the origin.
		if (!sys.noiseGraphHandle.isValid())
			return 0;

		// Anchor on the source grid, not a live tile: one that has not rebuilt yet
		// would report its old stride, and group() order picks which tile wins.
		const float stride = sourceStride(sys);
		if (stride <= 0.0f)
			return 0;

		const glm::ivec2 center((int)std::floor(viewpoint.x / stride),
			(int)std::floor(viewpoint.z / stride));
		const int radius = (std::max)(0, sys.streamRadiusTiles);
		const int pad = (std::max)(0, sys.streamUnloadPadTiles);

		// One occupancy pass, so the reconcile below is linear in the live tile count
		// instead of a findTile scan per candidate coordinate.
		const auto liveTiles = scene.getRegistry().group<TransformComponent, TerrainComponent>();
		std::unordered_map<int64_t, uint32_t> occupied;
		occupied.reserve(liveTiles.size());
		for (uint32_t id : liveTiles)
			occupied.emplace(packCoord(scene.getComponent<TerrainComponent>(id).tileCoord), id);

		// The pad is the hysteresis band: a tile is created within `radius` and only
		// destroyed past `radius + pad`, so a camera on a grid boundary does not
		// create and destroy the same tile every frame.
		for (auto it = activeCoords.begin(); it != activeCoords.end(); ) {
			const glm::ivec2 d = TerrainDataResource::unpack(*it) - center;
			if ((std::max)(std::abs(d.x), std::abs(d.y)) <= radius + pad) {
				++it;
				continue;
			}
			const auto found = occupied.find(*it);
			// Present but not streamed: the author took this cell over, not ours.
			if (found != occupied.end() &&
				scene.getComponent<TerrainComponent>(found->second).bStreamed) {
				destroyTile(scene, found->second);
				occupied.erase(found);
			}
			it = activeCoords.erase(it);
		}

		// Occupied cells are skipped rather than claimed: addTile would hand back the
		// author's tile, and this streamer must never destroy what it did not create.
		std::vector<glm::ivec2> wanted;
		for (int z = center.y - radius; z <= center.y + radius; ++z)
			for (int x = center.x - radius; x <= center.x + radius; ++x) {
				const glm::ivec2 coord(x, z);
				if (!occupied.count(packCoord(coord)))
					wanted.push_back(coord);
			}
		if (wanted.empty())
			return 0;

		// Nearest first, so a budget smaller than the missing set surfaces the ground
		// under the camera rather than the rim.
		const auto cellDist = [&center](const glm::ivec2& c) {
			const glm::ivec2 d = c - center;
			return d.x * d.x + d.y * d.y;
		};
		std::sort(wanted.begin(), wanted.end(), [&cellDist](const glm::ivec2& a, const glm::ivec2& b) {
			return cellDist(a) < cellDist(b);
		});

		const int budget = (std::min)((std::max)(1, sys.streamBuildBudget), (int)wanted.size());
		for (int i = 0; i < budget; ++i) {
			createTileAt(scene, wanted[i], /*streamed=*/true);
			activeCoords.insert(packCoord(wanted[i]));
		}
		return budget;
	}

	void TerrainSubsystem::fillWeight(int layerIndex) {
		Scene& scene = *m_scene;

		if (layerIndex < 1 || layerIndex >= kMaxMaterialLayers)
			return;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return;

		const int layerCount = terrainLayerCount(scene);
		if (layerIndex >= layerCount)
			return;

		auto data = ensureTerrainDataOf(scene);
		if (!data)
			return;

		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);

			// Tiles with nothing to paint over (the flat placeholder).
			if (comp.heightRes <= 0)
				continue;

			TerrainDataTile& tile = tileFor(*data, comp);
			if (tile.weightRes <= 0)
				tile.weightRes = kTerrainWeightRes;
			tile.layerCount = layerCount;
			const size_t sliceSize = (size_t)tile.weightRes * tile.weightRes;
			if (tile.weightData.size() != (size_t)tile.layerCount * sliceSize)
				tile.weightData.assign((size_t)tile.layerCount * sliceSize, 0);

			AssetHandle weights = ensureWeightMap(scene, comp, id, sys->tileSize);
			if (!weights.isValid())
				continue;

			uint8_t* slice = tile.weightData.data() + (size_t)layerIndex * sliceSize;
			std::fill(slice, slice + sliceSize, (uint8_t)255);

			comp.bWeightsDirty = true;
		}

		data->bDirty = true;
	}

	void TerrainSubsystem::clearWeight(int layerIndex) {
		Scene& scene = *m_scene;

		if (layerIndex < 1 || layerIndex >= kMaxMaterialLayers)
			return;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return;

		const int layerCount = terrainLayerCount(scene);
		if (layerIndex >= layerCount)
			return;

		auto data = ensureTerrainDataOf(scene);
		if (!data)
			return;

		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);

			// Tiles with nothing to paint over (the flat placeholder).
			if (comp.heightRes <= 0)
				continue;

			TerrainDataTile& tile = tileFor(*data, comp);
			if (tile.weightRes <= 0)
				tile.weightRes = kTerrainWeightRes;
			tile.layerCount = layerCount;
			const size_t sliceSize = (size_t)tile.weightRes * tile.weightRes;
			if (tile.weightData.size() != (size_t)tile.layerCount * sliceSize)
				tile.weightData.assign((size_t)tile.layerCount * sliceSize, 0);

			AssetHandle weights = ensureWeightMap(scene, comp, id, sys->tileSize);
			if (!weights.isValid())
				continue;

			uint8_t* slice = tile.weightData.data() + (size_t)layerIndex * sliceSize;
			std::fill(slice, slice + sliceSize, (uint8_t)0);

			comp.bWeightsDirty = true;
		}

		data->bDirty = true;
	}

	void TerrainSubsystem::paintWeightWorld(uint32_t referenceTile, const glm::vec3& worldPoint,
		int layerIndex, float radius, float strength) {

		Scene& scene = *m_scene;

		if (layerIndex < 1 || layerIndex >= kMaxMaterialLayers || radius <= 0.0f || strength == 0.0f)
			return;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys || referenceTile == UINT32_MAX ||
			!scene.getRegistry().has<TerrainComponent>(referenceTile))
			return;

		const float worldSize = TerrainTile::tileWorldSize(
			scene.getComponent<TerrainComponent>(referenceTile), *sys);
		if (worldSize <= 0.0f)
			return;

		// Occupancy keyed by tile coordinate, so each candidate cell is one lookup
		// instead of a findTile scan.
		std::unordered_map<int64_t, uint32_t> byCoord;
		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>())
			byCoord.emplace(packCoord(scene.getComponent<TerrainComponent>(id).tileCoord), id);

		// Every tile the brush ball can touch. Their control-map texels are already
		// aligned in world space, so passing the same world point into each tile makes
		// the mask agree across the boundary instead of restarting at every edge.
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
				const glm::vec2 local = wp - TerrainTile::tileOrigin(
					scene.getComponent<TerrainComponent>(tile), *sys);
				TerrainTile(scene, tile).paint(local, layerIndex, radius, strength);
			}
		}
	}

	bool TerrainSubsystem::raycastScene(const glm::vec3& origin, const glm::vec3& dir,
		float maxDist, uint32_t& outEntity, glm::vec3& outWorldPoint) {

		Scene& scene = *m_scene;
		if (!findSystemOf(scene))
			return false;

		bool hit = false;
		float bestDist = 1e30f;

		for (uint32_t id : scene.getRegistry().group<TransformComponent, TerrainComponent>()) {
			const TerrainComponent& comp = scene.getComponent<TerrainComponent>(id);
			if (comp.heightRes <= 0 || comp.heightField.empty())
				continue;

			// Terrain-local ray: the tile transform is translation-only, as the
			// control-map mapping and the editor brush both assume.
			const glm::vec3 t(glm::vec3(scene.getComponent<TransformComponent>(id).position));
			const glm::vec3 localO = origin - glm::vec3(t.x, 0.0f, t.z);

			glm::vec3 localP;
			if (!TerrainTile(scene, id).raycast(localO, dir, maxDist, localP))
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

	int TerrainSubsystem::pumpBuilds() {
		Scene& scene = *m_scene;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys || !sys->builds)
			return 0;

		int committed = 0;
		for (TerrainBuildQueue::Entry& entry : sys->builds->takeFinished(kMaxCommitsPerFrame)) {
			// No tile at that coordinate any more: it was streamed away, or moved.
			const uint32_t entity = findTile(TerrainDataResource::unpack(entry.coord));
			if (entity == UINT32_MAX)
				continue;
			if (commitTileBuild(scene, entity, *sys, entry.key, *entry.product))
				++committed;
		}
		return committed;
	}

	bool TerrainTile::valid() const {
		return m_entity != UINT32_MAX && m_scene->getRegistry().has<TerrainComponent>(m_entity);
	}

	float TerrainTile::tileWorldSize(const TerrainComponent& comp, const TerrainSystemComponent& sys) {
		// Before a bake the field is empty, so fall back to the resolution the noise
		// graph will be sampled at; otherwise a sourced-but-ungenerated tile reports
		// no extent and cannot be placed or snapped.
		int res = comp.heightRes;
		if (res <= 1 && sys.noiseGraphHandle.isValid())
			res = std::clamp(sys.noiseResolution, 16, 4096);
		return res > 1 ? (float)(res - 1) * sys.tileSize : 0.0f;
	}

	glm::vec2 TerrainTile::tileOrigin(const TerrainComponent& comp, const TerrainSystemComponent& sys) {
		return glm::vec2(comp.tileCoord) * tileWorldSize(comp, sys);
	}

	float TerrainTile::worldSize() {
		TerrainSystemComponent* sys = findSystemOf(*m_scene);
		if (!sys || !valid())
			return 0.0f;
		return tileWorldSize(m_scene->getComponent<TerrainComponent>(m_entity), *sys);
	}

	glm::vec2 TerrainTile::origin() {
		TerrainSystemComponent* sys = findSystemOf(*m_scene);
		if (!sys || !valid())
			return glm::vec2(0.0f);
		return tileOrigin(m_scene->getComponent<TerrainComponent>(m_entity), *sys);
	}

	void TerrainTile::place() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;
		if (!scene.getRegistry().has<TransformComponent>(entity))
			return;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return;

		const TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		const float size = tileWorldSize(comp, *sys);
		if (size <= 0.0f)
			return;

		const glm::vec2 origin = tileOrigin(comp, *sys);
		TransformComponent& tr = scene.getComponent<TransformComponent>(entity);
		tr.position.x = (ve::Vec3::value_type)origin.x;
		tr.position.z = (ve::Vec3::value_type)origin.y;
	}

	void TerrainTile::setCoord(const glm::ivec2& coord) {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (comp.tileCoord == coord)
			return;
		comp.tileCoord = coord;
		place();
	}

	void TerrainTile::snapToGrid() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;
		if (!scene.getRegistry().has<TransformComponent>(entity))
			return;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return;

		const TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		const float size = tileWorldSize(comp, *sys);
		if (size <= 0.0f)
			return;

		const TransformComponent& tr = scene.getComponent<TransformComponent>(entity);
		setCoord(glm::ivec2(
			(int)std::lround((float)tr.position.x / size),
			(int)std::lround((float)tr.position.z / size)));
		// Also pull a tile that is inside the right cell back onto the exact grid.
		place();
	}

	void TerrainTile::ensureBuilt() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		// No system means no shared source or material, so the tile stays invisible
		// rather than drawing a placeholder with nothing to shade it.
		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		const uint64_t key = makeBuildInputs(comp, *sys).key();
		if (comp.generatedMeshHandle.isValid() && !comp.bDirty && comp.builtKey == key)
			return;

		// Noise graph wins when present, then heightmap, then the flat plane.
		if (sys->noiseGraphHandle.isValid()) {
			rebuildFromNoiseGraph();
			return;
		}

		if (comp.heightMapHandle.isValid()) {
			rebuildFromHeightMap();
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

	void TerrainTile::ensureBuiltAsync() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (entity == UINT32_MAX || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		const uint64_t key = makeBuildInputs(comp, *sys).key();
		if (comp.generatedMeshHandle.isValid() && !comp.bDirty && comp.builtKey == key)
			return;

		// Only the noise source earns a worker: it samples and meshes a whole grid,
		// while a heightmap tile is one file load and the flat plane is a fixed small
		// grid. Those two keep going through the synchronous path.
		if (!sys->noiseGraphHandle.isValid()) {
			ensureBuilt();
			return;
		}

		if (!sys->builds)
			sys->builds = CreateRef<TerrainBuildQueue>();

		// Admission before the snapshot: a queued or over-capacity tile would
		// otherwise copy the graph and resolve the weight bake every frame only to
		// have submit() reject them.
		if (!sys->builds->canAccept(packCoord(comp.tileCoord)))
			return;

		auto resource = ResourceManager::get<NoiseGraphResource>(sys->noiseGraphHandle);
		if (!resource) {
			VE_CORE_ERROR_PRINT("Terrain: failed to load noise graph");
			return;
		}

		// Snapshot here: the graph is only ever replaced whole, so a copy keeps the
		// nodes the worker's plan points at alive. Checking for an Output is cheap;
		// the full plan is built on the worker.
		NoiseGraph graph = resource->graph;
		if (!findNoiseOutput(graph)) {
			VE_CORE_ERROR_PRINT("Terrain: noise graph has no Output");
			return;
		}

		// Snapshot the weight bake too, so it runs on the worker beside the mesh and
		// the render thread only uploads the result.
		WeightBakeInputs weights;
		const bool hasWeights = resolveWeightBake(scene, comp, *sys, key, weights);

		sys->builds->submit(packCoord(comp.tileCoord), key,
			[graph = std::move(graph), coord = comp.tileCoord,
			 n = std::clamp(sys->noiseResolution, 16, 4096), tileSize = sys->tileSize,
			 heightScale = sys->heightScale, maxDepth = sys->maxDepth,
			 segments = sys->segments, weights = std::move(weights), hasWeights]
			(TerrainMeshBuild& out) {
				const NoisePlan plan = buildNoisePlan(graph);
				if (!plan.valid())
					return;
				buildNoiseTile(out, plan, coord, n, tileSize, heightScale, maxDepth, segments);
				if (hasWeights)
					bakeIntoProduct(out, weights, out.heights.data(), out.heightRes);
			});
	}

	void TerrainTile::rebuildFromHeightMap() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (!comp.heightMapHandle.isValid())
			return;

		const std::string heightMapPath =
			toAbsolute(ResourceManager::getPath<Texture2D>(comp.heightMapHandle));
		if (heightMapPath.empty())
			return;

		// The heightmap may be a raw image or an already-baked .veasset texture (e.g.
		// exported from the noise editor), so dispatch on the extension exactly like
		// Texture2D::create rather than assuming a source image.
		auto textureResource = utils::isEngineAsset(heightMapPath)
			? TextureImporter::deserialize(heightMapPath)
			: TextureImporter::importFromFile(heightMapPath);
		if (!textureResource) {
			VE_CORE_ERROR_PRINT("Terrain: failed to load heightmap %s", heightMapPath.c_str());
			return;
		}

		TerrainMeshBuild product;
		extractHeightField(product.heights, product.heightRes, textureResource);
		// The extent comes from the resolution being built: tileOrigin reads
		// heightRes, which the component does not have yet.
		const float worldSize = product.heightRes > 1
			? (float)(product.heightRes - 1) * sys->tileSize : 0.0f;
		product.mesh = QuadTreeTerrain::buildResource(textureResource, sys->tileSize,
			sys->heightScale, sys->maxDepth, sys->segments,
			glm::vec2(comp.tileCoord) * worldSize);

		commitTileBuild(scene, entity, *sys, makeBuildInputs(comp, *sys).key(), product);
	}

	void TerrainTile::rebuildFromNoiseGraph() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		TerrainSystemComponent* sys = findSystemOf(scene);
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

		TerrainMeshBuild product;
		buildNoiseTile(product, plan, comp.tileCoord, std::clamp(sys->noiseResolution, 16, 4096),
			sys->tileSize, sys->heightScale, sys->maxDepth, sys->segments);

		commitTileBuild(scene, entity, *sys, makeBuildInputs(comp, *sys).key(), product);
	}

	void TerrainTile::bakeWeights() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		// The height field is the surface these weights land on.
		if (comp.heightRes <= 0 || comp.heightField.empty() || sys->tileSize <= 0.0f)
			return;

		WeightBakeInputs in;
		if (!resolveWeightBake(scene, comp, *sys, comp.builtKey, in))
			return;

		// A still terrain never re-walks the grid: the key folds the height field, the
		// graph and every layer's scalar rule. The bake fills the whole map, so it
		// overwrites painted coverage too.
		if (in.key == comp.weightBakeKey)
			return;

		auto data = ensureTerrainDataOf(scene);
		if (!data)
			return;

		// One slice per layer the material has; the base (slice 0) is never written,
		// being the residual in the shader.
		TerrainDataTile& tile = tileFor(*data, comp);
		tile.weightRes = in.weightRes;
		tile.layerCount = in.layerCount;

		bakeWeightSlices(tile.weightData, in, comp.heightField.data(), comp.heightRes,
			/*parallel=*/true);

		comp.bWeightsDirty = true;
		data->bDirty = true;
		comp.weightBakeKey = in.key;
	}

	bool TerrainTile::flushWeightMap() {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (!scene.getRegistry().has<TerrainComponent>(entity))
			return false;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (!comp.bWeightsDirty || !comp.weightMapHandle.isValid())
			return false;

		auto data = findTerrainDataOf(scene);
		if (!data)
			return false;
		TerrainDataTile* tile = data->find(comp.tileCoord);
		if (!tile || tile->weightData.empty())
			return false;

		auto tex = ResourceManager::get<Texture2DArray>(comp.weightMapHandle);
		if (!tex)
			return false;

		// A shape change needs a fresh allocation, not an in-place upload: drop the
		// handle so ensureWeightMap rebuilds next frame.
		if (tex->getWidth() != (uint32_t)tile->weightRes
			|| tex->getHeight() != (uint32_t)tile->weightRes
			|| tex->getLayers() != (uint32_t)tile->layerCount) {
			ResourceManager::getStorage<Texture2DArray>().remove(comp.weightMapHandle);
			comp.weightMapHandle = INVALID_ASSET_HANDLE;
			return false;
		}

		tex->setData(tile->weightData.data(), (uint32_t)tile->weightData.size());
		comp.bWeightsDirty = false;
		return true;
	}

	AssetHandle TerrainTile::ensureMaterialInstance(const glm::vec2& worldOrigin) {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return INVALID_ASSET_HANDLE;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		// Fall back to the built-in material locally rather than writing a default
		// back into the system from a render call.
		const AssetHandle baseHandle = sys->materialHandle.isValid()
			? sys->materialHandle : BuiltinResources::getDefaultMaterial();

		// Rebuild only when the base changed; the overrides below are refreshed on
		// every call so they track a map that appears later and a transform that moves.
		const bool upToDate = comp.materialInstanceHandle.isValid() &&
			comp.materialInstanceBase == baseHandle;

		if (!upToDate) {
			auto base = ResourceManager::get<Material>(baseHandle);
			if (!base)
				return INVALID_ASSET_HANDLE;

			if (!comp.materialInstance)
				comp.materialInstance = CreateRef<MaterialInstance>();

			comp.materialInstance->base = base;
			comp.materialInstance->clearOverrides();

			// Drop any entry a previous life of this id left behind: store() dedups by
			// path and would otherwise hand back a stale resource.
			auto& storage = ResourceManager::getStorage<Material>();
			const std::string key = "__terrain_matinst_" + std::to_string(entity);
			storage.remove(key);
			comp.materialInstanceHandle = storage.store(key, comp.materialInstance);
			storage.pin(comp.materialInstanceHandle);
			comp.materialInstanceBase = baseHandle;
		}

		if (comp.materialInstance) {
			// Engaged only once the tile has baked slices; until then the base's
			// all-zero 1x1 default stands in.
			const float span = tileWorldSize(comp, *sys);
			AssetHandle weights = ensureWeightMap(scene, comp, entity, sys->tileSize);

			if (weights.isValid() && span > 0.0f) {
				comp.materialInstance->setTextureArray("u_Weights", weights,
					LayeredShaderBuilder::kWeightsTextureUnit);
				comp.materialInstance->setVec2("u_TerrainOrigin", worldOrigin);
				comp.materialInstance->setFloat("u_TerrainInvSize", 1.0f / span);
			}
		}

		return comp.materialInstanceHandle;
	}

	void TerrainTile::paint(const glm::vec2& localXZ, int layerIndex, float radius, float strength) {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (layerIndex < 1 || layerIndex >= kMaxMaterialLayers || radius <= 0.0f || strength == 0.0f)
			return;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys || !scene.getRegistry().has<TerrainComponent>(entity))
			return;

		TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);

		// Nothing to paint over without a height field; bail before allocating a
		// slice buffer the tile could never show.
		if (comp.heightRes <= 0 || sys->tileSize <= 0.0f)
			return;

		// The material's layer count bounds the slices; a layer it does not have would
		// write past the tile's buffer.
		const int layerCount = terrainLayerCount(scene);
		if (layerIndex >= layerCount)
			return;

		auto data = ensureTerrainDataOf(scene);
		if (!data)
			return;

		TerrainDataTile& tile = tileFor(*data, comp);
		if (tile.weightRes <= 0)
			tile.weightRes = kTerrainWeightRes;
		tile.layerCount = layerCount;

		const int res = tile.weightRes;
		const size_t sliceSize = (size_t)res * res;
		if (tile.weightData.size() != (size_t)tile.layerCount * sliceSize)
			tile.weightData.assign((size_t)tile.layerCount * sliceSize, 0);

		// Now the buffer exists, ensureWeightMap can build the array from it.
		AssetHandle weights = ensureWeightMap(scene, comp, entity, sys->tileSize);
		if (!weights.isValid())
			return;

		const float worldSize = tileWorldSize(comp, *sys);
		if (res <= 0 || worldSize <= 0.0f)
			return;

		const float texPerUnit = (float)res / worldSize;
		// The brush is a world-space ball centred on the surface: each texel's own
		// surface point is measured against that centre, so a stroke reads as a round
		// decal however steep the ground under it is.
		const float centerH = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale,
			localXZ.x, localXZ.y);
		const glm::vec3 center(localXZ.x, centerH, localXZ.y);
		const float rTex = std::max(radius * texPerUnit, 0.5f);

		const int x0 = std::max(0, (int)std::floor(center.x * texPerUnit - rTex));
		const int x1 = std::min(res - 1, (int)std::ceil(center.x * texPerUnit + rTex));
		const int z0 = std::max(0, (int)std::floor(center.z * texPerUnit - rTex));
		const int z1 = std::min(res - 1, (int)std::ceil(center.z * texPerUnit + rTex));

		uint8_t* slice = tile.weightData.data() + (size_t)layerIndex * sliceSize;
		// Additive and clamped: a positive strength paints coverage on, a negative one
		// erases it. Applied after the bake so a stroke is the last writer.
		const float delta = std::clamp(strength, -1.0f, 1.0f);
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
				const float add = f * f * delta * 255.0f;

				uint8_t& slot = slice[(size_t)z * res + x];
				slot = (uint8_t)std::clamp((int)std::lround((float)slot + add), 0, 255);
			}
		}

		comp.bWeightsDirty = true;
		data->bDirty = true;
	}

	bool TerrainTile::raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist,
		glm::vec3& outPoint) {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (!scene.getRegistry().has<TerrainComponent>(entity))
			return false;

		const TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (comp.heightRes <= 0 || comp.heightField.empty())
			return false;

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return false;

		const float dLen = glm::length(dir);
		if (dLen < 1e-6f)
			return false;
		const glm::vec3 d = dir / dLen;

		const float worldSize = tileWorldSize(comp, *sys);
		if (worldSize <= 0.0f)
			return false;

		// Clip the ray to the tile's XZ footprint so the march stays bounded.
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

	glm::vec3 TerrainTile::surfaceNormal(float localX, float localZ) {
		Scene& scene = *m_scene;
		const uint32_t entity = m_entity;

		if (!scene.getRegistry().has<TerrainComponent>(entity))
			return glm::vec3(0.0f, 1.0f, 0.0f);

		const TerrainComponent& comp = scene.getComponent<TerrainComponent>(entity);
		if (comp.heightRes <= 0 || comp.heightField.empty())
			return glm::vec3(0.0f, 1.0f, 0.0f);

		TerrainSystemComponent* sys = findSystemOf(scene);
		if (!sys)
			return glm::vec3(0.0f, 1.0f, 0.0f);

		const float e = sys->tileSize > 0.0f ? sys->tileSize : 1.0f;
		const float hL = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX - e, localZ);
		const float hR = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX + e, localZ);
		const float hD = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX, localZ - e);
		const float hU = sampleSurfaceLocal(comp, sys->tileSize, sys->heightScale, localX, localZ + e);

		return glm::normalize(glm::vec3(hL - hR, 2.0f * e, hD - hU));
	}

}
