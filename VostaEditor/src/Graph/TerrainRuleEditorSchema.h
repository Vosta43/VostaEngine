#pragma once

#include "Graph/NodeGraphEditorSchema.h"

#include <functional>
#include <string>
#include <vector>

namespace ve {

// Terrain-rule policy for the generic NodeGraphPanel. Two node kinds: a Field Source
// (a noise asset or a terrain attribute) feeding a Layer Bands node, which splits the
// field's range into slices and routes each slice to a layer.
class TerrainRuleEditorSchema : public NodeGraphEditorSchema {
public:
	ImColor getPinColor(PinType type) const override;
	ImColor getNodeColor(NodeType type) const override;

	bool canDeleteNode(const GraphNode& node) const override;

	void drawNodeContent(GraphNode* node) override;
	void drawDetailsContent(GraphNode* node) override;
	void buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
		const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) override;
	void drawSuspendedContent(NodeGraph& graph) override;

	// Drops deferred-popup state when the graph is replaced wholesale by a file load.
	void resetNodeState();

private:
	// Parameter widgets for a node, shared by the node body and the details panel.
	// Returns false when the node kind has no parameters.
	bool drawParams(GraphNode* node, bool canvasSpace);

	void enumDropdown(const char* label, const char* const* names, int count,
		int current, std::function<void(int)> onPick, bool canvasSpace);
	void assetPicker(const char* label, const std::string& currentPath, bool canvasSpace,
		std::function<void(const std::string&)> onPick);

	// A popup cannot open from inside a node, so the button latches a request and
	// drawSuspendedContent builds it outside the canvas.
	struct PendingDropdown {
		bool                     requestOpen = false;
		ImVec2                   anchor      = ImVec2(0.0f, 0.0f);
		const char* const*       names       = nullptr;
		int                      count       = 0;
		int                      current     = 0;
		std::function<void(int)> onPick;
	} m_dropdown;

	struct PendingAssetPicker {
		bool                                    requestOpen = false;
		bool                                    open        = false;
		ImVec2                                  anchor      = ImVec2(0.0f, 0.0f);
		std::vector<std::string>                items;      // snapshotted on click
		std::function<void(const std::string&)> onPick;
	} m_assetPicker;
};

}
