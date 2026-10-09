#pragma once

#include "Noise/NoiseGraph.h"

#include <vector>

namespace ve {

	// Where a slot's input comes from: an earlier slot's output, or a literal
	// substituted when the pin is unconnected (the owning node's own fallback).
	struct NoisePlanInput {
		int   slot    = -1;      // >= 0 means wired
		float literal = 0.0f;    // used when slot < 0
	};

	struct NoisePlanSlot {
		const NoiseNode*            node = nullptr;
		std::vector<NoisePlanInput> inputs;
	};

	// Caller-owned, so one plan can be evaluated from many threads at once.
	struct NoisePlanScratch {
		std::vector<float> values;   // one per slot
		std::vector<float> inputs;   // reused gather buffer
	};

	// A graph flattened once into topological order with resolved input slots, so a
	// per-sample evaluation never walks the node list or the link list again.
	class NoisePlan;
	VE_API NoisePlan buildNoisePlan(const NoiseGraph& graph);

	class VE_API NoisePlan {
	public:
		int  slotCount() const { return (int)m_slots.size(); }
		bool valid()     const { return m_outputSlot >= 0; }

		float evaluate(NoisePlanScratch& scratch, float x, float y) const;

	private:
		friend NoisePlan buildNoisePlan(const NoiseGraph&);

		std::vector<NoisePlanSlot> m_slots;
		int m_outputSlot = -1;
	};

}
