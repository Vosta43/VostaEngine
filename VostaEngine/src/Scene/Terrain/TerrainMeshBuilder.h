#pragma once

#include "Renderer/StaticMesh.h"
#include "Renderer/Texture.h"
#include "Asset/TextureResource.h"

#include <glm.hpp>

namespace ve {

	class VE_API TerrainMeshBuilder {
	public:

		// World size one texture repeat covers; the default for every uvTileMeters
		// below, and named so callers forced to pass it stay in step with the default.
		static constexpr float kDefaultUvTileMeters = 8.0f;

		// uvTileMeters is the world size (metres) covered by one texture repeat.
		// UVs are derived from world XZ (worldXZ / uvTileMeters) rather than
		// normalised to 0..1, which is what lets the sampler tile the texture and
		// keeps quad-tree LOD chunks seamless. worldOrigin is the tile's world-space
		// corner, so UVs stay continuous across a tile set instead of restarting at
		// every tile's own local zero.
		static Ref<StaticMesh> buildMeshFromHeightMap(const Ref<TextureResource>& heightMap,float tileSize = 1.0f,float heightScale = 1.0f,float uvTileMeters = kDefaultUvTileMeters,const glm::vec2& worldOrigin = glm::vec2(0.0f));
		// Same vertex math as buildMeshFromHeightMap, but returns the raw resource
		// (vertices only) so a caller like QuadTreeTerrain can keep the shared
		// full-res vertex buffer while supplying its own index buffer.
		static Ref<StaticMeshResource> buildMeshResourceFromHeightMap(const Ref<TextureResource>& heightMap, float tileSize = 1.0f, float heightScale = 1.0f, float uvTileMeters = kDefaultUvTileMeters, const glm::vec2& worldOrigin = glm::vec2(0.0f));
		// Build meshes from height map (blocks stands for 1x1 2x2 etc.)
		static std::vector<Ref<StaticMesh>> buildMeshesFromHeightMap(const Ref<TextureResource>& heightMap, float tileSize = 1.0f, float heightScale = 1.0f,int blocks = 1);

		// A flat grid in the XZ plane starting at local (0,0) (y = 0, normals up).
		// The starting surface for a terrain with no heightmap yet; same corner
		// origin as the heightmap path so the two cover the same footprint.
		static Ref<StaticMeshResource> buildFlatMeshResource(float worldSize, int segments, float uvTileMeters = kDefaultUvTileMeters, const glm::vec2& worldOrigin = glm::vec2(0.0f));
		static Ref<StaticMesh> buildFlatMesh(float worldSize, int segments, float uvTileMeters = kDefaultUvTileMeters, const glm::vec2& worldOrigin = glm::vec2(0.0f));
	private:

	};

}
