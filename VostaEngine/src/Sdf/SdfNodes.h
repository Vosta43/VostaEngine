#pragma once

#include "Graph/NodeGraph.h"
#include "Sdf/SdfValue.h"
#include "Geometry/GeometryBuffer.h"
#include "Geometry/GeometryGraph.h"
#include "Noise/NoiseGraphResource.h"
#include "Noise/NoisePlan.h"
#include "Core/ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm.hpp>

namespace ve {

	// One resolved field per SDF input pin (empty when the pin is unconnected).
	struct SdfEvalContext {
		std::vector<SdfValue> inputs;
	};

	// Base of the SDF family. evaluate() returns a *function*, not a buffer: a node
	// wraps its upstream fields and its own parameters into a new closure, so the graph
	// is the expression tree and nothing is sampled until a Marching Cubes node runs.
	class VE_API SdfNode : public GraphNode {
	public:
		virtual SdfValue evaluate(const SdfEvalContext& ctx) const { (void)ctx; return {}; }

	protected:
		static SdfValue inputField(const SdfEvalContext& ctx, size_t pin) {
			if (pin < ctx.inputs.size()) return ctx.inputs[pin];
			return {};
		}
	};

	// A single primitive: distance to a sphere, negative inside. Marching Cubes of
	// `length(p - c) - r` yields an outward-wound surface.
	class SdfSphereNode : public SdfNode {
	public:
		glm::vec3 center = glm::vec3(0.0f);
		float     radius = 1.0f;

		SdfSphereNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "SDF Sphere";
			m_outputPins.push_back(GraphPin("Sdf", {}, PinType::Sdf, false));
		}

		SdfValue evaluate(const SdfEvalContext&) const override {
			const glm::vec3 c = center;
			const float     r = radius;
			SdfValue out;
			out.fn = [c, r](const glm::vec3& p) { return glm::length(p - c) - r; };
			return out;
		}

