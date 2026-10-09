#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"
#include "Scene/Terrain/TerrainDataResource.h"

#include <glm.hpp>
#include <cstdint>

namespace ve {

	struct TerrainComponent;
	struct TerrainSystemComponent;
	// Forward-declared, not included: Components.h includes this header for its
	// VECOMPONENTINIT hook, so including Scene.h here would close the cycle.
	class Scene;

	// The single engine-side entry point for terrain assets. Vertex data lives
	// in TerrainMeshBuilder; the "__terrain/<id>" registration bookkeeping lives
	// here. Editor panels and other callers go through these functions instead of
	// touching ResourceManager or the mesh builder directly.
	//
	// The scene's TerrainSystem owns every shared parameter (source, material,
	// LOD, tileSize/heightScale); a tile carries only its own heightmap and
	// coordinate. Every function that needs a shared parameter resolves the
	// system first and no-ops when there is none, so a tile without a system is
	// simply not drawn.
	namespace Terrain {

		// --- Shared parameters ---

		// The scene's terrain system, or null when none exists.
		VE_API TerrainSystemComponent* findSystem(Scene& scene);

		// The scene's terrain system, creating a Name+TerrainSystem entity when
		// there is none. Idempotent. Creating an entity reallocates component
		// storage, so a caller must not hold a component pointer across this call.
		VE_API uint32_t ensureSystem(Scene& scene);

		// The scene's terrain data (painted weights), resolved from the system's
		// handle, or null when the system has none and nothing is cached yet.
		VE_API Ref<TerrainDataResource> findTerrainData(Scene& scene);

		// As above, but creates an empty in-memory resource when there is no handle
		// yet, so painting works before the scene is first saved.
		VE_API Ref<TerrainDataResource> ensureTerrainData(Scene& scene);

		// World extent one tile covers along one axis, and the spacing of the tile
		// grid. Before a bake the height field is empty, so the system's noise
		// resolution stands in as the extent a pending noise bake will produce.
		VE_API float tileWorldSize(const TerrainComponent& comp, const TerrainSystemComponent& sys);

		// World-space XZ of this tile's local (0,0) corner.
		VE_API glm::vec2 tileOrigin(const TerrainComponent& comp, const TerrainSystemComponent& sys);

		// Make a tile drawable from the system's shared settings: the noise graph
		// wins when the system has one, then this tile's heightmap, then a flat
		// placeholder. No-op without a system, which leaves a tile invisible.
		// Cheap when already built and clean, so the renderer calls it every frame.
		VE_API void ensureBuilt(Scene& scene, uint32_t entity);

		// (Re)build the mesh from this tile's heightmap. No-op without a system or
		// a heightmap.
		VE_API void rebuildFromHeightMap(Scene& scene, uint32_t entity);

		// (Re)build the mesh by baking the system's noise graph to a float height
		// field in memory. No-op without a system or a graph.
		VE_API void rebuildFromNoiseGraph(Scene& scene, uint32_t entity);

		// Build (or rebase) the per-instance material view that carries this tile's
		// control map, and return its handle. Cheap to call every frame: it only
		// touches the resource manager when the system's base material changed.
		// worldOrigin is the tile's world-space XZ placement, needed so the painted
		// control map lines up with the drawn surface.
		VE_API AssetHandle ensureMaterialInstance(Scene& scene, uint32_t entity,
			const glm::vec2& worldOrigin);

		// Upload the tile's CPU weight data to its control-map texture if it changed
		// since the last call. Returns true when an upload happened.
		VE_API bool flushWeightMap(Scene& scene, uint32_t entity);

		// March a ray, in terrain-local space, against the CPU height field.
		// Returns true and the surface point (local space) on a hit. Always false
		// for a terrain with no height field (the flat placeholder) or no system.
		VE_API bool raycast(Scene& scene, const TerrainComponent& comp, const glm::vec3& origin,
			const glm::vec3& dir, float maxDist, glm::vec3& outPoint);

