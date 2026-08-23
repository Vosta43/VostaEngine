#pragma once

#include "Core/Core.h"
#include "Scene/Archive.h"

#include <vector>
#include <string>
#include <stdint.h>
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

	inline std::string getPinTypeName(PinType type) {

		switch (type)
		{
			case PinType::Float:  return "float";
			case PinType::Float2: return "vec2";
			case PinType::Float3: return "vec3";
			case PinType::Float4: return "vec4";
			case PinType::Sampler2D: return "sampler2D";

		default:
			break;
		}
	}

	inline std::string getDefaultValue(PinType type) {
		switch (type) {
		case ve::PinType::Float:
			return "0.0";
		case ve::PinType::Float2:
			return "vec2(0.0)";
		case ve::PinType::Float3:
			return "vec3(0.0)";
		case ve::PinType::Float4:
			return "vec4(0.0)";
		case ve::PinType::Sampler2D:
			return "0";
		default:
			return "0.0";
		}
	}

	enum class NodeType : uint8_t {
		Input,
		Math,
		Vector,
		Texture,
		Output,
		Utility
	};

	// Serialization type tags — one per concrete MaterialNode subclass.
	enum class MaterialNodeTag : uint8_t {
		ConstantFloat     = 0,
		Constant2Vector   = 1,
		Constant3Vector   = 2,
		Constant4Vector   = 3,
		TextureCoordinate = 4,
		TextureSampler    = 5,
		Multiply          = 6,
		Add               = 7,
		Subtract          = 8,
		Divide            = 9,
		Lerp              = 10,
		Clamp             = 11,
		MaterialOutput    = 12,
	};

	class PinId {
	public:
		uint32_t id;
		uint32_t pinIndex;

		bool operator==(const PinId& other) const {
			return id == other.id && pinIndex == other.pinIndex;
		}
	};

	class MaterialPin {
	public:
		std::string displayName;
		PinId id;
		PinType type;
		bool isInputPin;

		MaterialPin(const std::string& name,PinId pinId,PinType type,bool isInputPin)
			:displayName(name),id(pinId),type(type), isInputPin(isInputPin){

		}
	};


	class VE_API MaterialNode {
	public:
		uint32_t id;
		std::string m_displayName;
		NodeType m_nodeType = NodeType::Math;
		glm::vec2 m_pos = glm::vec2(0.0f);

		std::vector<MaterialPin> m_inputPins;
		std::vector<MaterialPin> m_outputPins;

		const std::vector<MaterialPin> getInputPins() { return m_inputPins; }
		const std::vector<MaterialPin> getOutputPins() { return m_outputPins; }

		virtual std::string generateCode(const std::vector<std::string>& inputVarNames, const std::vector<std::string>& outputVarNames) const;

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


	class MaterialLink {
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

	// Factory: instantiate the correct node subclass from a type tag.
	Ref<MaterialNode> createMaterialNode(MaterialNodeTag tag);

	class VE_API MaterialGraph {
	public:
		std::vector<Ref<MaterialNode>> nodes;
		std::vector<MaterialLink> links;

		uint32_t m_nextNodeId = 1;
		uint32_t m_nextLinkId = 1;

		void assignPinIds(MaterialNode* node);

		void addNode(Ref<MaterialNode> node);

		void addNode(Ref<MaterialNode> node,glm::vec2 pos);

		const char* addLink(PinId from, PinId to);

		void removeNode(uint32_t nodeId);

		void removeLink(uint32_t linkId);

		void removeLinksTo(PinId target);

		MaterialNode* findNode(uint32_t id) const;

		std::vector<Ref<MaterialNode>> topologicalSort() const;

		void serialize(Archive& ar) const;
		void deserialize(Archive& ar);
	};

}
