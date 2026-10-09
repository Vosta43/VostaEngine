#include "vepch.h"
#include "QuadTreeTerrain.h"
#include "TerrainMeshBuilder.h"
#include "Math/Math.h"

namespace ve {
	Ref<StaticMesh> QuadTreeTerrain::build(const Ref<TextureResource>& heightMap, float tileSize, float heightScale, int maxDepth, int segments, const glm::vec2& worldOrigin) {
		Ref<StaticMeshResource> res = TerrainMeshBuilder::buildMeshResourceFromHeightMap(heightMap, tileSize, heightScale, TerrainMeshBuilder::kDefaultUvTileMeters, worldOrigin);
		int size = (int)sqrt((double)res->vertexBuffer.size());
		segments = (std::max)(1, segments);
		
		// Smallest depth whose root still spans the whole grid, so every tile gets a
		// complete mesh. Sizing the build off the incoming Max Depth instead let the
		// far edge go ungenerated whenever the resolution was not a clean multiple of
		// segments, which reads as a gap between neighbouring tiles.
		int buildDepth = 0;
		while (((size_t)1 << buildDepth) * (size_t)segments < (size_t)(size - 1))
			++buildDepth;
		maxDepth = buildDepth;
		mMaxDepth = maxDepth; // remember the depth we actually built

		std::vector<int> indices;
		mChunks.clear();

		for (int d = 0;d <= maxDepth;d++) {
			int stride = 1 << (maxDepth - d);
			int nodeSpan = stride * segments;
			int perSideNodeCount = 1 << d;
			float nodeWorldSize = nodeSpan * tileSize;
			
			for (int cz = 0;cz < perSideNodeCount;cz ++) {
				for (int cx = 0;cx < perSideNodeCount;cx++) {
					int x0 = cx * nodeSpan;
					int z0 = cz * nodeSpan;

					uint32_t firstIndex = (uint32_t)indices.size();
					generateNode(x0, z0, stride, size, segments, indices);
					ChunkRange range;
					range.firstIndex = firstIndex;
					range.indexCount = (uint32_t)indices.size() - firstIndex;
					range.centerWorld = worldOrigin + glm::vec2((x0 + nodeSpan * 0.5) * tileSize, (z0 + nodeSpan * 0.5) * tileSize);
					range.worldSize = nodeWorldSize;
					mChunks.push_back(range);
				}
			}
		}


		res->indexBuffer.assign(indices.begin(), indices.end());
		return StaticMesh::create(res);
	}

	void QuadTreeTerrain::generateNode(int x0, int z0, int stride, int size, int segments, std::vector<int>& indices) {
		// The last tile's node grid usually overhangs the vertex grid; clamp each quad
		// back to the final row/column so the mesh ends exactly on the edge instead of
		// indexing past the vertex buffer.
		const int last = size - 1;
		for (int qx = 0; qx < segments; qx++) {
			const int i = x0 + qx * stride; // grid row (x)
			if (i >= last) break;
			const int i1 = (std::min)(i + stride, last);
			for (int qz = 0; qz < segments; qz++) {
				const int j = z0 + qz * stride; // grid col (z)
				if (j >= last) break;
				const int j1 = (std::min)(j + stride, last);

				int leftUp    = i * size + j;
				int rightUp   = i * size + j1;
				int leftDown  = i1 * size + j;
				int rightDown = i1 * size + j1;

				indices.push_back(leftUp);
				indices.push_back(rightUp);
				indices.push_back(leftDown);
				indices.push_back(rightUp);
				indices.push_back(rightDown);
				indices.push_back(leftDown);
			}
		}
	}

	size_t QuadTreeTerrain::getChunkIndex(int depth, int x, int z) {
		size_t perSide = size_t(1) << depth;
		size_t offset = ((size_t(1) << (2 * depth)) - 1) / 3;
		return offset + size_t(z) * perSide + size_t(x);
	}

	const std::vector<QuadTreeTerrain::ChunkRange>& QuadTreeTerrain::update(const glm::vec3& cameraPos, int maxDepth, float lodDetail, float renderDistance) {
		mActives.clear();
		if (mChunks.empty()) return mActives; 
		maxDepth = (std::min)(maxDepth, mMaxDepth); // never split deeper than build() generated
		walk(0, 0, 0, maxDepth, cameraPos, lodDetail, renderDistance);

		// Node ranges tile the shared index buffer back to back, so neighbouring
		// chunks draw contiguous indices. Coalescing them collapses a fully
		// subdivided tile into one draw instead of one per chunk.
		std::sort(mActives.begin(), mActives.end(),
			[](const ChunkRange& a, const ChunkRange& b) { return a.firstIndex < b.firstIndex; });
		size_t kept = 0;
		for (size_t i = 0; i < mActives.size(); ++i) {
			if (kept > 0 &&
				mActives[kept - 1].firstIndex + mActives[kept - 1].indexCount == mActives[i].firstIndex)
				mActives[kept - 1].indexCount += mActives[i].indexCount;
			else
				mActives[kept++] = mActives[i];
		}
		mActives.resize(kept);

		return mActives;
	}

	void QuadTreeTerrain::walk(int depth, int x, int z, int maxDepth, const glm::vec3& cameraPos, float lodDetail, float renderDistance) {
		const ChunkRange& node = mChunks[getChunkIndex(depth, x, z)];
		float dist = glm::distance(glm::vec2(cameraPos.x, cameraPos.z), node.centerWorld);

		// Too far: drop this node (distance cull).
		if (dist > renderDistance) return;

		// Close enough and can still split: recurse into the 4 children.
		if (depth < maxDepth && dist < node.worldSize * lodDetail) {
			walk(depth + 1, 2 * x,     2 * z,     maxDepth, cameraPos, lodDetail, renderDistance);
			walk(depth + 1, 2 * x + 1, 2 * z,     maxDepth, cameraPos, lodDetail, renderDistance);
			walk(depth + 1, 2 * x,     2 * z + 1, maxDepth, cameraPos, lodDetail, renderDistance);
			walk(depth + 1, 2 * x + 1, 2 * z + 1, maxDepth, cameraPos, lodDetail, renderDistance);
		} else {
			// Far enough: emit this node's index range.
			mActives.push_back(node);
		}
	}

}

