#pragma once

#include "Terrain/RuleGraph.h"

#include <cstdint>

namespace ve {

	// Lets a rule graph load through ResourceManager like any other asset, so a
	// layered material can reference one by handle. The on-disk header ("terrain_rule"
	// token + a version int) matches TerrainRulePanel's writer, so both sides agree.
	struct VE_API TerrainRuleGraphResource {
		RuleGraph graph;
		// Hash of the file bytes, so the terrain can key a re-bake on the graph's content.
		uint64_t contentHash = 0;

		static Ref<TerrainRuleGraphResource> create(const std::string& path);

		// Reload a registered graph from disk in place, refreshing its content hash.
		// No-op when the path is not currently loaded.
		static void refreshFromFile(const std::string& path);
	};

}
