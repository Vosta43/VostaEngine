#pragma once

#include "Core/Core.h"
#include "Scene/Archive.h"

#include <vector>
#include <string>
#include <stdint.h>
#include <unordered_map>
#include <glm.hpp>

namespace ve {

	enum class PinType : uint8_t {
		Float,
		Float2,
		Float3,
		Float4,
		Sampler2D
	};

	inline PinType GetPromotedType(PinType a, PinType b) {
		if (a == b) return a;

		if (a == PinType::Float && (int)b >= (int)PinType::Float2) return b;
		if (b == PinType::Float && (int)a >= (int)PinType::Float2) return a;

		return (int)a > (int)b ? a : b;
	}

	enum class NodeType : uint8_t {
		Input,
		Math,
		Vector,
		Texture,
		Output,
		Utility
	};

	class PinId {
	public:
		uint32_t id;
		uint32_t pinIndex;

		bool operator==(const PinId& other) const {
			return id == other.id && pinIndex == other.pinIndex;
		}
	};

	class GraphPin {
	public:
		std::string displayName;
		PinId id;
		PinType type;      // resolved type, recomputed by NodeGraph::propagateTypes
		PinType baseType;  // type declared at construction; the fallback when unconnected
		bool isInputPin;

		GraphPin(const std::string& name, PinId pinId, PinType type, bool isInputPin)
			:displayName(name), id(pinId), type(type), baseType(type), isInputPin(isInputPin) {
		}
	};

	class VE_API GraphNode {
	public:
		uint32_t id = 0;
		std::string m_displayName;
		NodeType m_nodeType = NodeType::Math;
		glm::vec2 m_pos = glm::vec2(0.0f);

		std::vector<GraphPin> m_inputPins;
		std::vector<GraphPin> m_outputPins;

		virtual ~GraphNode() = default;

		const std::vector<GraphPin> getInputPins() { return m_inputPins; }
		const std::vector<GraphPin> getOutputPins() { return m_outputPins; }

		// Value the evaluator substitutes for an unconnected input pin. Lets a node
		// whose pin is optional (a param the inline widget also owns) tell "no wire"
		// apart from a real 0. Defaults to 0, which is what a bare math node wants.
		virtual float inputDefault(uint32_t /*pinIndex*/) const { return 0.0f; }

		// Expression the code generator substitutes for an unconnected input pin,
		// or "" to fall back to the pin type's zero value. Lets a node whose input
		// is optional supply a meaningful default (mesh UVs, the engine clock, ...).
		virtual std::string inputDefaultExpr(uint32_t /*pinIndex*/) const { return {}; }

		// Whether input pins take their type from whatever is wired into them
		// (math/vector nodes) instead of keeping the declared type (typed inputs
		// such as Material Output's Base Color, whose codegen expects a fixed type).
		virtual bool adaptsInputTypes() const { return false; }

		// Recompute output pin types once input types are resolved. Nodes with a
		// fixed output type leave this empty.
		virtual void computeOutputTypes() {}

		virtual void serialize(Archive& ar) const = 0;
		virtual void deserialize(Archive& ar) = 0;

	protected:
		void serializeBase(Archive& ar) const {
			ar << static_cast<int32_t>(id) << m_pos;
			ar << m_displayName;
			ar << static_cast<int32_t>(static_cast<uint8_t>(m_nodeType));
		}
		void deserializeBase(Archive& ar) {
			int32_t tmp = 0;
			ar >> tmp; id = static_cast<uint32_t>(tmp);
			ar >> m_pos;
			ar >> m_displayName;
			ar >> tmp; m_nodeType = static_cast<NodeType>(static_cast<uint8_t>(tmp));
		}
	};

	class GraphLink {
	public:
		uint32_t id;
		PinId startPin;
		PinId endPin;

		void serialize(Archive& ar) const {
			ar << id << startPin.id << startPin.pinIndex << endPin.id << endPin.pinIndex;
		}
		void deserialize(Archive& ar) {
			ar >> id >> startPin.id >> startPin.pinIndex >> endPin.id >> endPin.pinIndex;
		}
	};

	// Maps serialization tags <-> concrete node classes, so a NodeGraph can
	// (de)serialize polymorphic nodes without knowing the concrete kinds.
	class NodeRegistry {
	public:
		using Factory = Ref<GraphNode>(*)();
		using Matcher = bool(*)(const GraphNode&);

		template<typename T>
		void registerNode(uint8_t tag) {
			m_entries[tag] = Entry{
				[]() -> Ref<GraphNode> { return CreateRef<T>(); },
				[](const GraphNode& n) { return dynamic_cast<const T*>(&n) != nullptr; }
			};
		}

		Ref<GraphNode> create(uint8_t tag) const;
		uint8_t tagOf(const GraphNode& node) const;

	private:
		struct Entry {
			Factory factory;
			Matcher matches;
		};
		std::unordered_map<uint8_t, Entry> m_entries;
	};

	class VE_API NodeGraph {
	public:
		std::vector<Ref<GraphNode>> nodes;
		std::vector<GraphLink> links;

		uint32_t m_nextNodeId = 1;
		uint32_t m_nextLinkId = 1;

		void assignPinIds(GraphNode* node);

		void addNode(Ref<GraphNode> node);

		void addNode(Ref<GraphNode> node, glm::vec2 pos);

		const char* addLink(PinId from, PinId to);

		void removeNode(uint32_t nodeId);

		void removeLink(uint32_t linkId);

		void removeLinksTo(PinId target);

		GraphNode* findNode(uint32_t id) const;

		// Typed lookup without an explicit cast at each call site.
		template<typename T>
		T* findNodeAs(uint32_t id) const {
			return dynamic_cast<T*>(findNode(id));
		}

		std::vector<Ref<GraphNode>> topologicalSort() const;

		// Re-resolve every pin's type from the wiring: adaptive inputs adopt the
		// type wired into them, then each node derives its output types. Call after
		// any link change (and after deserialize) so codegen and the editor agree.
		void propagateTypes();

		void serialize(const NodeRegistry& registry, Archive& ar) const;
		void deserialize(const NodeRegistry& registry, Archive& ar);
	};

}