		// Raycast every terrain tile in the scene and return the nearest hit: the
		// tile entity and the world-space surface point. False when the ray misses
		// every tile or the scene has no system. Lets a brush find whatever tile is
		// under the cursor without a prior selection.
		VE_API bool raycastScene(Scene& scene, const glm::vec3& origin, const glm::vec3& dir,
			float maxDist, uint32_t& outEntity, glm::vec3& outWorldPoint);

		// Surface normal at a terrain-local XZ position, from central differences
		// on the height field. Local space, which equals world space for the
		// translation-only transforms the terrain mapping assumes. Falls back to
		// straight up when there is no height field or no system.
		VE_API glm::vec3 surfaceNormal(Scene& scene, const TerrainComponent& comp,
			float localX, float localZ);

		// Raise layerIndex's weight (1..3 -> control-map r,g,b) inside a radial
		// brush centred on a terrain-local XZ position. Strength caps the weight
		// the brush writes (1 = up to full). Falloff is white-centre to black-edge.
		VE_API void paintWeight(Scene& scene, uint32_t entity, const glm::vec2& localXZ,
			int layerIndex, float radius, float strength);

		// Paint one brush dab at a world-space point into EVERY tile the brush
		// overlaps. referenceTile supplies the weight resolution and tile extent
		// (equal across a system's tiles); adjacent tiles share a texel grid, so the
		// world-derived mask agrees on both sides and the stroke has no seam at a
		// tile border. No-op without a system.
		VE_API void paintWeightWorld(Scene& scene, uint32_t referenceTile, const glm::vec3& worldPoint,
			int layerIndex, float radius, float strength);

		// Set layerIndex's weight to full across every tile, restoring the
		// procedural layer rules the mask was hiding.
		VE_API void fillWeight(Scene& scene, int layerIndex);

		// Zero layerIndex's weight across every tile, so only the layers the other
		// channels cover remain.
		VE_API void clearWeight(Scene& scene, int layerIndex);

		// --- Tile set ---
		// Which side of a tile to look at, in grid steps.
		enum class TileDir { Left, Right, Up, Down };

		// The coordinate offset one grid step in dir takes.
		VE_API glm::ivec2 tileStep(TileDir dir);

		// The tile at coord, or UINT32_MAX when there is none.
		VE_API uint32_t findTile(Scene& scene, const glm::ivec2& coord);

		// The tile one grid step in dir from entity, or UINT32_MAX.
		VE_API uint32_t neighbor(Scene& scene, uint32_t entity, TileDir dir);

		// Spawn a tile at coord with the default source. It bakes lazily through the
		// renderer's ensureBuilt, so a system created after it is still picked up.
		VE_API uint32_t addTile(Scene& scene, const glm::ivec2& coord);

		// Destroy the tile at coord; false when there was none.
		VE_API bool removeTile(Scene& scene, const glm::ivec2& coord);

		// Batch variants over an inclusive coordinate rectangle: fill a whole
		// region in one pass instead of one findTile scan per cell. A grid editor
		// painting dozens of tiles at once would otherwise go quadratic.
		VE_API void addTiles(Scene& scene, const glm::ivec2& minCoord, const glm::ivec2& maxCoord);
		VE_API void removeTiles(Scene& scene, const glm::ivec2& minCoord, const glm::ivec2& maxCoord);

		// Write the tile's world XZ from its tileCoord. A tile with no extent yet
		// (nothing baked) is left where it is.
		VE_API void placeTile(Scene& scene, uint32_t entity);

		// Move the tile's coordinate and re-place it. The coordinate is part of the
		// build key, so the mesh re-bakes next frame.
		VE_API void setTileCoord(Scene& scene, uint32_t entity, const glm::ivec2& coord);

		// Round the entity's world XZ onto the nearest grid coordinate.
		VE_API void snapToGrid(Scene& scene, uint32_t entity);

	}

}
