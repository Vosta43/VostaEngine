#include "vepch.h"
#include "Noise/NoisePlan.h"
#include "Noise/NoiseNodes.h"

#include <unordered_map>

namespace ve {

	NoisePlan buildNoisePlan(const NoiseGraph& graph) {
		NoisePlan plan;

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
				return NoisePlan{};   // a non-noise node reached the evaluator
			NoisePlanSlot slot;
			slot.node = node;
			slot.inputs.reserve(node->m_inputPins.size());

			for (const auto& pin : node->m_inputPins) {
				NoisePlanInput in;
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

			const int s = (int)plan.m_slots.size();
			slotOf[node->id] = s;
			if (dynamic_cast<NoiseOutputNode*>(node)) plan.m_outputSlot = s;
			plan.m_slots.push_back(std::move(slot));
		}

		return plan;
	}

	float NoisePlan::evaluate(NoisePlanScratch& scratch, float x, float y) const {
		scratch.values.resize(m_slots.size());
		for (size_t s = 0; s < m_slots.size(); ++s) {
			const NoisePlanSlot& slot = m_slots[s];
			scratch.inputs.clear();
			for (const NoisePlanInput& in : slot.inputs)
				scratch.inputs.push_back(in.slot >= 0 ? scratch.values[in.slot] : in.literal);
			scratch.values[s] = slot.node->evaluate(scratch.inputs, x, y);
		}
		return m_outputSlot >= 0 ? scratch.values[m_outputSlot] : 0.0f;
	}

}
