#pragma once

#include "Geometry/GeometryGraph.h"
#include "Asset/StaticMeshResource.h"

#include <string>

namespace ve {

	// Runs the whole graph once per node (not per sample), caching one buffer per node
	// id. Returns the buffer feeding the Mesh Output terminal, or null with a
	// human-readable reason in *outError.
	VE_API Ref<const GeometryBuffer> evaluateGeometryGraphBuffer(const GeometryGraph& graph,
	                                                             std::string* outError = nullptr);

	// Packs the evaluated buffer into an asset-layer mesh (single submesh, no material).
	VE_API Ref<StaticMeshResource> toMeshResource(const GeometryBuffer& buffer,
	                                              const std::string& subMeshName = "geometry");

	// Convenience: buffer -> mesh, naming the submesh after the Mesh Output node.
	VE_API Ref<StaticMeshResource> evaluateGeometryGraph(const GeometryGraph& graph,
	                                                     std::string* outError = nullptr);

}
