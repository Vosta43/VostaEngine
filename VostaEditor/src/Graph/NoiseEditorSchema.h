#pragma once

#include "Graph/NodeGraphEditorSchema.h"
#include "Renderer/Texture.h"

#include <cstdint>
#include <unordered_map>

namespace ve {

struct NoiseSettings;
class NoiseSplineNode;

// Noise-graph policy for the generic NodeGraphPanel: pin/node colours, the inline
// NoiseSettings form on a Noise Unit node, the Output range editor, the six-type
// create menu, and the rule that the Output terminal can never be deleted.
class NoiseEditorSchema : public NodeGraphEditorSchema {
public:
	ImColor getPinColor(PinType type) const override;
	ImColor getNodeColor(NodeType type) const override;

	bool canDeleteNode(const GraphNode& node) const override;
	void onNodeDeleted(uint32_t nodeId) override;

	void drawNodeContent(GraphNode* node) override;
	void drawDetailsContent(GraphNode* node) override;
	void buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
		const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) override;
	void drawSuspendedContent(NodeGraph& graph) override;

	// Drops the per-node thumbnails and collapse state. Call when the graph is
	// replaced wholesale (a file load), where node ids no longer mean the same node.
	void resetNodeState();

private:
	// canvasSpace tells the dropdown where its button was laid out: node content is in
	// canvas-local coordinates, the details panel is already in screen space.
	void drawNoiseSettings(NoiseSettings& s, bool canvasSpace);

	// Parameter widgets for a node, shared by the node body and the details panel.
	// Returns false when the node kind has no parameters.
	bool drawParams(GraphNode* node, bool canvasSpace);
	void enumDropdown(const char* label, const char* const* names, int count,
		int current, std::function<void(int)> onPick, bool canvasSpace);

	// Thumbnail of one unit's field, re-baked when its settings change.
	void drawUnitPreview(uint32_t nodeId, NoiseSettings& s);

	// Control-point curve editor for a Spline node: drag points, double-click empty
	// space to insert, right-click a point to remove.
	void drawSplineEditor(NoiseSplineNode* sp);

	// Interaction state for a spline widget, keyed by the widget's ImGui id so the
	// copy inside a node and the copy in the details panel drag independently.
	struct SplineDrag {
		int    point = -1;
		ImVec2 dx{ 0.0f, 1.0f };
		ImVec2 dy{ 0.0f, 1.0f };
	};
	std::unordered_map<ImGuiID, SplineDrag> m_splineDrags;

	struct PendingDropdown {
		bool                     requestOpen = false;   // one-frame pulse to open
		ImVec2                   anchor      = ImVec2(0.0f, 0.0f);
		const char* const*       names       = nullptr;
		int                      count       = 0;
		int                      current     = 0;
		std::function<void(int)> onPick;
	} m_dropdown;

	struct NodeThumb {
		Ref<Texture2D> tex;
		uint32_t       hash = 0;
	};

	std::unordered_map<uint32_t, NodeThumb> m_thumbs;
	// Absent means collapsed, which is also the state a freshly created node starts in.
	std::unordered_map<uint32_t, bool>      m_collapsed;
};

}
