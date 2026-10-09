#pragma once

#include "Graph/NodeGraph.h"
#include "Graph/NodeGraphEditorSchema.h"
#include "Renderer/Texture.h"

#include <imgui_node_editor.h>

#include <string>
#include <stdint.h>

namespace ed = ax::NodeEditor;

namespace ve {

// Generic node-graph canvas. Draws any NodeGraph into the current ImGui window;
// per-application behavior comes from a NodeGraphEditorSchema. Each instance owns
// its own ed::EditorContext, so multiple panels can coexist.
class NodeGraphPanel {
public:
	explicit NodeGraphPanel(const std::string& name = "NodeCanvas");
	~NodeGraphPanel();

	// Renders the canvas into the current ImGui window/child. The caller owns the
	// surrounding window (title bar, toolbars, side panels).
	void draw(NodeGraph& graph, NodeGraphEditorSchema& schema);

	// Id of the selected node, or 0 when none (ids start at 1). Latched between mouse
	// gestures: the library recomputes the selection every frame while a marquee box or
	// a node drag is in progress, so anything drawn from it would flip mid-gesture.
	uint32_t selectedNodeId() const { return m_selectedNodeId; }

private:
	// imgui-node-editor uses opaque uintptr_t IDs; Vosta packs {nodeId, pinIndex}
	// into a PinId, so nodeId goes in the high 32 bits and pinIndex in the low
	// 32 bits (lossless on 64-bit builds).
	static ed::PinId  toEdPinId(const PinId& pid);
	static PinId      fromEdPinId(ed::PinId epid);
	static ed::NodeId toEdNodeId(uint32_t id);
	static uint32_t   fromEdNodeId(ed::NodeId eid);
	static ed::LinkId toEdLinkId(uint32_t id);
	static uint32_t   fromEdLinkId(ed::LinkId eid);

	// Draws the pin dot; returns its centre in canvas-local space so the pin
	// pivot (where links attach) lands exactly on the dot. `connected` swaps in
	// the wired sprite.
	ImVec2 drawPinIcon(ImColor color, bool connected);
	void   drawNodePin(const GraphPin& pin, bool isInput, const ImVec2& nodeMin,
	                   const ImVec2& nodeSize, ImColor color, bool connected);
	void   drawNode(Ref<GraphNode> node, const NodeGraph& graph, NodeGraphEditorSchema& schema);

	void handleLinkCreation(NodeGraph& graph, NodeGraphEditorSchema& schema);
	void handleDeletion(NodeGraph& graph, NodeGraphEditorSchema& schema);

	ed::EditorContext* m_context = nullptr;

	// Pin sprites, tinted per pin type by the schema. The connected one is shown
	// once the pin has at least one link.
	Ref<Texture2D> m_pinIcon;
	Ref<Texture2D> m_pinConnectedIcon;

	// Per-instance canvas label so multiple panels do not collide in ImGui.
	std::string m_canvasLabel;
	uint32_t    m_instanceId = 0;

	uint32_t    m_selectedNodeId = 0;
};

}
