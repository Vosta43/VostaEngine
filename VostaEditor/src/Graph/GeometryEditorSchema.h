#pragma once

#include "Graph/NodeGraphEditorSchema.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ve {

struct NoiseSettings;

// Geometry-graph policy for the generic NodeGraphPanel: pin/node colours, inline
// parameters on every node kind (including the shared NoiseSettings form on a Noise
// Displace node), the create menu, and the rule that the Mesh Output terminal can
// never be deleted. Self-contained, mirroring how the material and noise schemas each
// own their widgets rather than sharing a UI layer.
class GeometryEditorSchema : public NodeGraphEditorSchema {
public:
	ImColor getPinColor(PinType type) const override;
	ImColor getNodeColor(NodeType type) const override;

	bool canDeleteNode(const GraphNode& node) const override;

	void drawNodeContent(GraphNode* node) override;
	void drawDetailsContent(GraphNode* node) override;
	void buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
		const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) override;
	void drawSuspendedContent(NodeGraph& graph) override;

private:
	// Parameter widgets for a node, shared by the node body and the details panel.
	// Returns false when the node kind has no parameters.
	bool drawParams(GraphNode* node, bool canvasSpace);

	// The NoiseSettings form, reused verbatim by a Noise Displace node.
	void drawNoiseSettings(NoiseSettings& s, bool canvasSpace);

	// canvasSpace tells the dropdown where its button was laid out: node content is in
	// canvas-local coordinates, the details panel is already in screen space.
	void enumDropdown(const char* label, const char* const* names, int count,
		int current, std::function<void(int)> onPick, bool canvasSpace);

	// A button naming the bound noise-graph asset, deferring its list popup the same
	// way enumDropdown does. onPick receives the chosen asset-root-relative path, or
	// an empty string for "None".
	void assetPicker(const char* label, const std::string& currentPath, bool canvasSpace,
		std::function<void(const std::string&)> onPick);

	// A dropdown cannot open from inside a node (imgui-node-editor draws nodes in a
	// transformed local space, popups are screen-space). The button latches a request;
	// drawSuspendedContent builds the list outside the canvas transform. Same pattern
	// as NoiseEditorSchema.
	struct PendingDropdown {
		bool                     requestOpen = false;
		ImVec2                   anchor      = ImVec2(0.0f, 0.0f);
		const char* const*       names       = nullptr;
		int                      count       = 0;
		int                      current     = 0;
		std::function<void(int)> onPick;
	} m_dropdown;

	// Same deferral for the asset list; the items are snapshotted on click so the
	// deferred frame never touches ResourceManager.
	struct PendingAssetPicker {
		bool                              requestOpen = false;
		bool                              open        = false;
		ImVec2                            anchor      = ImVec2(0.0f, 0.0f);
		std::vector<std::string>          items;
		std::function<void(const std::string&)> onPick;
	} m_assetPicker;
};

}
