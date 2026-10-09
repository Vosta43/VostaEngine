#pragma once

#include "Graph/NodeGraph.h"

#include <string>
#include <vector>

namespace ve {

	// GLSL-specific pin helpers — material codegen only.
	inline std::string getPinTypeName(PinType type) {
		switch (type) {
			case PinType::Float:  return "float";
			case PinType::Float2: return "vec2";
			case PinType::Float3: return "vec3";
			case PinType::Float4: return "vec4";
			case PinType::Sampler2D: return "sampler2D";
			default: break;
		}
		return "float";
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

	// Material node: a GraphNode that additionally knows how to emit GLSL.
	class VE_API MaterialNode : public GraphNode {
	public:
		virtual std::string generateCode(const std::vector<std::string>& inputVarNames, const std::vector<std::string>& outputVarNames) const;
	};

	// Tag <-> class table used by NodeGraph (de)serialization.
	const NodeRegistry& getMaterialNodeRegistry();

	using MaterialGraph = NodeGraph;
}
