#pragma once

#include "Noise/NoiseGraph.h"

#include <cstdint>

namespace ve {

	// Lets a noise graph load through ResourceManager like any other asset, so a
	// terrain can reference one by handle. The on-disk header ("noise" token + a
	// version int) matches NoisePanel's reader, so both sides read the same files.
	struct VE_API NoiseGraphResource {
		NoiseGraph graph;
		// Hash of the file bytes, so a dependent mesh can key on the graph's content.
		uint64_t contentHash = 0;

		static Ref<NoiseGraphResource> create(const std::string& path);

		// Reload a registered graph from disk in place, refreshing its content hash.
		// No-op when the path is not currently loaded.
		static void refreshFromFile(const std::string& path);
	};

}
