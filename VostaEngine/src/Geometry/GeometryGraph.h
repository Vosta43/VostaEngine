#pragma once

#include "Graph/NodeGraph.h"
#include "Geometry/GeometryBuffer.h"
#include "Sdf/SdfValue.h"

#include <cstdint>
#include <vector>

namespace ve {

	// One resolved value per input pin. A Geometry pin carries an upstream buffer
	// (null when unconnected), an Sdf pin a signed-distance function (empty when
	// unconnected); a scalar pin carries a float already resolved from the node's
	// inputDefault() when no wire feeds it.
	struct GeometryEvalInput {
		Ref<const GeometryBuffer> geometry;
		SdfValue                  sdf;
		float                     scalar = 0.0f;
	};

	struct GeometryEvalContext {
		std::vector<GeometryEvalInput> inputs;
		// Reserved: a graph-level seed for future nodes. M1 nodes carry their own
		// noise seed, so the evaluator leaves this at 0.
		int seed = 0;
	};

	// A node that produces a whole geometry buffer when evaluated. Unlike NoiseNode,
	// which is evaluated once per sample, a geometry node runs once per graph
	// evaluation, so the evaluator just topological-sorts and caches one buffer per
	// node (no per-sample flattening plan).
	class VE_API GeometryNode : public GraphNode {
	public:
		virtual Ref<const GeometryBuffer> evaluate(const GeometryEvalContext& ctx) const { return nullptr; }
	};

	enum class GeometryNodeTag : uint8_t {
		Icosphere     = 0,
		Subdivide     = 1,
		NoiseDisplace = 2,
		Transform     = 3,
		MeshOutput    = 4,
		FieldDisplace = 5,
		SdfSphere     = 6,
		MarchingCubes = 7,
		SdfNoiseDisplace = 8,
		SdfBoolean       = 9,
	};

	const NodeRegistry& getGeometryNodeRegistry();

	using GeometryGraph = NodeGraph;

	VE_API void serializeGeometryGraph(const GeometryGraph& graph, Archive& ar);
	VE_API void deserializeGeometryGraph(GeometryGraph& graph, Archive& ar);

	// The single MeshOutput terminal of a geometry graph, or null while none exists.
	VE_API GeometryNode* findGeometryOutput(GeometryGraph& graph);

}
