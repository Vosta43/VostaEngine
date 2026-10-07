#pragma once

#include "Core/Core.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Texture.h"
#include "Asset/TextureResource.h"

#include <cstdint>
#include <vector>

namespace ve {

	// Quadtree terrain LOD. Reuses TerrainMeshBuilder for the shared full-res
	// vertex buffer; this class only owns subdivision (per-node index ranges
	// into one master index buffer) and the per-frame update (LOD selection).
	// The mesh never changes after build(); LOD just picks which node ranges to
	// draw via StaticMesh::draw(startIndex, indexCount).
	class VE_API QuadTreeTerrain {
	public:

		// One node's draw range into the master index buffer, plus the world-space
		// info the LOD walk needs to test camera distance against node size.
		struct ChunkRange {
			uint32_t firstIndex = 0;
			uint32_t indexCount = 0;
			glm::vec2 centerWorld = glm::vec2(0.0f);
			float worldSize = 0.0f; // node edge length in world units
		};

		// Build the shared mesh plus every node's index range. Reuses
		// TerrainMeshBuilder for the vertices. Call once when the heightmap or
		// terrain parameters change.
		Ref<StaticMesh> build(const Ref<TextureResource>& heightMap, float tileSize,
			float heightScale, int maxDepth, int segments);

		// Re-pick the active LOD chunks for this frame's camera. Returns the list
		// of ranges to draw, all referencing the mesh returned by build().
		const std::vector<ChunkRange>& update(const glm::vec3& cameraPos, int maxDepth,
			float lodDetail, float renderDistance);

	private:

		// Append one node's triangles (segments x segments quads at stride) into
		// `indices`.
		void generateNode(int x0, int z0, int stride, int size, int segments,
			std::vector<int>& indices);

		// Recursive LOD walk; emit the kept ranges into mActives.
		void walk(int depth, int x, int z, int maxDepth, const glm::vec3& cameraPos,
			float lodDetail, float renderDistance);

		// Flat index of node (depth, x, z): nodes above it + z*perSide + x.
		static size_t getChunkIndex(int depth, int x, int z);

		int mMaxDepth = 0;                 // depth actually built; update() clamps to it
		std::vector<ChunkRange> mChunks;   // one range per node, flat quadtree order
		std::vector<ChunkRange> mActives;  // ranges selected by the last update()
	};

}
