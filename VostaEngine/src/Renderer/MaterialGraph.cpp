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
			return r;
		}();
		return registry;
	}

}
