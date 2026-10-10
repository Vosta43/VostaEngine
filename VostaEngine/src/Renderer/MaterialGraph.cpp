#include "vepch.h"
#include "MaterialGraph.h"
#include "MaterialNodes.h"

namespace ve {

	std::string MaterialNode::generateCode(const std::vector<std::string>&, const std::vector<std::string>&) const {
		return "";
	}

	const NodeRegistry& getMaterialNodeRegistry() {
		static const NodeRegistry registry = [] {
			NodeRegistry r;
			r.registerNode<ConstantFloatNode>((uint8_t)MaterialNodeTag::ConstantFloat);
			r.registerNode<Constant2VectorNode>((uint8_t)MaterialNodeTag::Constant2Vector);
			r.registerNode<Constant3VectorNode>((uint8_t)MaterialNodeTag::Constant3Vector);
			r.registerNode<Constant4VectorNode>((uint8_t)MaterialNodeTag::Constant4Vector);
			r.registerNode<TextureCoordinateNode>((uint8_t)MaterialNodeTag::TextureCoordinate);
			r.registerNode<TextureSamplerNode>((uint8_t)MaterialNodeTag::TextureSampler);
			r.registerNode<MultiplyNode>((uint8_t)MaterialNodeTag::Multiply);
			r.registerNode<AddNode>((uint8_t)MaterialNodeTag::Add);
			r.registerNode<SubtractNode>((uint8_t)MaterialNodeTag::Subtract);
			r.registerNode<DivideNode>((uint8_t)MaterialNodeTag::Divide);
			r.registerNode<LerpNode>((uint8_t)MaterialNodeTag::Lerp);
			r.registerNode<ClampNode>((uint8_t)MaterialNodeTag::Clamp);
			r.registerNode<MaterialOutputNode>((uint8_t)MaterialNodeTag::MaterialOutput);
			r.registerNode<TimeNode>((uint8_t)MaterialNodeTag::Time);
			r.registerNode<PowerNode>((uint8_t)MaterialNodeTag::Power);
			r.registerNode<PannerNode>((uint8_t)MaterialNodeTag::Panner);
			r.registerNode<WorldPositionNode>((uint8_t)MaterialNodeTag::WorldPosition);
			r.registerNode<VertexNormalNode>((uint8_t)MaterialNodeTag::VertexNormal);
			r.registerNode<FracNode>((uint8_t)MaterialNodeTag::Frac);
			r.registerNode<OneMinusNode>((uint8_t)MaterialNodeTag::OneMinus);
			r.registerNode<SinNode>((uint8_t)MaterialNodeTag::Sin);
			r.registerNode<CosNode>((uint8_t)MaterialNodeTag::Cos);
			r.registerNode<FloorNode>((uint8_t)MaterialNodeTag::Floor);
			r.registerNode<StepNode>((uint8_t)MaterialNodeTag::Step);
			r.registerNode<SmoothstepNode>((uint8_t)MaterialNodeTag::Smoothstep);
			r.registerNode<SaturateNode>((uint8_t)MaterialNodeTag::Saturate);
			r.registerNode<ComponentMaskNode>((uint8_t)MaterialNodeTag::ComponentMask);
			r.registerNode<AppendNode>((uint8_t)MaterialNodeTag::Append);
			return r;
		}();
		return registry;
	}

}