		void serialize(Archive& ar) const override {
			SdfNode::serializeBase(ar);
			ar << center << radius;
		}
		void deserialize(Archive& ar) override {
			SdfNode::deserializeBase(ar);
			ar >> center >> radius;
		}
	};

	// Adds a scalar NoiseGraph field to the distance: the surface grows where the field
	// is positive and carves where it is negative. Because it perturbs the *field*, not
	// vertices, it stays seamless and still applies after booleans, unlike a mesh
	// displacement. Keep amplitude * frequency small (roughly <= 0.3): the perturbation
	// must not exceed 1 or the field grows spurious zero crossings.
	class SdfNoiseDisplaceNode : public SdfNode {
	public:
		// Project-relative path of a noise .veasset, resolved through ResourceManager.
		std::string noisePath;
		float       amplitude = 0.2f;
		float       bias      = 0.0f;
		float       frequency = 1.5f;

		SdfNoiseDisplaceNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "SDF Noise Displace";
			m_inputPins.push_back(GraphPin("Sdf", {}, PinType::Sdf, true));
			m_outputPins.push_back(GraphPin("Sdf", {}, PinType::Sdf, false));
		}

		SdfValue evaluate(const SdfEvalContext& ctx) const override {
			const SdfValue up = inputField(ctx, 0);
			if (!up.valid()) return {};

			// Pass the upstream field through while unset, so a half-wired graph still
			// previews. Resolve now: the returned closure outlives this call and is
			// invoked later by Marching Cubes, so it may only touch captured values.
			if (noisePath.empty()) return up;
			AssetHandle handle = ResourceManager::find<NoiseGraphResource>(noisePath);
			if (!handle.isValid())
				handle = ResourceManager::store<NoiseGraphResource>(noisePath);
			if (!handle.isValid()) return up;

			Ref<NoiseGraphResource> resource = ResourceManager::get<NoiseGraphResource>(handle);
			if (!resource) return up;

			// Captured alongside the plan: the plan's slots hold raw NoiseNode* into the
			// resource's graph, so the resource must outlive the closure.
			auto plan = std::make_shared<NoisePlan>(buildNoisePlan(resource->graph));
			if (!plan->valid()) return up;

			const float amp = amplitude;
			const float b   = bias;
			const float f   = frequency;
			SdfValue out;
			out.fn = [up, plan, resource, amp, b, f](const glm::vec3& p) {
				static thread_local NoisePlanScratch scratch;
				const float n = plan->evaluate(scratch, p.x * f, p.y * f, p.z * f);
				return up.fn(p) - (amp * n + b);
			};
			return out;
		}

		void serialize(Archive& ar) const override {
			SdfNode::serializeBase(ar);
			ar << noisePath;
			ar << amplitude << bias << frequency;
		}
		void deserialize(Archive& ar) override {
			SdfNode::deserializeBase(ar);
			ar >> noisePath;
			ar >> amplitude >> bias >> frequency;
		}
	};

	namespace sdf_detail {

		// Polynomial smooth min (Inigo Quilez): blends a and b with a fillet of radius
		// k, and only where |a - b| < k, so untouched regions stay exact. k <= 0 is a
		// hard min. The blend is not an exact distance field — its error is bounded by
		// roughly k/4, which softens the gradient but Marching Cubes only reads signs.
		inline float smoothMin(float a, float b, float k) {
			if (k <= 1e-5f) return std::min(a, b);
			const float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
			return b + (a - b) * h - k * h * (1.0f - h);
		}
		inline float smoothMax(float a, float b, float k) {
			return -smoothMin(-a, -b, k);
		}

	}

	// Combines two fields. Subtract means "A minus B": max(a, -b) is the distance to the
	// part of A outside B, which carves B's volume out of A.
	class SdfBooleanNode : public SdfNode {
	public:
		enum class Mode : int { Union = 0, Subtract = 1, Intersect = 2 };

		Mode  mode         = Mode::Union;
		// Fuse radius in world units. 0 is a hard boolean; larger values round the seam.
		float smoothRadius = 0.0f;

		SdfBooleanNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "SDF Boolean";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Sdf, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Sdf, true));
			m_outputPins.push_back(GraphPin("Sdf", {}, PinType::Sdf, false));
		}

		SdfValue evaluate(const SdfEvalContext& ctx) const override {
			const SdfValue a = inputField(ctx, 0);
			const SdfValue b = inputField(ctx, 1);
			// One side unconnected: pass the other through so a half-wired graph still
			// previews instead of collapsing.
			if (!b.valid()) return a;
			if (!a.valid()) return b;

			const Mode  m = mode;
			const float k = smoothRadius;
			SdfValue out;
			out.fn = [a, b, m, k](const glm::vec3& p) {
				const float da = a.fn(p);
				const float db = b.fn(p);
				switch (m) {
					case Mode::Union:     return sdf_detail::smoothMin(da, db, k);
					case Mode::Subtract:  return sdf_detail::smoothMax(da, -db, k);
					default:              return sdf_detail::smoothMax(da, db, k);
				}
			};
			return out;
		}

		void serialize(Archive& ar) const override {
			SdfNode::serializeBase(ar);
			ar << static_cast<int32_t>(mode);
			ar << smoothRadius;
		}
		void deserialize(Archive& ar) override {
			SdfNode::deserializeBase(ar);
			int32_t m = 0;
			ar >> m >> smoothRadius;
			mode = static_cast<Mode>(m);
		}
	};

	namespace mc_detail {

		// Cube corners as (dx,dy,dz) offsets from the cell origin, in the numbering the
		// tables below assume. Edge e joins kEdgeCorners[e][0] and [1]. The two match
		// the canonical Paul Bourke polygonise tables (public domain).
		inline constexpr int kCorner[8][3] = {
			{ 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 },
			{ 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 },
		};
		inline constexpr int kEdgeCorners[12][2] = {
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
			{ 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
		};

		// 8-bit inside/outside mask -> 12-bit set of crossed edges.
		inline constexpr int kEdgeTable[256] = {
			0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c, 0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
			0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c, 0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
			0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c, 0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
			0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac, 0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
			0x460, 0x569, 0x663, 0x76a, 0x066, 0x16f, 0x265, 0x36c, 0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
			0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc, 0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
			0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c, 0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
			0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc, 0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
			0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc, 0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
			0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c, 0x15c, 0x055, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
			0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc, 0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
			0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c, 0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
			0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac, 0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
			0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c, 0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
			0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c, 0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
			0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c, 0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000,
		};

		// Inside mask -> triangles as edge indices, up to 15 then a -1 terminator.
		inline constexpr int kTriTable[256][16] = {
			{ -1 },
			{ 0, 8, 3, -1 },
			{ 0, 1, 9, -1 },
			{ 1, 8, 3, 9, 8, 1, -1 },
			{ 1, 2, 10, -1 },
			{ 0, 8, 3, 1, 2, 10, -1 },
			{ 9, 2, 10, 0, 2, 9, -1 },
			{ 2, 8, 3, 2, 10, 8, 10, 9, 8, -1 },
			{ 3, 11, 2, -1 },
			{ 0, 11, 2, 8, 11, 0, -1 },
			{ 1, 9, 0, 2, 3, 11, -1 },
			{ 1, 11, 2, 1, 9, 11, 9, 8, 11, -1 },
			{ 3, 10, 1, 11, 10, 3, -1 },
			{ 0, 10, 1, 0, 8, 10, 8, 11, 10, -1 },
			{ 3, 9, 0, 3, 11, 9, 11, 10, 9, -1 },
			{ 9, 8, 10, 10, 8, 11, -1 },
			{ 4, 7, 8, -1 },
			{ 4, 3, 0, 7, 3, 4, -1 },
			{ 0, 1, 9, 8, 4, 7, -1 },
			{ 4, 1, 9, 4, 7, 1, 7, 3, 1, -1 },
			{ 1, 2, 10, 8, 4, 7, -1 },
			{ 3, 4, 7, 3, 0, 4, 1, 2, 10, -1 },
			{ 9, 2, 10, 9, 0, 2, 8, 4, 7, -1 },
			{ 2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1 },
			{ 8, 4, 7, 3, 11, 2, -1 },
			{ 11, 4, 7, 11, 2, 4, 2, 0, 4, -1 },
			{ 9, 0, 1, 8, 4, 7, 2, 3, 11, -1 },
			{ 4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1 },
			{ 3, 10, 1, 3, 11, 10, 7, 8, 4, -1 },
			{ 1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1 },
			{ 4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1 },
			{ 4, 7, 11, 4, 11, 9, 9, 11, 10, -1 },
			{ 9, 5, 4, -1 },
			{ 9, 5, 4, 0, 8, 3, -1 },
			{ 0, 5, 4, 1, 5, 0, -1 },
			{ 8, 5, 4, 8, 3, 5, 3, 1, 5, -1 },
			{ 1, 2, 10, 9, 5, 4, -1 },
			{ 3, 0, 8, 1, 2, 10, 4, 9, 5, -1 },
			{ 5, 2, 10, 5, 4, 2, 4, 0, 2, -1 },
			{ 2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1 },
			{ 9, 5, 4, 2, 3, 11, -1 },
			{ 0, 11, 2, 0, 8, 11, 4, 9, 5, -1 },
			{ 0, 5, 4, 0, 1, 5, 2, 3, 11, -1 },
			{ 2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1 },
			{ 10, 3, 11, 10, 1, 3, 9, 5, 4, -1 },
			{ 4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1 },
			{ 5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1 },
			{ 5, 4, 8, 5, 8, 10, 10, 8, 11, -1 },
			{ 9, 7, 8, 5, 7, 9, -1 },
			{ 9, 3, 0, 9, 5, 3, 5, 7, 3, -1 },
			{ 0, 7, 8, 0, 1, 7, 1, 5, 7, -1 },
			{ 1, 5, 3, 3, 5, 7, -1 },
			{ 9, 7, 8, 9, 5, 7, 10, 1, 2, -1 },
			{ 10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1 },
			{ 8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1 },
			{ 2, 10, 5, 2, 5, 3, 3, 5, 7, -1 },
			{ 7, 9, 5, 7, 8, 9, 3, 11, 2, -1 },
			{ 9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1 },
			{ 2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1 },
			{ 11, 2, 1, 11, 1, 7, 7, 1, 5, -1 },
			{ 9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1 },
			{ 5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1 },
			{ 11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1 },
			{ 11, 10, 5, 7, 11, 5, -1 },
			{ 10, 6, 5, -1 },
			{ 0, 8, 3, 5, 10, 6, -1 },
			{ 9, 0, 1, 5, 10, 6, -1 },
			{ 1, 8, 3, 1, 9, 8, 5, 10, 6, -1 },
			{ 1, 6, 5, 2, 6, 1, -1 },
			{ 1, 6, 5, 1, 2, 6, 3, 0, 8, -1 },
			{ 9, 6, 5, 9, 0, 6, 0, 2, 6, -1 },
			{ 5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1 },
			{ 2, 3, 11, 10, 6, 5, -1 },
			{ 11, 0, 8, 11, 2, 0, 10, 6, 5, -1 },
			{ 0, 1, 9, 2, 3, 11, 5, 10, 6, -1 },
			{ 5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1 },
			{ 6, 3, 11, 6, 5, 3, 5, 1, 3, -1 },
			{ 0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1 },
			{ 3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1 },
			{ 6, 5, 9, 6, 9, 11, 11, 9, 8, -1 },
			{ 5, 10, 6, 4, 7, 8, -1 },
			{ 4, 3, 0, 4, 7, 3, 6, 5, 10, -1 },
			{ 1, 9, 0, 5, 10, 6, 8, 4, 7, -1 },
			{ 10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1 },
			{ 6, 1, 2, 6, 5, 1, 4, 7, 8, -1 },
			{ 1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1 },
			{ 8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1 },
			{ 7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1 },
			{ 3, 11, 2, 7, 8, 4, 10, 6, 5, -1 },
			{ 5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1 },
			{ 0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1 },
			{ 9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1 },
			{ 8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1 },
			{ 5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1 },
			{ 0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1 },
			{ 6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1 },
			{ 10, 4, 9, 6, 4, 10, -1 },
			{ 4, 10, 6, 4, 9, 10, 0, 8, 3, -1 },
			{ 10, 0, 1, 10, 6, 0, 6, 4, 0, -1 },
			{ 8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1 },
			{ 1, 4, 9, 1, 2, 4, 2, 6, 4, -1 },
			{ 3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1 },
			{ 0, 2, 4, 4, 2, 6, -1 },
			{ 8, 3, 2, 8, 2, 4, 4, 2, 6, -1 },
			{ 10, 4, 9, 10, 6, 4, 11, 2, 3, -1 },
			{ 0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1 },
			{ 3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1 },
			{ 6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1 },
			{ 9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1 },
			{ 8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1 },
			{ 3, 11, 6, 3, 6, 0, 0, 6, 4, -1 },
			{ 6, 4, 8, 11, 6, 8, -1 },
			{ 7, 10, 6, 7, 8, 10, 8, 9, 10, -1 },
			{ 0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1 },
			{ 10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1 },
			{ 10, 6, 7, 10, 7, 1, 1, 7, 3, -1 },
			{ 1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1 },
			{ 2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1 },
			{ 7, 8, 0, 7, 0, 6, 6, 0, 2, -1 },
			{ 7, 3, 2, 6, 7, 2, -1 },
			{ 2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1 },
			{ 2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1 },
			{ 1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1 },
			{ 11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1 },
			{ 8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1 },
			{ 0, 9, 1, 11, 6, 7, -1 },
			{ 7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1 },
			{ 7, 11, 6, -1 },
			{ 7, 6, 11, -1 },
			{ 3, 0, 8, 11, 7, 6, -1 },
			{ 0, 1, 9, 11, 7, 6, -1 },
			{ 8, 1, 9, 8, 3, 1, 11, 7, 6, -1 },
			{ 10, 1, 2, 6, 11, 7, -1 },
			{ 1, 2, 10, 3, 0, 8, 6, 11, 7, -1 },
			{ 2, 9, 0, 2, 10, 9, 6, 11, 7, -1 },
			{ 6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1 },
			{ 7, 2, 3, 6, 2, 7, -1 },
			{ 7, 0, 8, 7, 6, 0, 6, 2, 0, -1 },
			{ 2, 7, 6, 2, 3, 7, 0, 1, 9, -1 },
			{ 1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1 },
			{ 10, 7, 6, 10, 1, 7, 1, 3, 7, -1 },
			{ 10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1 },
			{ 0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1 },
			{ 7, 6, 10, 7, 10, 8, 8, 10, 9, -1 },
			{ 6, 8, 4, 11, 8, 6, -1 },
			{ 3, 6, 11, 3, 0, 6, 0, 4, 6, -1 },
			{ 8, 6, 11, 8, 4, 6, 9, 0, 1, -1 },
			{ 9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1 },
			{ 6, 8, 4, 6, 11, 8, 2, 10, 1, -1 },
			{ 1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1 },
			{ 4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1 },
			{ 10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1 },
			{ 8, 2, 3, 8, 4, 2, 4, 6, 2, -1 },
			{ 0, 4, 2, 4, 6, 2, -1 },
			{ 1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1 },
			{ 1, 9, 4, 1, 4, 2, 2, 4, 6, -1 },
			{ 8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1 },
			{ 10, 1, 0, 10, 0, 6, 6, 0, 4, -1 },
			{ 4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1 },
			{ 10, 9, 4, 6, 10, 4, -1 },
			{ 4, 9, 5, 7, 6, 11, -1 },
			{ 0, 8, 3, 4, 9, 5, 11, 7, 6, -1 },
			{ 5, 0, 1, 5, 4, 0, 7, 6, 11, -1 },
			{ 11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1 },
			{ 9, 5, 4, 10, 1, 2, 7, 6, 11, -1 },
			{ 6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1 },
			{ 7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1 },
			{ 3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1 },
			{ 7, 2, 3, 7, 6, 2, 5, 4, 9, -1 },
			{ 9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1 },
			{ 3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1 },
			{ 6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1 },
			{ 9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1 },
			{ 1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1 },
			{ 4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1 },
			{ 7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1 },
			{ 6, 9, 5, 6, 11, 9, 11, 8, 9, -1 },
			{ 3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1 },
			{ 0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1 },
			{ 6, 11, 3, 6, 3, 5, 5, 3, 1, -1 },
			{ 1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1 },
			{ 0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1 },
			{ 11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1 },
			{ 6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1 },
			{ 5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1 },
			{ 9, 5, 6, 9, 6, 0, 0, 6, 2, -1 },
			{ 1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1 },
			{ 1, 5, 6, 2, 1, 6, -1 },
			{ 1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1 },
			{ 10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1 },
			{ 0, 3, 8, 5, 6, 10, -1 },
			{ 10, 5, 6, -1 },
			{ 11, 5, 10, 7, 5, 11, -1 },
			{ 11, 5, 10, 11, 7, 5, 8, 3, 0, -1 },
			{ 5, 11, 7, 5, 10, 11, 1, 9, 0, -1 },
			{ 10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1 },
			{ 11, 1, 2, 11, 7, 1, 7, 5, 1, -1 },
			{ 0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1 },
			{ 9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1 },
			{ 7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1 },
			{ 2, 5, 10, 2, 3, 5, 3, 7, 5, -1 },
			{ 8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1 },
			{ 9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1 },
			{ 9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1 },
			{ 1, 3, 5, 3, 7, 5, -1 },
			{ 0, 8, 7, 0, 7, 1, 1, 7, 5, -1 },
			{ 9, 0, 3, 9, 3, 5, 5, 3, 7, -1 },
			{ 9, 8, 7, 5, 9, 7, -1 },
			{ 5, 8, 4, 5, 10, 8, 10, 11, 8, -1 },
			{ 5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1 },
			{ 0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1 },
			{ 10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1 },
			{ 2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1 },
			{ 0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1 },
			{ 0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1 },
			{ 9, 4, 5, 2, 11, 3, -1 },
			{ 2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1 },
			{ 5, 10, 2, 5, 2, 4, 4, 2, 0, -1 },
			{ 3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1 },
			{ 5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1 },
			{ 8, 4, 5, 8, 5, 3, 3, 5, 1, -1 },
			{ 0, 4, 5, 1, 0, 5, -1 },
			{ 8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1 },
			{ 9, 4, 5, -1 },
			{ 4, 11, 7, 4, 9, 11, 9, 10, 11, -1 },
			{ 0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1 },
			{ 1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1 },
			{ 3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1 },
			{ 4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1 },
			{ 9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1 },
			{ 11, 7, 4, 11, 4, 2, 2, 4, 0, -1 },
			{ 11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1 },
			{ 2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1 },
			{ 9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1 },
			{ 3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1 },
			{ 1, 10, 2, 8, 7, 4, -1 },
			{ 4, 9, 1, 4, 1, 7, 7, 1, 3, -1 },
			{ 4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1 },
			{ 4, 0, 3, 7, 4, 3, -1 },
			{ 4, 8, 7, -1 },
			{ 9, 10, 8, 10, 11, 8, -1 },
			{ 3, 0, 9, 3, 9, 11, 11, 9, 10, -1 },
			{ 0, 1, 10, 0, 10, 8, 8, 10, 11, -1 },
			{ 3, 1, 10, 11, 3, 10, -1 },
			{ 1, 2, 11, 1, 11, 9, 9, 11, 8, -1 },
			{ 3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1 },
			{ 0, 2, 11, 8, 0, 11, -1 },
			{ 3, 2, 11, -1 },
			{ 2, 3, 8, 2, 8, 10, 10, 8, 9, -1 },
			{ 9, 10, 2, 0, 9, 2, -1 },
			{ 2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1 },
			{ 1, 10, 2, -1 },
			{ 1, 3, 8, 9, 1, 8, -1 },
			{ 0, 9, 1, -1 },
			{ 0, 3, 8, -1 },
			{ -1 },
		};

	} // namespace mc_detail

	// Samples a signed-distance field on a lattice and extracts the zero isosurface as
	// a triangle mesh. This is the one bridge from SDF-land (functions) back to the
	// mesh pipeline; everything upstream is untouched by it.
	class MarchingCubesNode : public GeometryNode {
	public:
		glm::vec3 boundsMin = glm::vec3(-1.5f);
		glm::vec3 boundsMax = glm::vec3(1.5f);
		int       resolution = 48;   // cells per axis; the lattice is (resolution+1)^3

		MarchingCubesNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Marching Cubes";
			m_inputPins.push_back(GraphPin("Sdf", {}, PinType::Sdf, true));
			m_outputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, false));
		}

		Ref<const GeometryBuffer> evaluate(const GeometryEvalContext& ctx) const override {
			const SdfValue field = ctx.inputs.empty() ? SdfValue{} : ctx.inputs[0].sdf;
			if (!field.valid()) return nullptr;

			const glm::vec3 size = boundsMax - boundsMin;
			if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) return nullptr;

			const int       n    = std::clamp(resolution, 2, 128);
			const int       gx   = n + 1;
			const glm::vec3 step = size / static_cast<float>(n);

			std::vector<float> lattice(static_cast<size_t>(gx) * gx * gx);
			auto sample = [&](int x, int y, int z) -> float& {
				return lattice[(static_cast<size_t>(z) * gx + y) * gx + x];
			};
			for (int z = 0; z < gx; ++z)
				for (int y = 0; y < gx; ++y)
					for (int x = 0; x < gx; ++x)
						sample(x, y, z) = field.fn(boundsMin + glm::vec3(x, y, z) * step);

			auto out = CreateRef<GeometryBuffer>();

			// One shared vertex per crossed grid edge, keyed by the edge's two global
			// corner indices so neighbouring cells weld instead of cracking apart. An
			// unwelded mesh has torn normals at every cell boundary.
			std::unordered_map<uint64_t, uint32_t> edgeVertex;
			auto globalCorner = [&](int x, int y, int z, const int c[3]) -> uint64_t {
				return (static_cast<uint64_t>(z + c[2]) * gx + (y + c[1])) * gx + (x + c[0]);
			};

			glm::vec3 cellCorner[8];
			float     cellVal[8];
			for (int z = 0; z < n; ++z)
			for (int y = 0; y < n; ++y)
			for (int x = 0; x < n; ++x) {
				int cube = 0;
				for (int i = 0; i < 8; ++i) {
					const float v = sample(x + mc_detail::kCorner[i][0],
					                       y + mc_detail::kCorner[i][1],
					                       z + mc_detail::kCorner[i][2]);
					cellVal[i] = v;
					if (v < 0.0f) cube |= (1 << i);
				}
				if (mc_detail::kEdgeTable[cube] == 0) continue;

				for (int i = 0; i < 8; ++i)
					cellCorner[i] = boundsMin + glm::vec3(x + mc_detail::kCorner[i][0],
					                                      y + mc_detail::kCorner[i][1],
					                                      z + mc_detail::kCorner[i][2]) * step;

				uint32_t edgeIdx[12];
				for (int e = 0; e < 12; ++e) {
					edgeIdx[e] = 0xffffffffu;
					if (!(mc_detail::kEdgeTable[cube] & (1 << e))) continue;

					const int a = mc_detail::kEdgeCorners[e][0];
					const int b = mc_detail::kEdgeCorners[e][1];
					const uint64_t ga = globalCorner(x, y, z, mc_detail::kCorner[a]);
					const uint64_t gb = globalCorner(x, y, z, mc_detail::kCorner[b]);
					const uint64_t key = ga < gb ? (ga << 32) | gb : (gb << 32) | ga;

					auto it = edgeVertex.find(key);
					if (it != edgeVertex.end()) { edgeIdx[e] = it->second; continue; }

					const float denom = cellVal[a] - cellVal[b];
					const float t = (denom != 0.0f) ? (cellVal[a] / denom) : 0.5f;
					const glm::vec3 p = cellCorner[a] + (cellCorner[b] - cellCorner[a]) * t;

					edgeIdx[e] = static_cast<uint32_t>(out->positions.size());
					out->positions.push_back(p);
					edgeVertex.emplace(key, edgeIdx[e]);
				}

				for (int i = 0; mc_detail::kTriTable[cube][i] != -1; i += 3) {
					out->indices.push_back(edgeIdx[mc_detail::kTriTable[cube][i]]);
					out->indices.push_back(edgeIdx[mc_detail::kTriTable[cube][i + 1]]);
					out->indices.push_back(edgeIdx[mc_detail::kTriTable[cube][i + 2]]);
				}
			}

			if (out->empty()) return nullptr;

			// True SDF normals from the gradient (points outward, toward increasing
			// distance). The 256 table cases are not guaranteed to share a winding, so
			// orient each triangle to agree with the gradient rather than trusting it.
			const glm::vec3 h = step * 0.5f;
			auto gradient = [&](const glm::vec3& p) {
				const glm::vec3 g(
					field.fn(p + glm::vec3(h.x, 0.0f, 0.0f)) - field.fn(p - glm::vec3(h.x, 0.0f, 0.0f)),
					field.fn(p + glm::vec3(0.0f, h.y, 0.0f)) - field.fn(p - glm::vec3(0.0f, h.y, 0.0f)),
					field.fn(p + glm::vec3(0.0f, 0.0f, h.z)) - field.fn(p - glm::vec3(0.0f, 0.0f, h.z)));
				const float len = glm::length(g);
				return len > 1e-8f ? g / len : glm::vec3(0.0f, 1.0f, 0.0f);
			};

			out->normals.resize(out->positions.size());
			for (size_t i = 0; i < out->positions.size(); ++i)
				out->normals[i] = gradient(out->positions[i]);

			for (size_t i = 0; i + 2 < out->indices.size(); i += 3) {
				const glm::vec3& a = out->positions[out->indices[i]];
				const glm::vec3& b = out->positions[out->indices[i + 1]];
				const glm::vec3& c = out->positions[out->indices[i + 2]];
				const glm::vec3 faceN = glm::cross(b - a, c - a);
				const glm::vec3 vertN = out->normals[out->indices[i]]
				                      + out->normals[out->indices[i + 1]]
				                      + out->normals[out->indices[i + 2]];
				if (glm::dot(faceN, vertN) < 0.0f)
					std::swap(out->indices[i + 1], out->indices[i + 2]);
			}
			return out;
		}

		void serialize(Archive& ar) const override {
			GeometryNode::serializeBase(ar);
			ar << boundsMin << boundsMax;
			ar << static_cast<int32_t>(resolution);
		}
		void deserialize(Archive& ar) override {
			GeometryNode::deserializeBase(ar);
			int32_t res = resolution;
			ar >> boundsMin >> boundsMax;
			ar >> res;
			resolution = res;
		}
	};

}
