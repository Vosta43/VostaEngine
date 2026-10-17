#include "vepch.h"
#include "Terrain/RulePlan.h"
#include "Terrain/RuleNodes.h"

#include <algorithm>
#include <unordered_map>

namespace ve {

	RulePlan buildRulePlan(const RuleGraph& graph) {
		RulePlan plan;

		// Cycle -> {} -> invalid plan, which is exactly the failure the evaluator wants.
		const std::vector<Ref<GraphNode>> order = graph.topologicalSort();
		if (order.empty())
			return plan;

		std::unordered_map<uint32_t, int> slotOf;
		slotOf.reserve(order.size());
		plan.m_slots.reserve(order.size());

		for (const auto& nodePtr : order) {
			NoiseNode* node = dynamic_cast<NoiseNode*>(nodePtr.get());
			if (!node)
				return RulePlan{};   // a non-rule node reached the evaluator

			RulePlanSlot slot;
			slot.node = node;
			slot.inputs.reserve(node->m_inputPins.size());

			for (const auto& pin : node->m_inputPins) {
				RulePlanInput in;
				for (const auto& link : graph.links) {
					if (link.endPin == pin.id) {
						auto it = slotOf.find(link.startPin.id);
						if (it != slotOf.end()) in.slot = it->second;
						break;
					}
				}
				// Topological order guarantees a wired source already has a slot; a miss
				// means the pin is unconnected, so bake the node's own fallback.
				if (in.slot < 0) in.literal = node->inputDefault(pin.id.pinIndex);
				slot.inputs.push_back(in);
			}

			// Resolve any external asset the node reads once, on this thread, before the
			// parallel per-texel walk, so the walk below stays a pure read.
			if (const auto* rn = dynamic_cast<const RuleNode*>(node)) {
				rn->prepare();
				slot.terminal = rn->isTerminal();
				if (slot.terminal) ++plan.m_terminalCount;
			}

			const int s = (int)plan.m_slots.size();
			slotOf[node->id] = s;
			plan.m_slots.push_back(std::move(slot));
		}

		return plan;
	}

	int RulePlan::evaluate(RulePlanScratch& scratch, const RuleEvalContext& ctx,
		float* outWeights, int maxOut) const {
		for (int i = 0; i < maxOut; ++i)
			outWeights[i] = 0.0f;

		scratch.values.resize(m_slots.size());
		for (size_t s = 0; s < m_slots.size(); ++s) {
			const RulePlanSlot& slot = m_slots[s];
			scratch.inputs.clear();
			for (const RulePlanInput& in : slot.inputs)
				scratch.inputs.push_back(in.slot >= 0 ? scratch.values[in.slot] : in.literal);
			scratch.values[s] = evaluateRuleNode(*slot.node, ctx, scratch.inputs);

			// A terminal routes its now-evaluated inputs into the layer weights (max-
			// merged, so slices on the same layer union instead of stacking past 1).
			if (slot.terminal)
				if (const auto* rn = dynamic_cast<const RuleNode*>(slot.node))
					rn->contributeWeights(ctx, scratch.inputs, outWeights, maxOut);
		}

		return m_terminalCount;
	}

}
