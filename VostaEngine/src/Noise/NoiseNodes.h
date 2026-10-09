#pragma once

#include "Noise/NoiseGraph.h"
#include "Noise/NoiseSettings.h"
#include "Noise/NoiseCPU.h"

#include <algorithm>
#include <cmath>

namespace ve {

	// Leaf that produces a field from its own NoiseSettings. The settings form is
	// drawn inline inside the node by NoiseEditorSchema. Frequency and Seed are also
	// input pins so they can be driven by the graph (a shared world seed, a
	// spatially varying detail scale); an unconnected pin falls back to the widget.
	class NoiseUnitNode : public NoiseNode {
	public:
		NoiseSettings settings;

		NoiseUnitNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "Noise Unit";
			m_inputPins.push_back(GraphPin("Frequency", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Seed", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Out", {}, PinType::Float, false));
		}

		float inputDefault(uint32_t pinIndex) const override {
			if (pinIndex == 0) return settings.frequency;
			if (pinIndex == 1) return (float)settings.seed;
			return 0.0f;
		}

		float evaluate(const std::vector<float>& in, float x, float y) const override {
			NoiseSettings s = settings;
			if (in.size() > 0) s.frequency = in[0];
			if (in.size() > 1) s.seed = (int)std::lround(in[1]);
			return NoiseCPU::sample(s, x, y);
		}

		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			settings.serialize(ar);
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			settings.deserialize(ar);
		}
	};

	// Binary scalar operators. Inputs: A, B; output: Result.
	class NoiseAddNode : public NoiseNode {
	public:
		NoiseAddNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Add";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return in[0] + in[1]; }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	class NoiseSubtractNode : public NoiseNode {
	public:
		NoiseSubtractNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Subtract";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return in[0] - in[1]; }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	class NoiseMultiplyNode : public NoiseNode {
	public:
		NoiseMultiplyNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Multiply";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return in[0] * in[1]; }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	class NoiseDivideNode : public NoiseNode {
	public:
		NoiseDivideNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Divide";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override {
			// Guard the divisor so a half-built graph can't emit inf/nan into the bake.
			return std::abs(in[1]) > 1e-8f ? in[0] / in[1] : 0.0f;
		}
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	// Scalar source. Without it every constant (a threshold, a blend weight) would
	// have to be faked by abusing a noise unit.
	class NoiseConstantNode : public NoiseNode {
	public:
		float value = 0.5f;

		NoiseConstantNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "Constant";
			m_outputPins.push_back(GraphPin("Out", {}, PinType::Float, false));
		}

		float evaluate(const std::vector<float>&, float, float) const override { return value; }

		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << value;
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			ar >> value;
		}
	};

	// a*(1-t) + b*t. Wire a Constant into T for a fixed mix; an unconnected T
	// evaluates to 0, which returns A.
	class NoiseBlendNode : public NoiseNode {
	public:
		NoiseBlendNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Blend";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("T", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override {
			return in[0] * (1.0f - in[2]) + in[1] * in[2];
		}
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	class NoiseMinNode : public NoiseNode {
	public:
		NoiseMinNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Min";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return std::min(in[0], in[1]); }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	class NoiseMaxNode : public NoiseNode {
	public:
		NoiseMaxNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Max";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return std::max(in[0], in[1]); }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	// 1 - x, for flipping masks.
	class NoiseInvertNode : public NoiseNode {
	public:
		NoiseInvertNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Invert";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return 1.0f - in[0]; }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	class NoiseClampNode : public NoiseNode {
	public:
		float minValue = 0.0f;
		float maxValue = 1.0f;

		NoiseClampNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Clamp";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override {
			return std::clamp(in[0], minValue, maxValue);
		}
		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << minValue << maxValue;
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			ar >> minValue >> maxValue;
		}
	};

	// Maps an input range onto an output range, optionally clamping the ends — the
	// mid-graph contrast/offset control.
	class NoiseRemapNode : public NoiseNode {
	public:
		float inMin  = -1.0f;
		float inMax  = 1.0f;
		float outMin = 0.0f;
		float outMax = 1.0f;
		bool  clamp  = true;

		NoiseRemapNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Remap";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override {
			// Guard a degenerate input span so a half-built node can't emit inf/nan.
			const float span = inMax - inMin;
			float t = std::abs(span) > 1e-8f ? (in[0] - inMin) / span : 0.0f;
			if (clamp) t = std::clamp(t, 0.0f, 1.0f);
			return outMin + t * (outMax - outMin);
		}
		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << inMin << inMax << outMin << outMax;
			ar << static_cast<int32_t>(clamp ? 1 : 0);
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			ar >> inMin >> inMax >> outMin >> outMax;
			int32_t c = 0;
			ar >> c;
			clamp = (c != 0);
		}
	};

	// Hard step at Threshold when Falloff is 0; widening Falloff turns it into a
	// smoothstep of that width centred on Threshold.
	class NoiseThresholdNode : public NoiseNode {
	public:
		float threshold = 0.5f;
		float falloff   = 0.05f;

		NoiseThresholdNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Threshold";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override {
			const float s = std::max(falloff, 1e-5f);
			float t = std::clamp((in[0] - threshold) / s + 0.5f, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}
		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << threshold << falloff;
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			ar >> threshold >> falloff;
		}
	};

	class NoiseAbsNode : public NoiseNode {
	public:
		NoiseAbsNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Abs";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return std::abs(in[0]); }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	// erf(x / divisor) — squashes a native [-1, 1] field into a smooth ramp, so what
	// follows can treat it as a normalised coordinate rather than a raw sample.
	class NoiseErfNode : public NoiseNode {
	public:
		float divisor = 0.2022f;

		NoiseErfNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Erf";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override {
			const float d = std::abs(divisor) > 1e-8f ? divisor : 1.0f;
			return std::erf(in[0] / d);
		}
		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << divisor;
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			ar >> divisor;
		}
	};

	// Truncates toward zero, so a height field built in metres can be read as a whole
	// number of units.
	class NoiseTruncNode : public NoiseNode {
	public:
		NoiseTruncNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Trunc";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		float evaluate(const std::vector<float>& in, float, float) const override { return std::trunc(in[0]); }
		void serialize(Archive& ar) const override   { NoiseNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { NoiseNode::deserializeBase(ar); }
	};

	// Remap by an arbitrary control-point curve. Points are kept sorted by x; the
	// value is linearly interpolated between them and held flat outside the first and
	// last point.
	class NoiseSplineNode : public NoiseNode {
	public:
		std::vector<glm::vec2> points{ glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f) };

		NoiseSplineNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Spline";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}

		float evaluate(const std::vector<float>& in, float, float) const override {
			if (points.empty()) return 0.0f;
			if (points.size() == 1) return points[0].y;

			const float x = in[0];
			if (x <= points.front().x) return points.front().y;
			if (x >= points.back().x)  return points.back().y;

			for (size_t i = 1; i < points.size(); ++i) {
				if (x <= points[i].x) {
					const float x0 = points[i - 1].x;
					const float span = points[i].x - x0;
					const float t = span > 1e-8f ? (x - x0) / span : 0.0f;
					return points[i - 1].y + t * (points[i].y - points[i - 1].y);
				}
			}
			return points.back().y;
		}

		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << (uint32_t)points.size();
			for (const auto& p : points) ar << p;
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			uint32_t n = 0;
			ar >> n;
			points.clear();
			points.reserve(n);
			for (uint32_t i = 0; i < n; ++i) {
				glm::vec2 p(0.0f);
				ar >> p;
				points.push_back(p);
			}
		}
	};

	// Terminal. Its range remaps the finished field for preview/export only; each
	// unit's own NoiseSettings range shapes that unit before the math combines.
	class NoiseOutputNode : public NoiseNode {
	public:
		float outputMin = -1.0f;
		float outputMax = 1.0f;

		NoiseOutputNode() {
			m_nodeType = NodeType::Output;
			m_displayName = "Output";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
		}

		float evaluate(const std::vector<float>& in, float, float) const override {
			return in.empty() ? 0.0f : in[0];
		}

		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << outputMin << outputMax;
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			ar >> outputMin >> outputMax;
		}
	};

	// The single terminal of a noise graph, or null while none exists.
	inline NoiseOutputNode* findNoiseOutput(NoiseGraph& graph) {
		for (auto& node : graph.nodes)
			if (auto* out = dynamic_cast<NoiseOutputNode*>(node.get()))
				return out;
		return nullptr;
	}

}
