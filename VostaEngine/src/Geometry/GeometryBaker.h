#pragma once

#include "Geometry/GeometryGraph.h"

#include <string>

namespace ve {

	// Evaluates the graph to a mesh, writes it as a binary "staticmesh" .veasset, then
	// refreshes the loaded GPU mesh and its thumbnail. Re-baking the same path
	// overwrites it. Runs on the main thread (touches GL). Returns false with a reason
	// in *outError on failure.
	VE_API bool bakeGeometryGraphToAsset(const GeometryGraph& graph,
	                                     const std::string& outAssetPath,
	                                     std::string* outError = nullptr);

}
