#pragma once

#include "Terrain/RuleGraph.h"
#include "Noise/NoiseGraphResource.h"
#include "Noise/NoisePlan.h"
#include "Core/ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ve {

	// Surface sampling shared by a Field Source's terrain-attribute modes. The height
	// grid uses the terrain's [x * heightRes + z] layout in normalized units (world Y
	// = value * heightScale), so the slope a rule sees agrees with the drawn surface.
	namespace ruledetail {

		// Bilinear height (normalized) at a grid position.
		inline float sampleHeight(const RuleEvalContext& ctx, float fx, float fz) {
			auto H = [&](int x, int z) -> float {
				x = std::clamp(x, 0, ctx.heightRes - 1);
				z = std::clamp(z, 0, ctx.heightRes - 1);
				return ctx.heightGrid[(size_t)x * ctx.heightRes + z];
			};
			const int x0 = (int)std::floor(fx);
			const int z0 = (int)std::floor(fz);
			const float tx = fx - (float)x0;
			const float tz = fz - (float)z0;
			const float a = glm::mix(H(x0, z0), H(x0 + 1, z0), tx);
			const float b = glm::mix(H(x0, z0 + 1), H(x0 + 1, z0 + 1), tx);
			return glm::mix(a, b, tz);
		}

		// Central-difference world normal at grid (x, z).
		inline glm::vec3 normalAt(const RuleEvalContext& ctx, int x, int z) {
			auto H = [&](int i, int j) -> float {
				i = std::clamp(i, 0, ctx.heightRes - 1);
				j = std::clamp(j, 0, ctx.heightRes - 1);
				return ctx.heightGrid[(size_t)i * ctx.heightRes + j];
			};
			const float dhdx = (H(x + 1, z) - H(x - 1, z)) * ctx.heightScale / (2.0f * ctx.tileSize);
			const float dhdz = (H(x, z + 1) - H(x, z - 1)) * ctx.heightScale / (2.0f * ctx.tileSize);
			return glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
		}

		// Discrete Laplacian: positive on a ridge, negative in a valley.
		inline float curvatureAt(const RuleEvalContext& ctx, int x, int z) {
			auto H = [&](int i, int j) -> float {
				i = std::clamp(i, 0, ctx.heightRes - 1);
				j = std::clamp(j, 0, ctx.heightRes - 1);
				return ctx.heightGrid[(size_t)i * ctx.heightRes + j];
			};
			const float c = H(x + 1, z) + H(x - 1, z) + H(x, z + 1) + H(x, z - 1) - 4.0f * H(x, z);
			return c * ctx.heightScale / (ctx.tileSize * ctx.tileSize);
		}
	}

	// The field a Layer Band tests: an external noise asset, or a terrain attribute
	// read from the tile's height grid. One node kind, so the author never has to
	// remember which source wants the surface context.
	class FieldSourceNode : public RuleNode {
	public:
		enum class Mode : int32_t {
			NoiseAsset = 0,
			Height     = 1,
			Slope      = 2,
			Curvature  = 3,
		};

		Mode        mode = Mode::Height;
		std::string noisePath;   // used when mode == NoiseAsset

		FieldSourceNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "Field Source";
			m_outputPins.push_back(GraphPin("Value", {}, PinType::Float, false));
		}

		// Resolve the noise plan once, on the building thread, before the parallel bake,
		// so the per-texel walk stays a pure read.
		void prepare() const override {
			if (mode == Mode::NoiseAsset) ensureField();
		}

		float evaluateRule(const RuleEvalContext& ctx, const std::vector<float>&) const override {
			if (mode == Mode::NoiseAsset) {
				if (!m_planValid) return 0.0f;
				thread_local NoisePlanScratch scratch;
				return m_plan.evaluate(scratch, ctx.sample.x, ctx.sample.y);
			}
			return attribute(ctx);
		}

		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << static_cast<int32_t>(mode);
			ar << noisePath;
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			int32_t m = 0;
			ar >> m;
			mode = static_cast<Mode>(m);
			ar >> noisePath;
			m_planValid = false;
		}

	private:
		float attribute(const RuleEvalContext& ctx) const {
			if (!ctx.heightGrid || ctx.heightRes <= 1 || ctx.tileSize <= 0.0f)
				return 0.0f;

			// World -> height-grid sample coordinates; one step spans `tileSize` metres.
			const glm::vec2 local = ctx.sample - ctx.worldOrigin;
			const float fx = local.x / ctx.tileSize;
			const float fz = local.y / ctx.tileSize;

			if (mode == Mode::Height)
				return ruledetail::sampleHeight(ctx, fx, fz) * ctx.heightScale;

			const int x0 = (int)std::floor(fx);
			const int z0 = (int)std::floor(fz);
			if (mode == Mode::Slope) {
				const glm::vec3 n = ruledetail::normalAt(ctx, x0, z0);
				return 1.0f - std::clamp(n.y, 0.0f, 1.0f);
			}
			return ruledetail::curvatureAt(ctx, x0, z0);
		}

		// Resolves noisePath to a NoiseGraphResource and flattens its graph, rebuilt
		// only when the resource's content hash moves.
		bool ensureField() const {
			if (noisePath.empty()) return false;

			AssetHandle handle = ResourceManager::find<NoiseGraphResource>(noisePath);
			if (!handle.isValid())
				handle = ResourceManager::store<NoiseGraphResource>(noisePath);
			if (!handle.isValid()) return false;

			Ref<NoiseGraphResource> resource = ResourceManager::get<NoiseGraphResource>(handle);
			if (!resource) return false;

			if (m_planValid && resource->contentHash == m_cachedHash) return true;

			m_plan = buildNoisePlan(resource->graph);
			m_cachedHash = resource->contentHash;
			m_planValid = m_plan.valid();
			return m_planValid;
		}

		mutable NoisePlan m_plan;
		mutable uint64_t  m_cachedHash = 0;
		mutable bool      m_planValid = false;
	};

	// One slice of a source's range, routed to a layer. `soft` widens both edges into
	// a ramp so the seam is not a hard line; adjacent slices that share a boundary sum
	// to 1 across their soft zones, so the layers cross-fade instead of leaving a gap.
	struct LayerSegment {
		float minValue = 0.0f;
		float maxValue = 1.0f;
		float soft     = 0.05f;
		int   layer    = 1;
	};

	// Splits the incoming field into the layers: one row per slice of the range. This
	// is the graph's terminal — it holds the whole mapping (e.g. a humidity field into
	// dirt / grass / lush grass), so a single node drives every layer at once and the
	// row count is free to grow or shrink.
	class LayerBandsNode : public RuleNode {
	public:
		std::vector<LayerSegment> segments = {
			{ 0.0f, 0.5f, 0.05f, 1 },
			{ 0.5f, 0.8f, 0.05f, 2 },
			{ 0.8f, 1.0f, 0.05f, 3 },
		};

		LayerBandsNode() {
			m_nodeType = NodeType::Output;
			m_displayName = "Layer Bands";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
		}

		bool isTerminal() const override { return true; }

		void contributeWeights(const RuleEvalContext&, const std::vector<float>& in,
			float* out, int maxOut) const override {
			const float v = in.empty() ? 0.0f : in[0];
			for (const LayerSegment& s : segments) {
				const int ch = s.layer - 1;
				if (ch < 0 || ch >= maxOut) continue;
				out[ch] = std::max(out[ch], weight(s, v));
			}
		}

		// Slice membership in [0,1]: 1 well inside the range, ramping to 0 across `soft`
		// of field value on each edge. A zero soft is a hard step.
		static float weight(const LayerSegment& s, float v) {
			const float lo = std::min(s.minValue, s.maxValue);
			const float hi = std::max(s.minValue, s.maxValue);
			if (s.soft <= 1e-6f)
				return (v >= lo && v <= hi) ? 1.0f : 0.0f;
			const float a = glm::smoothstep(lo - s.soft, lo + s.soft, v);
			const float b = 1.0f - glm::smoothstep(hi - s.soft, hi + s.soft, v);
			return a * b;
		}

		void serialize(Archive& ar) const override {
			NoiseNode::serializeBase(ar);
			ar << static_cast<int32_t>(segments.size());
			for (const LayerSegment& s : segments) {
				ar << s.minValue << s.maxValue << s.soft;
				ar << static_cast<int32_t>(s.layer);
			}
		}
		void deserialize(Archive& ar) override {
			NoiseNode::deserializeBase(ar);
			int32_t n = 0;
			ar >> n;
			segments.clear();
			for (int32_t i = 0; i < n; ++i) {
				LayerSegment s;
				ar >> s.minValue >> s.maxValue >> s.soft;
				int32_t l = 1;
				ar >> l;
				s.layer = l;
				segments.push_back(s);
			}
		}
	};

}
