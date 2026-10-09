#pragma once

#include "Core/Core.h"
#include "Core/AssetHandle.h"

#include <glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ve {

	// Painted per-layer control maps for one terrain, keyed by tile coordinate.
	// RGB = layers 1..3 (layer 0 is the residual base). This is the CPU master;
	// each tile's GPU control map is uploaded from the buffer here.
	struct TerrainDataTile {
		int weightRes = 0;                  // 0 until first painted or loaded
		std::vector<uint8_t> weightData;    // res*res*3, interleaved
	};

	// A terrain's painted weights as a standalone asset, so a scene references
	// them instead of carrying the (large) buffers inline. On disk: binary, token
	// "terrain_data", version 1, then non-empty tiles only.
	struct VE_API TerrainDataResource {
		std::unordered_map<int64_t, TerrainDataTile> tiles;  // key = pack(coord)
		bool bDirty = false;                                 // runtime-only, not serialized

		static int64_t pack(const glm::ivec2& c) {
			return ((int64_t)c.x << 32) ^ (int64_t)(uint32_t)c.y;
		}

		TerrainDataTile& tile(const glm::ivec2& c);   // create if absent
		TerrainDataTile* find(const glm::ivec2& c);   // null when absent
		void             erase(const glm::ivec2& c);
		bool             anyPainted() const;          // any non-zero byte

		void write(const std::string& path) const;

		static Ref<TerrainDataResource> create(const std::string& path);
	};

}
