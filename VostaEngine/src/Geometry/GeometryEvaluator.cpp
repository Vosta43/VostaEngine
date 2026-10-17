#include "vepch.h"
#include "Geometry/GeometryEvaluator.h"
#include "Geometry/GeometryNodes.h"
#include "Sdf/SdfNodes.h"
#include "Asset/BuiltinReousrces.h"

#include <unordered_map>
#include <variant>

namespace ve {

	namespace {
		const MeshOutputNode* findOutput(const GeometryGraph& graph) {
			for (const auto& n : graph.nodes)
				if (auto* mo = dynamic_cast<MeshOutputNode*>(n.get()))
					return mo;
			return nullptr;
		}

		// What a node can leave in the cache: a mesh from a GeometryNode, or a
		// signed-distance function from an SdfNode.
		using NodeValue = std::variant<Ref<const GeometryBuffer>, SdfValue>;
	}

	Ref<const GeometryBuffer> evaluateGeometryGraphBuffer(const GeometryGraph& graph, std::string* outError) {
		auto setError = [&](const std::string& msg) { if (outError) *outError = msg; };

		// topologicalSort returns {} on a cycle, so this covers both "no nodes" and
		// "loop" with one check.
		const std::vector<Ref<GraphNode>> order = graph.topologicalSort();
		if (order.empty()) {
			setError("graph is empty or contains a cycle");
			return nullptr;
		}

		const MeshOutputNode* outputNode = findOutput(graph);
		if (!outputNode) {
			setError("no Mesh Output node");
			return nullptr;
		}

		std::unordered_map<uint32_t, NodeValue> cache;

		// The value an upstream node left on the pin feeding `pin`, or null when the
		// pin is unconnected.
		auto upstreamValue = [&](const GraphPin& pin) -> const NodeValue* {
			for (const GraphLink& link : graph.links) {
				if (link.endPin == pin.id) {
					auto it = cache.find(link.startPin.id);
					return it != cache.end() ? &it->second : nullptr;
				}
			}
			return nullptr;
		};

		for (const Ref<GraphNode>& nodeRef : order) {
			if (auto* node = dynamic_cast<GeometryNode*>(nodeRef.get())) {
				GeometryEvalContext ctx;
				ctx.inputs.resize(node->m_inputPins.size());

				for (size_t i = 0; i < node->m_inputPins.size(); ++i) {
					const GraphPin& pin = node->m_inputPins[i];
					ctx.inputs[i].scalar = node->inputDefault(static_cast<uint32_t>(i));

					const NodeValue* v = upstreamValue(pin);
					if (!v) continue;
					if (pin.type == PinType::Geometry) {
						if (const auto* g = std::get_if<Ref<const GeometryBuffer>>(v))
							ctx.inputs[i].geometry = *g;
					} else if (pin.type == PinType::Sdf) {
						if (const auto* s = std::get_if<SdfValue>(v))
							ctx.inputs[i].sdf = *s;
					}
				}

				if (Ref<const GeometryBuffer> out = node->evaluate(ctx))
					cache[node->id] = out;
			} else if (auto* node = dynamic_cast<SdfNode*>(nodeRef.get())) {
				SdfEvalContext ctx;
				ctx.inputs.resize(node->m_inputPins.size());

				for (size_t i = 0; i < node->m_inputPins.size(); ++i) {
					const NodeValue* v = upstreamValue(node->m_inputPins[i]);
					if (!v) continue;
					if (const auto* s = std::get_if<SdfValue>(v))
						ctx.inputs[i] = *s;
				}

				if (SdfValue out = node->evaluate(ctx); out.valid())
					cache[node->id] = out;
			}
		}

		auto it = cache.find(outputNode->id);
		const auto* out = (it != cache.end()) ? std::get_if<Ref<const GeometryBuffer>>(&it->second) : nullptr;
		if (!out || !*out || (*out)->empty()) {
			setError("output geometry is empty (an input is unconnected, or zero triangles)");
			return nullptr;
		}
		return *out;
	}

	Ref<StaticMeshResource> toMeshResource(const GeometryBuffer& buffer, const std::string& subMeshName) {
		auto mesh = CreateRef<StaticMeshResource>();
		mesh->vertexBuffer.resize(buffer.positions.size());
		for (size_t i = 0; i < buffer.positions.size(); ++i) {
			Vertex& v = mesh->vertexBuffer[i];
			v.position = buffer.positions[i];
			v.uv = buffer.hasUVs() ? buffer.uvs[i] : glm::vec2(0.0f);
			v.normal = buffer.hasNormals() ? buffer.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
			v.tangent = buffer.hasTangents() ? buffer.tangents[i] : glm::vec3(1.0f, 0.0f, 0.0f);
		}

		mesh->indexBuffer.reserve(buffer.indices.size());
		for (uint32_t idx : buffer.indices) mesh->indexBuffer.push_back(static_cast<int>(idx));

		SubMeshResource sub;
		sub.name = subMeshName;
		sub.firstIndex = 0;
		sub.indexCount = static_cast<uint32_t>(mesh->indexBuffer.size());
		// The built-in white material, not INVALID: the GBuffer skips a submesh with no
		// material, so a baked mesh would be invisible until one was assigned by hand.
		// store<Material> resolves the "__builtin_..." key from memory, so the path
		// survives the asset round-trip.
		sub.materialHandle = BuiltinResources::getDefaultMaterial();
		mesh->subMeshes.push_back(sub);
		return mesh;
	}

	Ref<StaticMeshResource> evaluateGeometryGraph(const GeometryGraph& graph, std::string* outError) {
		Ref<const GeometryBuffer> buffer = evaluateGeometryGraphBuffer(graph, outError);
		if (!buffer) return nullptr;

		std::string name = "geometry";
		if (const MeshOutputNode* out = findOutput(graph)) name = out->name;
		return toMeshResource(*buffer, name);
	}

}
