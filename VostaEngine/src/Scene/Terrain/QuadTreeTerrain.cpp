#include "vepch.h"
#include "QuadTreeTerrain.h"
#include "TerrainMeshBuilder.h"
#include "Math/Math.h"

namespace ve {
	Ref<StaticMesh> QuadTreeTerrain::build(const Ref<TextureResource>& heightMap, float tileSize, float heightScale, int maxDepth, int segments) {
		Ref<StaticMeshResource> res = TerrainMeshBuilder::buildMeshResourceFromHeightMap(heightMap, tileSize, heightScale);
		int size = (int)sqrt((double)res->vertexBuffer.size());
		segments = (std::max)(1, segments);
		
		int depthBySegments = 0;
		for(int room = size / segments; room > 1; room >>= 1)
			depthBySegments++;
		maxDepth = math::clamp(maxDepth,0,depthBySegments);
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
					range.centerWorld = glm::vec2((x0 + nodeSpan * 0.5) * tileSize, (z0 + nodeSpan * 0.5) * tileSize);
					range.worldSize = nodeWorldSize;
					mChunks.push_back(range);
				}
			}
		}


		res->indexBuffer.assign(indices.begin(), indices.end());
		return StaticMesh::create(res);
	}

	void QuadTreeTerrain::generateNode(int x0, int z0, int stride, int size, int segments, std::vector<int>& indices) {
		for (int qx = 0; qx < segments; qx++) {
			for (int qz = 0; qz < segments; qz++) {
				int i = x0 + qx * stride; // grid row (x)
				int j = z0 + qz * stride; // grid col (z)

				int leftUp    = i * size + j;
				int rightUp   = i * size + j + stride;
				int leftDown  = (i + stride) * size + j;
				int rightDown = (i + stride) * size + j + stride;

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

