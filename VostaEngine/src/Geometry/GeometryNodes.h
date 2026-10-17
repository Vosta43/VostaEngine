#pragma once

#include "Geometry/GeometryGraph.h"
#include "Noise/NoiseSettings.h"
#include "Noise/NoiseCPU.h"
#include "Noise/NoiseGraphResource.h"
#include "Noise/NoisePlan.h"
#include "Core/ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace ve {

	namespace geom_detail {

		inline const GeometryBuffer* inputGeometry(const GeometryEvalContext& ctx, size_t pin) {
			if (pin >= ctx.inputs.size() || !ctx.inputs[pin].geometry) return nullptr;
			return ctx.inputs[pin].geometry.get();
		}

		// Splits every triangle into four at its edge midpoints. Shared edges are welded
		// through a cache, so the surface stays watertight — unwelded seams are the
		// classic failure that cracks a displaced mesh apart. normalizeMidpoints projects
		// the new points back onto the unit sphere, which keeps an icosphere round
		// instead of letting it converge to a faceted blob.
		inline void subdivideMidpoints(std::vector<glm::vec3>& positions,
		                               std::vector<uint32_t>& indices,
		                               bool normalizeMidpoints) {
			std::unordered_map<uint64_t, uint32_t> midCache;
			std::vector<uint32_t> next;
			next.reserve(indices.size() * 4);

			auto midpoint = [&](uint32_t a, uint32_t b) -> uint32_t {
				const uint64_t key = a < b ? (static_cast<uint64_t>(a) << 32) | b
				                           : (static_cast<uint64_t>(b) << 32) | a;
				auto it = midCache.find(key);
				if (it != midCache.end()) return it->second;

				glm::vec3 mid = (positions[a] + positions[b]) * 0.5f;
				if (normalizeMidpoints) {
					const float len = glm::length(mid);
					if (len > 1e-8f) mid /= len;
				}
				const uint32_t idx = static_cast<uint32_t>(positions.size());
				positions.push_back(mid);
				midCache.emplace(key, idx);
				return idx;
			};

			for (size_t i = 0; i + 2 < indices.size(); i += 3) {
				const uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
				const uint32_t ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
				next.insert(next.end(), { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca });
			}
			indices.swap(next);
		}

	} // namespace geom_detail

	// Source node: a unit icosphere scaled by radius/scale. Its 12-vertex
	// golden-ratio base is subdivided with welded, spherical midpoints so later
	// displacement has a dense, seamless surface to push around.
	class IcosphereNode : public GeometryNode {
	public:
		float     radius = 1.0f;
		int       subdivisions = 3;
		glm::vec3 scale = glm::vec3(1.0f);

		IcosphereNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "Icosphere";
			m_outputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, false));
		}

		Ref<const GeometryBuffer> evaluate(const GeometryEvalContext&) const override {
			auto out = CreateRef<GeometryBuffer>();

			const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
			out->positions = {
				{ -1.0f,    t,  0.0f }, {  1.0f,    t,  0.0f }, { -1.0f,   -t,  0.0f }, {  1.0f,   -t,  0.0f },
				{  0.0f, -1.0f,    t }, {  0.0f,  1.0f,    t }, {  0.0f, -1.0f,   -t }, {  0.0f,  1.0f,   -t },
				{    t,  0.0f, -1.0f }, {    t,  0.0f,  1.0f }, {   -t,  0.0f, -1.0f }, {   -t,  0.0f,  1.0f },
			};
			for (auto& p : out->positions) p = glm::normalize(p);

			out->indices = {
				0, 11, 5,  0, 5, 1,  0, 1, 7,  0, 7, 10,  0, 10, 11,
				1, 5, 9,   5, 11, 4, 11, 10, 2, 10, 7, 6,  7, 1, 8,
				3, 9, 4,   3, 4, 2,  3, 2, 6,   3, 6, 8,   3, 8, 9,
				4, 9, 5,   2, 4, 11, 6, 2, 10,  8, 6, 7,   9, 8, 1,
			};

			// 6 levels is ~41k vertices; the cap keeps a bad value from exhausting memory.
			const int levels = std::clamp(subdivisions, 0, 6);
			for (int i = 0; i < levels; ++i)
				geom_detail::subdivideMidpoints(out->positions, out->indices, true);

			for (auto& p : out->positions) p *= (radius * scale);
			return out;
		}

		void serialize(Archive& ar) const override {
			GeometryNode::serializeBase(ar);
			ar << radius;
			ar << static_cast<int32_t>(subdivisions);
			ar << scale;
		}
		void deserialize(Archive& ar) override {
			GeometryNode::deserializeBase(ar);
			int32_t sd = subdivisions;
			ar >> radius >> sd >> scale;
			subdivisions = sd;
		}
	};

	// Generic topology refine: one or more midpoint passes. smooth projects midpoints
	// onto the unit sphere (wants to stay on for a spherical input, off for a flat one).
	class SubdivideNode : public GeometryNode {
	public:
		int  iterations = 1;
		bool smooth = false;

		SubdivideNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Subdivide";
			m_inputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, true));
			m_outputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, false));
		}

		Ref<const GeometryBuffer> evaluate(const GeometryEvalContext& ctx) const override {
			const GeometryBuffer* in = geom_detail::inputGeometry(ctx, 0);
			if (!in) return nullptr;

			auto out = CreateRef<GeometryBuffer>(*in);
			const int levels = std::clamp(iterations, 0, 4);
			for (int i = 0; i < levels; ++i)
				geom_detail::subdivideMidpoints(out->positions, out->indices, smooth);
			return out;
		}

		void serialize(Archive& ar) const override {
			GeometryNode::serializeBase(ar);
			ar << static_cast<int32_t>(iterations);
			ar << static_cast<int32_t>(smooth ? 1 : 0);
		}
		void deserialize(Archive& ar) override {
			GeometryNode::deserializeBase(ar);
			int32_t it = iterations, sm = smooth ? 1 : 0;
			ar >> it >> sm;
			iterations = it;
			smooth = (sm != 0);
		}
	};

	// The rock-maker: pushes each vertex by 3D noise, reusing NoiseSettings wholesale
	// so FBM, Ridged, PingPong and domain warp all apply. Sampling in unit space keeps
	// feature size independent of radius; on a welded sphere the 3D field is seamless.
	class NoiseDisplaceNode : public GeometryNode {
	public:
		// How the noise sample becomes a direction.
		enum class Mode : int32_t {
			Normal    = 0,  // outward along the vertex direction
			Direction = 1,  // a fixed vector (wind / crystal)
			XYZ       = 2,  // three independent samples, one per axis (erratic)
		};

		NoiseSettings settings;
		float     amplitude = 0.2f;
		float     bias = -0.1f;
		Mode      mode = Mode::Normal;
		glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
		bool      sampleInUnitSpace = true;

		NoiseDisplaceNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Noise Displace";
			m_inputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, true));
			m_outputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, false));
		}

		Ref<const GeometryBuffer> evaluate(const GeometryEvalContext& ctx) const override {
			const GeometryBuffer* in = geom_detail::inputGeometry(ctx, 0);
			if (!in) return nullptr;

			auto out = CreateRef<GeometryBuffer>(*in);
			// Moving vertices invalidates cached normals/tangents; downstream recomputes.
			out->normals.clear();
			out->tangents.clear();

			for (size_t i = 0; i < out->positions.size(); ++i) {
				const glm::vec3 p = out->positions[i];
				const float len = glm::length(p);
				const glm::vec3 unitDir = len > 1e-8f ? p / len : glm::vec3(0.0f, 1.0f, 0.0f);
				const glm::vec3 samplePos = sampleInUnitSpace ? unitDir : p;

				if (mode == Mode::XYZ) {
					const glm::vec3 dir(
						NoiseCPU::sample(settings, samplePos.x, samplePos.y, samplePos.z),
						NoiseCPU::sample(settings, samplePos.y, samplePos.z, samplePos.x),
						NoiseCPU::sample(settings, samplePos.z, samplePos.x, samplePos.y));
					out->positions[i] = p + dir * amplitude;
					continue;
				}

				const float v = NoiseCPU::sample(settings, samplePos.x, samplePos.y, samplePos.z);
				const glm::vec3 dir = (mode == Mode::Direction) ? direction : unitDir;
				out->positions[i] = p + dir * (amplitude * v + bias);
			}
			return out;
		}

		void serialize(Archive& ar) const override {
			GeometryNode::serializeBase(ar);
			settings.serialize(ar);
			ar << amplitude << bias;
			ar << static_cast<int32_t>(mode);
			ar << direction;
			ar << static_cast<int32_t>(sampleInUnitSpace ? 1 : 0);
		}
		void deserialize(Archive& ar) override {
			GeometryNode::deserializeBase(ar);
			settings.deserialize(ar);
			ar >> amplitude >> bias;
			int32_t m = 0, u = 1;
			ar >> m >> direction >> u;
			mode = static_cast<Mode>(m);
			sampleInUnitSpace = (u != 0);
		}
	};

	// The decoupled rock-maker: identical displacement to NoiseDisplaceNode, but its
	// scalar comes from an external NoiseGraph asset instead of an inline NoiseSettings,
	// so one authored field can drive many meshes and be edited in the noise editor.
	// Samples in 3D (see NoiseNode::evaluate3), so a closed surface stays seamless.
	class FieldDisplaceNode : public GeometryNode {
	public:
		using Mode = NoiseDisplaceNode::Mode;

		// Project-relative path of a noise .veasset, resolved through ResourceManager.
		std::string noisePath;
		float     amplitude = 0.2f;
		float     bias = -0.1f;
		Mode      mode = Mode::Normal;
		glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
		bool      sampleInUnitSpace = true;

		FieldDisplaceNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Field Displace";
			m_inputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, true));
			m_outputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, false));
		}

		Ref<const GeometryBuffer> evaluate(const GeometryEvalContext& ctx) const override {
			const GeometryBuffer* in = geom_detail::inputGeometry(ctx, 0);
			if (!in) return nullptr;

			// No resolvable field yet: pass the mesh through, so a half-wired graph
			// previews as-is instead of collapsing to an error.
			if (!ensureField()) return CreateRef<GeometryBuffer>(*in);

			auto out = CreateRef<GeometryBuffer>(*in);
			out->normals.clear();
			out->tangents.clear();

			static thread_local NoisePlanScratch scratch;
			for (size_t i = 0; i < out->positions.size(); ++i) {
				const glm::vec3 p = out->positions[i];
				const float len = glm::length(p);
				const glm::vec3 unitDir = len > 1e-8f ? p / len : glm::vec3(0.0f, 1.0f, 0.0f);
				const glm::vec3 samplePos = sampleInUnitSpace ? unitDir : p;

				if (mode == Mode::XYZ) {
					const glm::vec3 dir(
						m_plan.evaluate(scratch, samplePos.x, samplePos.y, samplePos.z),
						m_plan.evaluate(scratch, samplePos.y, samplePos.z, samplePos.x),
						m_plan.evaluate(scratch, samplePos.z, samplePos.x, samplePos.y));
					out->positions[i] = p + dir * amplitude;
					continue;
				}

				const float v = m_plan.evaluate(scratch, samplePos.x, samplePos.y, samplePos.z);
				const glm::vec3 dir = (mode == Mode::Direction) ? direction : unitDir;
				out->positions[i] = p + dir * (amplitude * v + bias);
			}
			return out;
		}

		void serialize(Archive& ar) const override {
			GeometryNode::serializeBase(ar);
			ar << noisePath;
			ar << amplitude << bias;
			ar << static_cast<int32_t>(mode);
			ar << direction;
			ar << static_cast<int32_t>(sampleInUnitSpace ? 1 : 0);
		}
		void deserialize(Archive& ar) override {
			GeometryNode::deserializeBase(ar);
			ar >> noisePath;
			ar >> amplitude >> bias;
			int32_t m = 0, u = 1;
			ar >> m >> direction >> u;
			mode = static_cast<Mode>(m);
			sampleInUnitSpace = (u != 0);
			m_planValid = false;   // the path may now point at a different field
		}

	private:
		// Resolves noisePath to a NoiseGraphResource and flattens its graph into a
		// plan, rebuilt only when the resource's content hash moves. Cached so a
		// per-vertex displacement never rebuilds the plan.
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

	// Rigid/affine transform. Normals use the inverse-transpose so non-uniform scale
	// keeps them perpendicular to the moved surface.
	class TransformNode : public GeometryNode {
	public:
		glm::vec3 translate = glm::vec3(0.0f);
		glm::vec3 rotateEulerDeg = glm::vec3(0.0f);
		glm::vec3 scale = glm::vec3(1.0f);

		TransformNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Transform";
			m_inputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, true));
			m_outputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, false));
		}

		Ref<const GeometryBuffer> evaluate(const GeometryEvalContext& ctx) const override {
			const GeometryBuffer* in = geom_detail::inputGeometry(ctx, 0);
			if (!in) return nullptr;

			const glm::mat4 m =
				glm::translate(glm::mat4(1.0f), translate) *
				glm::rotate(glm::mat4(1.0f), glm::radians(rotateEulerDeg.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
				glm::rotate(glm::mat4(1.0f), glm::radians(rotateEulerDeg.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
				glm::rotate(glm::mat4(1.0f), glm::radians(rotateEulerDeg.z), glm::vec3(0.0f, 0.0f, 1.0f)) *
				glm::scale(glm::mat4(1.0f), scale);
			const glm::mat3 linear(m);
			const glm::mat3 normalMat = glm::transpose(glm::inverse(linear));

			auto out = CreateRef<GeometryBuffer>(*in);
			for (auto& p : out->positions) p = glm::vec3(m * glm::vec4(p, 1.0f));
			if (out->hasNormals())
				for (auto& n : out->normals) n = glm::normalize(normalMat * n);
			if (out->hasTangents())
				for (auto& tg : out->tangents) tg = glm::normalize(linear * tg);
			return out;
		}

		void serialize(Archive& ar) const override {
			GeometryNode::serializeBase(ar);
			ar << translate << rotateEulerDeg << scale;
		}
		void deserialize(Archive& ar) override {
			GeometryNode::deserializeBase(ar);
			ar >> translate >> rotateEulerDeg >> scale;
		}
	};

	// Terminal. Guarantees the attributes the asset needs — spherical UVs if the graph
	// never made any, smooth normals, tangents — so intermediate nodes can stay cheap.
	class MeshOutputNode : public GeometryNode {
	public:
		std::string name = "geometry";
		bool recalcNormals = true;
		bool generateUVs = true;
		bool recalcTangents = true;

		MeshOutputNode() {
			m_nodeType = NodeType::Output;
			m_displayName = "Mesh Output";
			m_inputPins.push_back(GraphPin("Geometry", {}, PinType::Geometry, true));
		}

		Ref<const GeometryBuffer> evaluate(const GeometryEvalContext& ctx) const override {
			const GeometryBuffer* in = geom_detail::inputGeometry(ctx, 0);
			if (!in) return nullptr;

			auto out = CreateRef<GeometryBuffer>(*in);
			if (generateUVs && !out->hasUVs()) out->generateSphericalUVs();
			if (recalcNormals || !out->hasNormals()) out->computeSmoothNormals();
			if (recalcTangents || !out->hasTangents()) out->computeTangents();
			return out;
		}

		void serialize(Archive& ar) const override {
			GeometryNode::serializeBase(ar);
			ar << name;
			ar << static_cast<int32_t>(recalcNormals ? 1 : 0);
			ar << static_cast<int32_t>(generateUVs ? 1 : 0);
			ar << static_cast<int32_t>(recalcTangents ? 1 : 0);
		}
		void deserialize(Archive& ar) override {
			GeometryNode::deserializeBase(ar);
			int32_t rn = 1, gu = 1, rt = 1;
			ar >> name >> rn >> gu >> rt;
			recalcNormals = (rn != 0);
			generateUVs = (gu != 0);
			recalcTangents = (rt != 0);
		}
	};

}
