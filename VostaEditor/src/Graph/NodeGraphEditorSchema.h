#pragma once

#include "Graph/NodeGraph.h"

#include <imgui.h>
#include <functional>
#include <glm.hpp>

namespace ve {

// Application-specific policy for NodeGraphPanel. One implementation per graph
// kind (e.g. MaterialEditorSchema). The panel owns the generic canvas mechanics;
// everything that depends on the meaning of the nodes lives here.
class NodeGraphEditorSchema {
public:
	virtual ~NodeGraphEditorSchema() = default;

	virtual ImColor getPinColor(PinType type) const = 0;
	virtual ImColor getNodeColor(NodeType type) const = 0;

	// Extra validation on top of the generic output->input rule.
	virtual bool canConnect(const GraphNode& fromNode, PinId fromPin,
	                        const GraphNode& toNode, PinId toPin) const { return true; }

	// Nodes that must always exist (e.g. the material output) return false.
	virtual bool canDeleteNode(const GraphNode& node) const { return true; }
	virtual void onNodeDeleted(uint32_t nodeId) {}

	// Optional inline widgets drawn inside the node body.
	virtual void drawNodeContent(GraphNode* node) {}

	// Optional parameters view for an external details panel: the same widgets as
	// drawNodeContent, minus anything that only makes sense inside the node (a
	// collapse toggle, an inline preview). Drawn into the current window, not a canvas.
	virtual void drawDetailsContent(GraphNode* node) {}

	// Populate the background context menu; addNode places a node at a canvas position.
	virtual void buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
		const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) = 0;

	// Extra popups drawn inside ed::Suspend (e.g. a deferred texture picker).
	virtual void drawSuspendedContent(NodeGraph& graph) {}
};

}
