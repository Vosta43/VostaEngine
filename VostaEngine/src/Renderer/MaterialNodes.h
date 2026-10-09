#pragma once

#include "Core/AssetHandle.h"
#include "Core/ResourceManager.h"
#include "MaterialGraph.h"
#include "Renderer/Texture.h"

#include <glm.hpp>

namespace ve {

	class ConstantFloatNode : public MaterialNode {
	public:
		float value = 0.0f;

		ConstantFloatNode() {
			m_nodeType = NodeType::Vector;
			m_displayName = "Float";
			m_outputPins.push_back(GraphPin("Float",{},PinType::Float,false));
		}

		std::string generateCode(const std::vector<std::string>& inputVarNames, const std::vector<std::string>& outputVarNames) const override {
			return "float " + outputVarNames[0] + " = " + std::to_string(value) + ";\n";
		}

		void serialize(Archive& ar) const override {
			MaterialNode::serializeBase(ar);
			ar << value;
		}
		void deserialize(Archive& ar) override {
			MaterialNode::deserializeBase(ar);
			ar >> value;
		}
	};

	class TextureSamplerNode : public MaterialNode {
	public:
		AssetHandle textureHandle;
		std::string samplerName;

		TextureSamplerNode() {
			m_nodeType = NodeType::Texture;
			m_displayName = "TextureSampler";
			m_inputPins.push_back(GraphPin("UVs", {}, PinType::Float2, true));
			m_outputPins.push_back(GraphPin("RGBA", {}, PinType::Float4, false));
			m_outputPins.push_back(GraphPin("R", {}, PinType::Float, false));
			m_outputPins.push_back(GraphPin("G", {}, PinType::Float, false));
			m_outputPins.push_back(GraphPin("B", {}, PinType::Float, false));
			m_outputPins.push_back(GraphPin("A", {}, PinType::Float, false));
		}

		std::string generateSamplerDeclaration(int texSlot) const {
			return "uniform sampler2D " + samplerName + ";\n";
		}

		std::string generateCode( const std::vector<std::string>& inputVarNames,
			const std::vector<std::string>& outputVarNames) const override {

			std::string uvVar = inputVarNames[0];

			if (inputVarNames[0].find("DEFAULT_") == 0) {
				uvVar = "v_TexCoord";
			}

			return
				"vec4 " + outputVarNames[0] + " = texture(" +
				samplerName + ", " + uvVar + ");\n" +
				"float " + outputVarNames[1] + " = " + outputVarNames[0] + ".r;\n" +
				"float " + outputVarNames[2] + " = " + outputVarNames[0] + ".g;\n" +
				"float " + outputVarNames[3] + " = " + outputVarNames[0] + ".b;\n" +
				"float " + outputVarNames[4] + " = " + outputVarNames[0] + ".a;\n";
		}

		void serialize(Archive& ar) const override {
			MaterialNode::serializeBase(ar);
			std::string path = ResourceManager::getPath<Texture2D>(textureHandle);
			ar << path;
		}
		void deserialize(Archive& ar) override {
			MaterialNode::deserializeBase(ar);
			std::string path;
			ar >> path;
			if (!path.empty()) {
				textureHandle = ResourceManager::store<Texture2D>(path);
			}
		}
	};

	class MultiplyNode : public MaterialNode {
	public:
		MultiplyNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Multiply";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}

