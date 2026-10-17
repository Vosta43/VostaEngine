#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"

#include <glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ve {

	// Per-layer weight slices for one terrain, keyed by tile coordinate. One R8
	// slice per material layer: slice i is layer i's weight and slice 0 is unused
	// (layer 0 is the residual base). Slice-major, so the GPU texture array
	// uploads straight through: weightData[layer * res*res + z*res + x]. This is
	// the CPU master; each tile's GPU array is uploaded from the buffer here.
	struct TerrainDataTile {
		int weightRes = 0;                  // 0 until first written or loaded
		int layerCount = 0;                 // slices; 0 until first written or loaded
		std::vector<uint8_t> weightData;    // layerCount * res*res, slice-major
		// Belongs to a streamed tile, so it is kept out of the sidecar: a window
		// follows the camera, and what it happened to cover at save time is not
		// authored data. Runtime-only, never written and never read back.
		bool bTransient = false;
	};

	// A terrain's baked/painted weights as a standalone asset, so a scene
	// references them instead of carrying the (large) buffers inline. On disk:
	// binary, token "terrain_data", version 2, then non-empty tiles only.
	struct VE_API TerrainDataResource {
		std::unordered_map<int64_t, TerrainDataTile> tiles;  // key = pack(coord)
		bool bDirty = false;                                 // runtime-only, not serialized

		static int64_t pack(const glm::ivec2& c) {
			return ((int64_t)c.x << 32) ^ (int64_t)(uint32_t)c.y;
		}

		static glm::ivec2 unpack(int64_t k) {
			return glm::ivec2((int32_t)(k >> 32), (int32_t)(uint32_t)k);
		}

		TerrainDataTile& tile(const glm::ivec2& c);   // create if absent
		TerrainDataTile* find(const glm::ivec2& c);   // null when absent
		void             erase(const glm::ivec2& c);
		bool             anyPainted() const;          // any non-zero byte

		void write(const std::string& path) const;

		static Ref<TerrainDataResource> create(const std::string& path);
	};

}
