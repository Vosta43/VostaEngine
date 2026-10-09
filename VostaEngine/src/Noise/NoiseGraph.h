#pragma once

#include "Graph/NodeGraph.h"

#include <cstdint>
#include <vector>

namespace ve {

	// A noise node additionally knows how to produce a scalar at a world (x, y).
	// Concrete kinds live in NoiseNodes.h; this base is what the editor schema and
	// the evaluator dynamic_cast against. Mirrors MaterialNode's role for materials.
	class VE_API NoiseNode : public GraphNode {
	public:
		// inputs holds the evaluated value of each input pin in pin order, and 0 for
		// an unconnected pin, so a node never has to walk the graph itself.
		virtual float evaluate(const std::vector<float>& inputs, float x, float y) const { return 0.0f; }
	};

	// Serialization tags — one per concrete NoiseNode subclass.
	enum class NoiseNodeTag : uint8_t {
		NoiseUnit = 0,
		Add       = 1,
		Subtract  = 2,
		Multiply  = 3,
		Divide    = 4,
		Output    = 5,
		Constant  = 6,
		Blend     = 7,
		Min       = 8,
		Max       = 9,
		Invert    = 10,
		Clamp     = 11,
		Remap     = 12,
		Threshold = 13,
		Abs       = 14,
		Erf       = 15,
		Trunc     = 16,
		Spline    = 17,
	};

	const NodeRegistry& getNoiseNodeRegistry();

	using NoiseGraph = NodeGraph;

	// Evaluates the graph's Output terminal at world (x, y). Returns 0 when the
	// graph has no Output or the input chain is incomplete.
	VE_API float evaluateNoiseGraph(const NoiseGraph& graph, float x, float y);

	// Persistence helpers, so callers never touch the tag registry directly.
	VE_API void serializeNoiseGraph(const NoiseGraph& graph, Archive& ar);
	VE_API void deserializeNoiseGraph(NoiseGraph& graph, Archive& ar);

}
