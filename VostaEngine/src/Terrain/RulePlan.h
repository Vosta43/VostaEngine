#pragma once

#include "Terrain/RuleGraph.h"
#include "Noise/NoisePlan.h"

#include <vector>

namespace ve {

	// Same shape as the noise plan's input edge: an earlier slot's output, or a
	// literal baked from the node's own default when the pin is unconnected.
	using RulePlanInput = NoisePlanInput;

	struct RulePlanSlot {
		const NoiseNode*           node = nullptr;
		std::vector<RulePlanInput> inputs;
		bool                       terminal = false;   // node routes into layer weights
	};

	// Caller-owned, so one plan can be evaluated from many threads at once.
	struct RulePlanScratch {
		std::vector<float> values;   // one per slot
		std::vector<float> inputs;   // reused gather buffer
	};

	class RulePlan;
	VE_API RulePlan buildRulePlan(const RuleGraph& graph);

	// A rule graph flattened once into topological order with resolved input slots and
	// collected outputs. Built fresh per bake: the plan holds raw node pointers into
	// the graph, so caching it would only extend the lifetime it has to keep valid.
	class VE_API RulePlan {
	public:
		int  slotCount() const { return (int)m_slots.size(); }
		bool valid()     const { return m_terminalCount > 0; }

		// Fills outWeights[0..maxOut-1] with the weights for layers 1..maxOut at
		// ctx.sample; a channel no terminal routes into is left at 0, and slices that
		// land on the same layer take the maximum. Returns the terminal count.
		int evaluate(RulePlanScratch& scratch, const RuleEvalContext& ctx,
			float* outWeights, int maxOut) const;

	private:
		friend RulePlan buildRulePlan(const RuleGraph&);

		std::vector<RulePlanSlot> m_slots;
		int                       m_terminalCount = 0;
	};

}
