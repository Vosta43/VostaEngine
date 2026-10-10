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

		// Unconnected UVs: sample with the mesh's interpolated texture coordinates.
		std::string inputDefaultExpr(uint32_t pinIndex) const override {
			return pinIndex == 0 ? std::string("v_TexCoord") : std::string();
		}

		std::string generateSamplerDeclaration(int texSlot) const {
			return "uniform sampler2D " + samplerName + ";\n";
		}

		std::string generateCode( const std::vector<std::string>& inputVarNames,
			const std::vector<std::string>& outputVarNames) const override {

			std::string uvVar = inputVarNames[0];

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

	// Base for nodes whose pin types follow their connections: every input adopts
	// the type wired into it, and the whole node works at the promoted input type.
	class AdaptiveMathNode : public MaterialNode {
	public:
		bool adaptsInputTypes() const override { return true; }

		void computeOutputTypes() override {
			if (m_outputPins.empty() || m_inputPins.empty()) return;

			// Promote every pin to the widest type in play and declare the output
			// at it. Codegen converts each operand to the pin type, so mixed widths
			// (vec3 * float, vec2 + vec4) align instead of failing to compile.
			PinType t = m_inputPins[0].type;
			for (size_t i = 1; i < m_inputPins.size(); ++i)
				t = GetPromotedType(t, m_inputPins[i].type);
			for (auto& pin : m_inputPins) pin.type = t;
			m_outputPins[0].type = t;
		}
	};

	class MultiplyNode : public AdaptiveMathNode {
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

	class LerpNode : public AdaptiveMathNode {
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

	class AddNode : public AdaptiveMathNode {
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

	class SubtractNode : public AdaptiveMathNode {
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

	class DivideNode : public AdaptiveMathNode {
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

	class PowerNode : public AdaptiveMathNode {
	public:
		PowerNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Power";
			m_inputPins.push_back(GraphPin("Base", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Exp",  {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}

		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {

			PinType a = m_inputPins[0].type;
			PinType b = m_inputPins[1].type;
			PinType t = GetPromotedType(a, b);
			std::string vecN = getPinTypeName(t);

			std::string base = in[0];
			std::string exp  = in[1];
			// GLSL pow() takes both operands as the same genType; a scalar needs
			// an explicit vecN() wrap or strict drivers reject pow(vec3, float).
			if (t != PinType::Float) {
				if (a == PinType::Float) base = vecN + "(" + base + ")";
				if (b == PinType::Float) exp  = vecN + "(" + exp  + ")";
			}
			return vecN + " " + out[0] + " = pow(" + base + ", " + exp + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class ClampNode : public AdaptiveMathNode {
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

	class TimeNode : public MaterialNode {
	public:
		TimeNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "Time";
			m_outputPins.push_back(GraphPin("Time", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>&,
		                         const std::vector<std::string>& out) const override {
			return "float " + out[0] + " = u_Time;\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class PannerNode : public MaterialNode {
	public:
		glm::vec2 speed = glm::vec2(1.0f, 0.0f);

		PannerNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "Panner";
			m_inputPins.push_back(GraphPin("UV",   {}, PinType::Float2, true));
			m_inputPins.push_back(GraphPin("Time", {}, PinType::Float,  true));
			m_outputPins.push_back(GraphPin("UV", {}, PinType::Float2, false));
		}

		// Unconnected: pan the mesh UVs with the engine clock.
		std::string inputDefaultExpr(uint32_t pinIndex) const override {
			if (pinIndex == 0) return "v_TexCoord";
			if (pinIndex == 1) return "u_Time";
			return {};
		}

		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return "vec2 " + out[0] + " = " + in[0] + " + " + in[1] + " * vec2(" +
			       std::to_string(speed.x) + ", " + std::to_string(speed.y) + ");\n";
		}

		void serialize(Archive& ar) const override {
			MaterialNode::serializeBase(ar);
			ar << speed;
		}
		void deserialize(Archive& ar) override {
			MaterialNode::deserializeBase(ar);
			ar >> speed;
		}
	};

	class WorldPositionNode : public MaterialNode {
	public:
		WorldPositionNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "World Position";
			m_outputPins.push_back(GraphPin("WorldPos", {}, PinType::Float3, false));
		}
		std::string generateCode(const std::vector<std::string>&,
		                         const std::vector<std::string>& out) const override {
			return "vec3 " + out[0] + " = v_WorldPos;\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class VertexNormalNode : public MaterialNode {
	public:
		VertexNormalNode() {
			m_nodeType = NodeType::Input;
			m_displayName = "Vertex Normal";
			m_outputPins.push_back(GraphPin("Normal", {}, PinType::Float3, false));
		}
		std::string generateCode(const std::vector<std::string>&,
		                         const std::vector<std::string>& out) const override {
			return "vec3 " + out[0] + " = normalize(v_TBN[2]);\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	// Element-wise unary math. The emitted type follows the input pin's type, so
	// these become vector ops for free once pin types propagate on connect.
	class FracNode : public AdaptiveMathNode {
	public:
		FracNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Frac";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return getPinTypeName(m_inputPins[0].type) + " " + out[0] + " = fract(" + in[0] + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class OneMinusNode : public AdaptiveMathNode {
	public:
		OneMinusNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "One Minus";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return getPinTypeName(m_inputPins[0].type) + " " + out[0] + " = 1.0 - " + in[0] + ";\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class SinNode : public AdaptiveMathNode {
	public:
		SinNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Sin";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return getPinTypeName(m_inputPins[0].type) + " " + out[0] + " = sin(" + in[0] + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class CosNode : public AdaptiveMathNode {
	public:
		CosNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Cos";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return getPinTypeName(m_inputPins[0].type) + " " + out[0] + " = cos(" + in[0] + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class FloorNode : public AdaptiveMathNode {
	public:
		FloorNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Floor";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return getPinTypeName(m_inputPins[0].type) + " " + out[0] + " = floor(" + in[0] + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class StepNode : public AdaptiveMathNode {
	public:
		StepNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Step";
			m_inputPins.push_back(GraphPin("Edge",  {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			PinType e = m_inputPins[0].type, v = m_inputPins[1].type;
			PinType t = GetPromotedType(e, v);
			std::string edge = in[0], val = in[1];
			// GLSL step() needs both operands the same genType; wrap scalars.
			if (t != PinType::Float) {
				std::string vecN = getPinTypeName(t);
				if (e == PinType::Float) edge = vecN + "(" + edge + ")";
				if (v == PinType::Float) val  = vecN + "(" + val  + ")";
			}
			return getPinTypeName(t) + " " + out[0] + " = step(" + edge + ", " + val + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class SmoothstepNode : public AdaptiveMathNode {
	public:
		SmoothstepNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Smoothstep";
			m_inputPins.push_back(GraphPin("Min",   {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Max",   {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			PinType t = GetPromotedType(
				GetPromotedType(m_inputPins[0].type, m_inputPins[1].type),
				m_inputPins[2].type);
			std::string a = in[0], b = in[1], c = in[2];
			// GLSL smoothstep() needs all three operands the same genType.
			if (t != PinType::Float) {
				std::string vecN = getPinTypeName(t);
				if (m_inputPins[0].type == PinType::Float) a = vecN + "(" + a + ")";
				if (m_inputPins[1].type == PinType::Float) b = vecN + "(" + b + ")";
				if (m_inputPins[2].type == PinType::Float) c = vecN + "(" + c + ")";
			}
			return getPinTypeName(t) + " " + out[0] +
			       " = smoothstep(" + a + ", " + b + ", " + c + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class SaturateNode : public AdaptiveMathNode {
	public:
		SaturateNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Saturate";
			m_inputPins.push_back(GraphPin("Value", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return getPinTypeName(m_inputPins[0].type) + " " + out[0] +
			       " = clamp(" + in[0] + ", 0.0, 1.0);\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class ComponentMaskNode : public MaterialNode {
	public:
		ComponentMaskNode() {
			m_nodeType = NodeType::Utility;
			m_displayName = "Component Mask";
			m_inputPins.push_back(GraphPin("Input", {}, PinType::Float4, true));
			m_outputPins.push_back(GraphPin("R", {}, PinType::Float, false));
			m_outputPins.push_back(GraphPin("G", {}, PinType::Float, false));
			m_outputPins.push_back(GraphPin("B", {}, PinType::Float, false));
			m_outputPins.push_back(GraphPin("A", {}, PinType::Float, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return
				"float " + out[0] + " = " + in[0] + ".r;\n" +
				"float " + out[1] + " = " + in[0] + ".g;\n" +
				"float " + out[2] + " = " + in[0] + ".b;\n" +
				"float " + out[3] + " = " + in[0] + ".a;\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

	class AppendNode : public MaterialNode {
	public:
		AppendNode() {
			m_nodeType = NodeType::Math;
			m_displayName = "Append";
			m_inputPins.push_back(GraphPin("A", {}, PinType::Float, true));
			m_inputPins.push_back(GraphPin("B", {}, PinType::Float, true));
			m_outputPins.push_back(GraphPin("Result", {}, PinType::Float2, false));
		}
		std::string generateCode(const std::vector<std::string>& in,
		                         const std::vector<std::string>& out) const override {
			return "vec2 " + out[0] + " = vec2(" + in[0] + ", " + in[1] + ");\n";
		}

		void serialize(Archive& ar) const override   { MaterialNode::serializeBase(ar); }
		void deserialize(Archive& ar) override       { MaterialNode::deserializeBase(ar); }
	};

}
