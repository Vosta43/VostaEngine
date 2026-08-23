#include "vepch.h"
#include "MaterialGraph.h"
#include "MaterialNodes.h"
#include "Core/Log.h"
#include <unordered_map>
#include <glm.hpp>

namespace ve {

	std::string MaterialNode::generateCode(const std::vector<std::string>&, const std::vector<std::string>&) const {
		return "";
	}

	// --- Node factory ---
	Ref<MaterialNode> createMaterialNode(MaterialNodeTag tag) {
		switch (tag) {
			case MaterialNodeTag::ConstantFloat:     return CreateRef<ConstantFloatNode>();
			case MaterialNodeTag::Constant2Vector:   return CreateRef<Constant2VectorNode>();
			case MaterialNodeTag::Constant3Vector:   return CreateRef<Constant3VectorNode>();
			case MaterialNodeTag::Constant4Vector:   return CreateRef<Constant4VectorNode>();
			case MaterialNodeTag::TextureCoordinate: return CreateRef<TextureCoordinateNode>();
			case MaterialNodeTag::TextureSampler:    return CreateRef<TextureSamplerNode>();
			case MaterialNodeTag::Multiply:          return CreateRef<MultiplyNode>();
			case MaterialNodeTag::Add:               return CreateRef<AddNode>();
			case MaterialNodeTag::Subtract:          return CreateRef<SubtractNode>();
			case MaterialNodeTag::Divide:            return CreateRef<DivideNode>();
			case MaterialNodeTag::Lerp:              return CreateRef<LerpNode>();
			case MaterialNodeTag::Clamp:             return CreateRef<ClampNode>();
			case MaterialNodeTag::MaterialOutput:    return CreateRef<MaterialOutputNode>();
		}
		return nullptr;
	}

	void MaterialGraph::assignPinIds(MaterialNode* node) {
		for (size_t i = 0; i < node->m_inputPins.size(); ++i) {
			node->m_inputPins[i].id = { node->id, (uint32_t)i };
		}
		// Offset output pin indices so they never collide with input pin indices
		// on the same node.  Without this, an input and output can share the same
		// PinId, which breaks the node editor's pin hit-testing.
		uint32_t base = (uint32_t)node->m_inputPins.size();
		for (size_t i = 0; i < node->m_outputPins.size(); ++i) {
			node->m_outputPins[i].id = { node->id, base + (uint32_t)i };
		}
	}

	void MaterialGraph::addNode(Ref<MaterialNode> node){

		node->id = m_nextNodeId++;
		assignPinIds(node.get());
		nodes.push_back(node);
	}

	void MaterialGraph::addNode(Ref<MaterialNode> node, glm::vec2 pos){

		node->id = m_nextNodeId++;
		node->m_pos = pos;
		assignPinIds(node.get());
		nodes.push_back(node);
	}

	const char* MaterialGraph::addLink(PinId from, PinId to)
	{
		for (auto& link : links) {
			if (link.endPin == to) {
				return "Input pin already connected";
			}
		}

		MaterialLink link;
		link.id = m_nextLinkId++;
		link.startPin = from;
		link.endPin = to;
		links.push_back(link);
		return nullptr;
	}

