#pragma once

#include "Geometry/GeometryGraph.h"

#include <cstdint>

namespace ve {

	// Lets a geometry graph load through ResourceManager like any other asset. The
	// on-disk header ("geometry" token + a version int) matches GeometryGraphPanel's
	// reader, so both sides read the same files.
	struct VE_API GeometryGraphResource {
		GeometryGraph graph;
		// Hash of the file bytes, so a dependent mesh can key on the graph's content.
		uint64_t contentHash = 0;

		static Ref<GeometryGraphResource> create(const std::string& path);

		// Reload a registered graph from disk in place, refreshing its content hash.
		// No-op when the path is not currently loaded.
		static void refreshFromFile(const std::string& path);
	};

}
