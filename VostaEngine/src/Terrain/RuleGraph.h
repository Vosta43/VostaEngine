#pragma once

#include "Noise/NoiseGraph.h"
#include "Scene/Archive.h"

#include <glm.hpp>
#include <cstdint>
#include <vector>

namespace ve {

	// Everything a rule node reads about the surface it is evaluated on. The height
	// grid uses the terrain's [x * heightRes + z] layout in raw normalized units
	// (world Y = value * heightScale). `sample` is the world XZ of the texel being
	// evaluated, so a field read here is seamless across tile borders.
	struct RuleEvalContext {
		const float* heightGrid = nullptr;
		int          heightRes  = 0;
		float        tileSize   = 1.0f;              // world metres per height-grid step
		float        heightScale = 1.0f;
		glm::vec2    worldOrigin = glm::vec2(0.0f);  // world XZ of grid (0, 0)
		glm::vec2    sample      = glm::vec2(0.0f);  // world XZ being evaluated
	};

	// A noise node that additionally knows the surface under it, so it can read
	// terrain attributes. The default forwards to the position-only evaluate, which
	// is what a rule graph's math nodes (which ignore position) want.
	//
	// prepare() runs once per plan build, on the building thread, before any parallel
	// evaluation; a node that resolves an external asset caches the result there so
	// the per-texel walk stays a pure read.
	class VE_API RuleNode : public NoiseNode {
	public:
		virtual void prepare() const {}

		virtual float evaluateRule(const RuleEvalContext& ctx, const std::vector<float>& inputs) const {
			return evaluate(inputs, ctx.sample.x, ctx.sample.y);
		}

		// A terminal routes its input into layer weights rather than feeding another
		// node. contributeWeights merges (max) into out[0..maxOut-1] for layers
		// 1..maxOut, taking the already-evaluated input pin values. A non-terminal
		// overrides neither, so the plan skips it.
		virtual bool isTerminal() const { return false; }
		virtual void contributeWeights(const RuleEvalContext&, const std::vector<float>&,
			float* /*out*/, int /*maxOut*/) const {}
	};

	// Serialization tags for the rule nodes. A rule graph is standalone — it holds no
	// noise operators — so this is its own small tag space.
	enum class RuleNodeTag : uint8_t {
		FieldSource = 0,
		LayerBands  = 1,
	};

	const NodeRegistry& getRuleNodeRegistry();

	using RuleGraph = NodeGraph;

	// Value of one node at a texel: evaluateRule when the node wants the surface
	// context, the position-only evaluate otherwise.
	inline float evaluateRuleNode(const NoiseNode& node, const RuleEvalContext& ctx,
		const std::vector<float>& inputs) {
		if (const auto* rn = dynamic_cast<const RuleNode*>(&node))
			return rn->evaluateRule(ctx, inputs);
		return node.evaluate(inputs, ctx.sample.x, ctx.sample.y);
	}

	// Persistence helpers, so callers never touch the tag registry directly.
	VE_API void serializeRuleGraph(const RuleGraph& graph, Archive& ar);
	VE_API void deserializeRuleGraph(RuleGraph& graph, Archive& ar);

}