		std::string generateCode(const std::vector<std::string>& inputVarNames,
			const std::vector<std::string>& outputVarNames) const override {

			PinType outType = GetPromotedType(m_inputPins[0].type, m_inputPins[1].type);

			return getPinTypeName(outType) + std::string(" ") + outputVarNames[0] + " = " + inputVarNames[0] + " * " + inputVarNames[1] + ";\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class MaterialOutputNode : public MaterialNode {
	public:
		MaterialOutputNode() {
			m_nodeType = NodeType::Output;
			m_displayName = "Material Output";

			m_inputPins.push_back(GraphPin("Base Color", {}, PinType::Float3, true));
			m_inputPins.push_back(GraphPin("Metallic", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Roughness", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Ambient Occlusion", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Normal", {}, PinType::Float3, true));
			m_inputPins.push_back(GraphPin("Emissive", {}, PinType::Float3, true));
			m_inputPins.push_back(GraphPin("Opacity Mask", {}, PinType::Float, true));
		}

		std::string generateCode(
			const std::vector<std::string>& in,
			const std::vector<std::string>&) const override
		{
			auto v = [&](int i, const char* def) {
				return in[i].find("DEFAULT_") == 0 ? def : in[i];
				};
			return
				"vec3  baseColor = " + v(0, "vec3(1.0)") + ";\n"
				"float metallic  = " + v(1, "0.0") + ";\n"
				"float roughness = " + v(2, "0.5") + ";\n"
				"float ao        = " + v(3, "1.0") + ";\n"
				"vec3  emissive  = " + v(5, "vec3(0.0)") + ";\n"
				"vec3  tangentNormal = normalize(" + v(4, "vec3(0.5, 0.5, 1.0)") + " * 2.0 - 1.0);\n"
				"vec3  worldNormal = normalize(v_TBN * tangentNormal);\n"
				"vec3  encodedNormal = worldNormal * 0.5 + 0.5;\n"
				"Frag0_Albedo   = vec4(baseColor, 1.0);\n"
				"Frag1_Normal   = vec4(encodedNormal, 0.0);\n"
				"Frag2_Material = vec4(metallic, roughness, ao, 0.0);\n"
				"Frag3_Emissive = vec4(emissive, 1.0);\n"
				"gl_FragDepth   = gl_FragCoord.z;\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class LerpNode : public MaterialNode {
	public:
		LerpNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Lerp";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("T", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
			const std::vector<std::string>& out) const override {
			PinType t = GetPromotedType(
				GetPromotedType(m_inputPins[0].type, m_inputPins[1].type),
				m_inputPins[2].type);
			return getPinTypeName(t) + " " + out[0] + " = mix(" + in[0] + ", " + in[1] + ", " + in[2] + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class Constant2VectorNode : public MaterialNode {
	public:
		glm::vec2 value = glm::vec2(0.0f);
		Constant2VectorNode() {
			m_nodeType = NodeType::Vector;
			m_displayName = "Vector2";
			m_outputPins.push_back(GraphPin("Vec2", {}, PinType::Float2, false));
		}
		std::string generateCode(const std::vector<std::string>&,
		                         const std::vector<std::string>& out) const override {
			return "vec2 " + out[0] + " = vec2(" +
			       std::to_string(value.x) + ", " + std::to_string(value.y) + ");\n";
		}

		void serialize(Archive& ar) const override {
			MaterialNode::serializeBase(ar);
			ar << value;
		}
		void deserialize(Archive& ar) override {
			MaterialNode::deserializeBase(ar);
			ar >> value;
		}
	};

	class Constant3VectorNode : public MaterialNode {
	public:
		glm::vec3 value = glm::vec3(0.0f);
		Constant3VectorNode() {
			m_nodeType = NodeType::Vector;
			m_displayName = "Vector3";
			m_outputPins.push_back(GraphPin("Vec3", {}, PinType::Float3, false));
		}
		std::string generateCode(const std::vector<std::string>&,
		                         const std::vector<std::string>& out) const override {
			return "vec3 " + out[0] + " = vec3(" +
			       std::to_string(value.x) + ", " + std::to_string(value.y) + ", " +
			       std::to_string(value.z) + ");\n";
		}

		void serialize(Archive& ar) const override {
			MaterialNode::serializeBase(ar);
			ar << value;
		}
		void deserialize(Archive& ar) override {
			MaterialNode::deserializeBase(ar);
			ar >> value;
		}
	};

	class Constant4VectorNode : public MaterialNode {
	public:
		glm::vec4 value = glm::vec4(0.0f);
		Constant4VectorNode() {
			m_nodeType = NodeType::Vector;
			m_displayName = "Vector4";
			m_outputPins.push_back(GraphPin("Vec4", {}, PinType::Float4, false));
		}
		std::string generateCode(const std::vector<std::string>&,
		                         const std::vector<std::string>& out) const override {
			return "vec4 " + out[0] + " = vec4(" +
			       std::to_string(value.x) + ", " + std::to_string(value.y) + ", " +
			       std::to_string(value.z) + ", " + std::to_string(value.w) + ");\n";
		}

		void serialize(Archive& ar) const override {
			MaterialNode::serializeBase(ar);
			ar << value;
		}
		void deserialize(Archive& ar) override {
			MaterialNode::deserializeBase(ar);
			ar >> value;
		}
	};

	class AddNode : public MaterialNode {
	public:
		AddNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Add";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			PinType t = GetPromotedType(m_inputPins[0].type, m_inputPins[1].type);
			return getPinTypeName(t) + " " + out[0] + " = " + in[0] + " + " + in[1] + ";\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class SubtractNode : public MaterialNode {
	public:
		SubtractNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Subtract";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			PinType t = GetPromotedType(m_inputPins[0].type, m_inputPins[1].type);
			return getPinTypeName(t) + " " + out[0] + " = " + in[0] + " - " + in[1] + ";\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class DivideNode : public MaterialNode {
	public:
		DivideNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Divide";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			PinType t = GetPromotedType(m_inputPins[0].type, m_inputPins[1].type);
			return getPinTypeName(t) + " " + out[0] + " = " + in[0] + " / " + in[1] + ";\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class ClampNode : public MaterialNode {
	public:
		ClampNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Clamp";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Min",   {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Max",   {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			PinType t = GetPromotedType(
				GetPromotedType(m_inputPins[0].type, m_inputPins[1].type),
				m_inputPins[2].type);
			return getPinTypeName(t) + " " + out[0] +
			       " = clamp(" + in[0] + ", " + in[1] + ", " + in[2] + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class TextureCoordinateNode : public MaterialNode {
	public:
		glm::vec2 uvScale = glm::vec2(1.0f);

		TextureCoordinateNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "TexCoord";
			m_outputPins.push_back(GraphPin("UV", {}, PinType::Float2, false));
		}
		std::string generateCode(const std::vector<std::string>&,
		                         const std::vector<std::string>& out) const override {
			return "vec2 " + out[0] + " = v_TexCoord * vec2(" +
			       std::to_string(uvScale.x) + ", " + std::to_string(uvScale.y) + ");\n";
		}

		void serialize(Archive& ar) const override {
			MaterialNode::serializeBase(ar);
			ar << uvScale;
		}
		void deserialize(Archive& ar) override {
			MaterialNode::deserializeBase(ar);
			ar >> uvScale;
		}
	};

}
