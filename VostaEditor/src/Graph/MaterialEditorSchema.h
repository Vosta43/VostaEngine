#pragma once

#include "Graph/NodeGraphEditorSchema.h"
#include "Renderer/MaterialNodes.h"

namespace ve {

// Material-graph policy for the generic NodeGraphPanel: pin/node colours, inline
// value editors, the material create-node menu, the texture picker, and the
// rule that the MaterialOutput node can never be deleted.
class MaterialEditorSchema : public NodeGraphEditorSchema {
public:
	ImColor getPinColor(PinType type) const override;
	ImColor getNodeColor(NodeType type) const override;

	bool canDeleteNode(const GraphNode& node) const override;
	void onNodeDeleted(uint32_t nodeId) override;

	void drawNodeContent(GraphNode* node) override;
	void buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
		const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) override;
	void drawSuspendedContent(NodeGraph& graph) override;

private:
	// Deferred texture picker — a TextureSamplerNode awaiting texture selection,
	// rendered in the canvas's suspended popup zone to avoid node-bound clipping.
	TextureSamplerNode* m_pendingTextureNode = nullptr;
	ImVec2              m_texPickerPos;
	char                m_texFilter[128] = {};
};

}
