#include "vepch.h"
#include "Noise/NoiseGraph.h"
#include "Noise/NoiseNodes.h"
#include "Noise/NoisePlan.h"

namespace ve {

	const NodeRegistry& getNoiseNodeRegistry() {
		static const NodeRegistry registry = [] {
			NodeRegistry r;
			r.registerNode<NoiseUnitNode>((uint8_t)NoiseNodeTag::NoiseUnit);
			r.registerNode<NoiseAddNode>((uint8_t)NoiseNodeTag::Add);
			r.registerNode<NoiseSubtractNode>((uint8_t)NoiseNodeTag::Subtract);
			r.registerNode<NoiseMultiplyNode>((uint8_t)NoiseNodeTag::Multiply);
			r.registerNode<NoiseDivideNode>((uint8_t)NoiseNodeTag::Divide);
			r.registerNode<NoiseOutputNode>((uint8_t)NoiseNodeTag::Output);
			r.registerNode<NoiseConstantNode>((uint8_t)NoiseNodeTag::Constant);
			r.registerNode<NoiseBlendNode>((uint8_t)NoiseNodeTag::Blend);
			r.registerNode<NoiseMinNode>((uint8_t)NoiseNodeTag::Min);
			r.registerNode<NoiseMaxNode>((uint8_t)NoiseNodeTag::Max);
			r.registerNode<NoiseInvertNode>((uint8_t)NoiseNodeTag::Invert);
			r.registerNode<NoiseClampNode>((uint8_t)NoiseNodeTag::Clamp);
			r.registerNode<NoiseRemapNode>((uint8_t)NoiseNodeTag::Remap);
			r.registerNode<NoiseThresholdNode>((uint8_t)NoiseNodeTag::Threshold);
			r.registerNode<NoiseAbsNode>((uint8_t)NoiseNodeTag::Abs);
			r.registerNode<NoiseErfNode>((uint8_t)NoiseNodeTag::Erf);
			r.registerNode<NoiseTruncNode>((uint8_t)NoiseNodeTag::Trunc);
			r.registerNode<NoiseSplineNode>((uint8_t)NoiseNodeTag::Spline);
			return r;
		}();
		return registry;
	}

	float evaluateNoiseGraph(const NoiseGraph& graph, float x, float y) {
		const NoisePlan plan = buildNoisePlan(graph);
		if (!plan.valid()) return 0.0f;
		NoisePlanScratch scratch;
		return plan.evaluate(scratch, x, y);
	}

	void serializeNoiseGraph(const NoiseGraph& graph, Archive& ar) {
		graph.serialize(getNoiseNodeRegistry(), ar);
	}

	void deserializeNoiseGraph(NoiseGraph& graph, Archive& ar) {
		graph.deserialize(getNoiseNodeRegistry(), ar);
	}

}