	void MaterialGraph::removeNode(uint32_t nodeId) {

		links.erase(std::remove_if(links.begin(), links.end(),
			[nodeId](const MaterialLink& l) {
				return l.startPin.id == nodeId || l.endPin.id == nodeId;
			}), links.end());


		nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
			[nodeId](const Ref<MaterialNode>& n) {
				return n->id == nodeId;
			}), nodes.end());
	}

	void MaterialGraph::removeLink(uint32_t linkId) {
		links.erase(std::remove_if(links.begin(), links.end(),
			[linkId](const MaterialLink& l) { return l.id == linkId; }),
			links.end());
	}

	void MaterialGraph::removeLinksTo(PinId target) {
		links.erase(std::remove_if(links.begin(), links.end(),
			[target](const MaterialLink& l) { return l.endPin == target; }),
			links.end());
	}

	MaterialNode* MaterialGraph::findNode(uint32_t id) const {
		for (auto& node : nodes) {
			if (node->id == id) return node.get();
		}
		return nullptr;
	}


	std::vector<Ref<MaterialNode>> MaterialGraph::topologicalSort() const {
		
		std::unordered_map<uint32_t,int> inDegree;
		std::unordered_map<uint32_t,Ref<MaterialNode>> nodeMap;

		for (auto& node : nodes) {
			inDegree[node->id] = 0;
			nodeMap[node->id] = node;
		}

		for (auto& link : links) {
			inDegree[link.endPin.id] ++;
		}

		std::vector<uint32_t> queue;
		for (auto& node : inDegree) {
			if (node.second == 0) {
				queue.push_back(node.first);
			}
		}

		std::vector<Ref<MaterialNode>> sorted;
		while (!queue.empty()) {
			uint32_t id = queue.front();
			queue.erase(queue.begin());
			sorted.push_back(nodeMap[id]);

			for (auto& link : links) {
				if (link.startPin.id == id) {
					inDegree[link.endPin.id]--;
					if (inDegree[link.endPin.id] == 0) { 
						queue.push_back(link.endPin.id);
					}
				}
			}
		}

		if (sorted.size() != nodes.size()) {
			VE_CORE_ERROR_PRINT("MaterialGraph contains a cycle!");
			return {};
		}
		return sorted;

	}

	// --- Serialization ---

	static MaterialNodeTag getNodeTag(const MaterialNode* node) {
		if (dynamic_cast<const ConstantFloatNode*>(node))     return MaterialNodeTag::ConstantFloat;
		if (dynamic_cast<const Constant2VectorNode*>(node))   return MaterialNodeTag::Constant2Vector;
		if (dynamic_cast<const Constant3VectorNode*>(node))   return MaterialNodeTag::Constant3Vector;
		if (dynamic_cast<const Constant4VectorNode*>(node))   return MaterialNodeTag::Constant4Vector;
		if (dynamic_cast<const TextureCoordinateNode*>(node)) return MaterialNodeTag::TextureCoordinate;
		if (dynamic_cast<const TextureSamplerNode*>(node))    return MaterialNodeTag::TextureSampler;
		if (dynamic_cast<const MultiplyNode*>(node))          return MaterialNodeTag::Multiply;
		if (dynamic_cast<const AddNode*>(node))               return MaterialNodeTag::Add;
		if (dynamic_cast<const SubtractNode*>(node))          return MaterialNodeTag::Subtract;
		if (dynamic_cast<const DivideNode*>(node))            return MaterialNodeTag::Divide;
		if (dynamic_cast<const LerpNode*>(node))              return MaterialNodeTag::Lerp;
		if (dynamic_cast<const ClampNode*>(node))             return MaterialNodeTag::Clamp;
		if (dynamic_cast<const MaterialOutputNode*>(node))    return MaterialNodeTag::MaterialOutput;
		return MaterialNodeTag::ConstantFloat; // fallback
	}

	void MaterialGraph::serialize(Archive& ar) const {
		uint32_t nodeCount = static_cast<uint32_t>(nodes.size());
		ar << nodeCount;
		for (auto& node : nodes) {
			uint8_t tag = static_cast<uint8_t>(getNodeTag(node.get()));
			ar << tag;
			node->serialize(ar);
		}

		uint32_t linkCount = static_cast<uint32_t>(links.size());
		ar << linkCount;
		for (auto& link : links) {
			link.serialize(ar);
		}
	}

	void MaterialGraph::deserialize(Archive& ar) {
		nodes.clear();
		links.clear();

		uint32_t nodeCount = 0;
		ar >> nodeCount;

		uint32_t maxId = 0;
		for (uint32_t i = 0; i < nodeCount; ++i) {
			uint8_t tagByte = 0;
			ar >> tagByte;

			auto node = createMaterialNode(static_cast<MaterialNodeTag>(tagByte));
			if (!node) continue;

			node->deserialize(ar);
			if (node->id > maxId) maxId = node->id;

			assignPinIds(node.get());
			nodes.push_back(node);
		}
		m_nextNodeId = maxId + 1;

		uint32_t linkCount = 0;
		ar >> linkCount;

		uint32_t maxLinkId = 0;
		for (uint32_t i = 0; i < linkCount; ++i) {
			MaterialLink link;
			link.deserialize(ar);
			if (link.id > maxLinkId) maxLinkId = link.id;
			links.push_back(link);
		}
		m_nextLinkId = maxLinkId + 1;
	}

}
