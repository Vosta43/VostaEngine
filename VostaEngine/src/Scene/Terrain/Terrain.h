#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"
#include "Scene/Terrain/TerrainDataResource.h"

#include <glm.hpp>
#include <cstdint>
#include <unordered_set>

namespace ve {

	struct TerrainComponent;
	struct TerrainSystemComponent;
	// Forward-declared: Components.h includes this header for its VECOMPONENTINIT
	// hook, so including Scene.h here would close the cycle.
	class Scene;

	// Scene-scoped terrain service (ULandscapeSubsystem). Stateless: the shared
	// parameters, tile set and painted weights all live in the scene, so a handle
	// is cheap and callers build one where they need it.
	class VE_API TerrainSubsystem {
	public:
		explicit TerrainSubsystem(Scene& scene) : m_scene(&scene) {}

		TerrainSystemComponent* system();      // null when the scene has none
		uint32_t ensureSystem();               // creates one; idempotent
		Ref<TerrainDataResource> data();       // painted weights, or null
		Ref<TerrainDataResource> ensureData(); // creates an empty one on demand

		enum class TileDir { Left, Right, Up, Down };
		static glm::ivec2 tileStep(TileDir dir);

		uint32_t findTile(const glm::ivec2& coord);   // UINT32_MAX when there is none
		uint32_t neighbor(uint32_t entity, TileDir dir);

		uint32_t addTile(const glm::ivec2& coord);
		bool     removeTile(const glm::ivec2& coord);
		// Inclusive rectangle in one occupancy pass; a per-cell findTile scan would
		// go quadratic.
		void     addTiles(const glm::ivec2& minCoord, const glm::ivec2& maxCoord);
		void     removeTiles(const glm::ivec2& minCoord, const glm::ivec2& maxCoord);

		// Reconcile the live tile window against the system's stream settings, per
		// frame. activeCoords holds every tile this streamer created and is the only
		// thing destroyed -- an author-placed tile is left alone. With
		// bDistanceStreaming off it retracts any window still out. Returns tiles made.
		int streamUpdate(std::unordered_set<int64_t>& activeCoords,
			const glm::vec3& viewpoint, const TerrainSystemComponent& sys);

		void fillWeight(int layerIndex);
		void clearWeight(int layerIndex);

		// Paint one brush dab at a world-space point into every tile the brush
		// overlaps. Adjacent tiles share a texel grid, so the mask agrees on both
		// sides and a stroke has no seam at a border.
		void paintWeightWorld(uint32_t referenceTile, const glm::vec3& worldPoint,
			int layerIndex, float radius, float strength);

		// Nearest terrain hit across all tiles: lets a brush find whatever tile is
		// under the cursor without a prior selection.
		bool raycastScene(const glm::vec3& origin, const glm::vec3& dir, float maxDist,
			uint32_t& outEntity, glm::vec3& outWorldPoint);

		// Commit the background builds finished since the last call; once per frame,
		// before collecting meshes.
		int pumpBuilds();

	private:
		Scene* m_scene;
	};

	// One terrain tile: a handle over a tile entity (ALandscapeProxy /
	// ULandscapeComponent). Cheap to build; each call re-resolves the scene's shared
	// parameters through its TerrainSystemComponent.
	class VE_API TerrainTile {
	public:
		TerrainTile(Scene& scene, uint32_t entity) : m_scene(&scene), m_entity(entity) {}

		uint32_t id() const { return m_entity; }
		bool valid() const;

		// World extent one tile covers and the world-space XZ of its local (0,0)
		// corner. Before a bake the height field is empty, so the system's noise
		// resolution stands in.
		static float tileWorldSize(const TerrainComponent& comp, const TerrainSystemComponent& sys);
		static glm::vec2 tileOrigin(const TerrainComponent& comp, const TerrainSystemComponent& sys);

		float worldSize();
		glm::vec2 origin();

		void place();
		void setCoord(const glm::ivec2& coord);
		void snapToGrid();

		// Make the tile drawable: the noise graph wins when the system has one, then
		// the tile's heightmap, then a flat placeholder. No-op without a system.
		void ensureBuilt();
		// As ensureBuilt, but a noise-graph tile is generated on worker threads and
		// committed later by pumpBuilds. Heightmap and flat tiles stay synchronous.
		void ensureBuiltAsync();
		void rebuildFromHeightMap();
		void rebuildFromNoiseGraph();

		// Bake the layer weights from the system's rule graph, or from the material
		// layers' own scalars when there is none. Keyed, so it runs once per edit.
		void bakeWeights();
		// Upload the tile's CPU weight data to its control-map texture if it changed.
		bool flushWeightMap();
		// Build (or rebase) the per-instance material view carrying this tile's control
		// map. worldOrigin is the tile's world-space XZ placement.
		AssetHandle ensureMaterialInstance(const glm::vec2& worldOrigin);

		// Add layerIndex's weight (1..layerCap-1) inside a radial brush centred on a
		// terrain-local XZ, clamping to 0..1. Negative strength erases.
		void paint(const glm::vec2& localXZ, int layerIndex, float radius, float strength);

		// March a ray against the tile's CPU height field, in terrain-local space.
		bool raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist,
			glm::vec3& outPoint);
		// From central differences on the height field; straight up without one.
		glm::vec3 surfaceNormal(float localX, float localZ);

	private:
		Scene* m_scene;
		uint32_t m_entity;
	};

}
