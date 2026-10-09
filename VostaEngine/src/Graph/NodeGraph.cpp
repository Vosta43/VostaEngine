#include "vepch.h"
#include "Graph/NodeGraph.h"

#include "Core/Log.h"
#include <algorithm>
#include <glm.hpp>

namespace ve {

	// --- NodeRegistry ---

	Ref<GraphNode> NodeRegistry::create(uint8_t tag) const {
		auto it = m_entries.find(tag);
		return it == m_entries.end() ? nullptr : it->second.factory();
	}

	uint8_t NodeRegistry::tagOf(const GraphNode& node) const {
		for (const auto& entry : m_entries) {
			if (entry.second.matches(node)) return entry.first;
		}
		return 0xFF;
	}

	// --- NodeGraph ---

	void NodeGraph::assignPinIds(GraphNode* node) {
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

	void NodeGraph::addNode(Ref<GraphNode> node) {

		node->id = m_nextNodeId++;
		assignPinIds(node.get());
		nodes.push_back(node);
	}

	void NodeGraph::addNode(Ref<GraphNode> node, glm::vec2 pos) {

		node->id = m_nextNodeId++;
		node->m_pos = pos;
		assignPinIds(node.get());
		nodes.push_back(node);
	}

	const char* NodeGraph::addLink(PinId from, PinId to)
	{
		for (auto& link : links) {
			if (link.endPin == to) {
				return "Input pin already connected";
			}
		}

		GraphLink link;
		link.id = m_nextLinkId++;
		link.startPin = from;
		link.endPin = to;
		links.push_back(link);
		return nullptr;
	}

	void NodeGraph::removeNode(uint32_t nodeId) {

		links.erase(std::remove_if(links.begin(), links.end(),
			[nodeId](const GraphLink& l) {
				return l.startPin.id == nodeId || l.endPin.id == nodeId;
			}), links.end());


		nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
			[nodeId](const Ref<GraphNode>& n) {
				return n->id == nodeId;
			}), nodes.end());
	}

	void NodeGraph::removeLink(uint32_t linkId) {
		links.erase(std::remove_if(links.begin(), links.end(),
			[linkId](const GraphLink& l) { return l.id == linkId; }),
			links.end());
	}

	void NodeGraph::removeLinksTo(PinId target) {
		links.erase(std::remove_if(links.begin(), links.end(),
			[target](const GraphLink& l) { return l.endPin == target; }),
			links.end());
	}

	GraphNode* NodeGraph::findNode(uint32_t id) const {
		for (auto& node : nodes) {
			if (node->id == id) return node.get();
		}
		return nullptr;
	}


	std::vector<Ref<GraphNode>> NodeGraph::topologicalSort() const {

		std::unordered_map<uint32_t, int> inDegree;
		std::unordered_map<uint32_t, Ref<GraphNode>> nodeMap;

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

		std::vector<Ref<GraphNode>> sorted;
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
			VE_CORE_ERROR_PRINT("NodeGraph contains a cycle!");
			return {};
		}
		return sorted;

	}

	// --- Serialization ---

	void NodeGraph::serialize(const NodeRegistry& registry, Archive& ar) const {
		uint32_t nodeCount = static_cast<uint32_t>(nodes.size());
		ar << nodeCount;
		for (auto& node : nodes) {
			uint8_t tag = registry.tagOf(*node);
			ar << tag;
			node->serialize(ar);
		}

		uint32_t linkCount = static_cast<uint32_t>(links.size());
		ar << linkCount;
		for (auto& link : links) {
			link.serialize(ar);
		}
	}

	void NodeGraph::deserialize(const NodeRegistry& registry, Archive& ar) {
		nodes.clear();
		links.clear();

		uint32_t nodeCount = 0;
		ar >> nodeCount;

		uint32_t maxId = 0;
		for (uint32_t i = 0; i < nodeCount; ++i) {
			uint8_t tagByte = 0;
			ar >> tagByte;

			auto node = registry.create(tagByte);
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
			GraphLink link;
			link.deserialize(ar);
			if (link.id > maxLinkId) maxLinkId = link.id;
			links.push_back(link);
		}
		m_nextLinkId = maxLinkId + 1;
	}

}
