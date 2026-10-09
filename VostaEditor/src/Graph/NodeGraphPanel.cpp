#include "NodeGraphPanel.h"

#include <imgui_internal.h>
#include <atomic>

namespace ve {

// Pin sprite footprint, shared by the icon draw and the right-alignment math.
static constexpr float kPinIconSize = 16.8f;   // 1.2x the previous 14px

// Allocates a distinct canvas ID per panel instance so several node editors can
// coexist without colliding in ImGui's ID space.
static std::atomic<uint32_t> s_nextInstance{ 0 };

// ============================================================================
// ID Encoding
// ============================================================================

ed::PinId NodeGraphPanel::toEdPinId(const PinId& pid) {
	uint64_t packed = (static_cast<uint64_t>(pid.id) << 32) | static_cast<uint64_t>(pid.pinIndex);
	return ed::PinId(static_cast<uintptr_t>(packed));
}

PinId NodeGraphPanel::fromEdPinId(ed::PinId epid) {
	uint64_t v = static_cast<uint64_t>(epid.Get());
	return { static_cast<uint32_t>(v >> 32), static_cast<uint32_t>(v & 0xFFFFFFFF) };
}

ed::NodeId NodeGraphPanel::toEdNodeId(uint32_t id) {
	return ed::NodeId(static_cast<uintptr_t>(id));
}

uint32_t NodeGraphPanel::fromEdNodeId(ed::NodeId eid) {
	return static_cast<uint32_t>(eid.Get());
}

ed::LinkId NodeGraphPanel::toEdLinkId(uint32_t id) {
	return ed::LinkId(static_cast<uintptr_t>(id));
}

uint32_t NodeGraphPanel::fromEdLinkId(ed::LinkId eid) {
	return static_cast<uint32_t>(eid.Get());
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

NodeGraphPanel::NodeGraphPanel(const std::string& name) {
	m_instanceId = s_nextInstance++;
	m_canvasLabel = "##" + name + "_" + std::to_string(m_instanceId);

	m_pinIcon = Texture2D::create("VostaEngine/resources/icons/pin.png");
	m_pinConnectedIcon = Texture2D::create("VostaEngine/resources/icons/pin_connected.png");

	ed::Config config;
	config.SettingsFile = nullptr; // No persistence for now

	// A wheel notch steps one entry of a zoom-level table, so the table's spacing
	// is the zoom speed. imgui-node-editor's default levels step 25% around 1.0,
	// which snaps too hard; squash them 60% of the way toward 1.0 so a notch is a
	// ~10% change and the same table still spans the full range.
	static const float kBaseZoomLevels[] = {
		0.10f, 0.15f, 0.20f, 0.25f, 0.33f, 0.50f, 0.75f, 1.00f,
		1.25f, 1.50f, 2.00f, 2.50f, 3.00f, 4.00f, 5.00f, 6.00f, 7.00f, 8.00f,
	};
	for (float level : kBaseZoomLevels)
		config.CustomZoomLevels.push_back(1.0f + (level - 1.0f) * 0.6f);

	m_context = ed::CreateEditor(&config);
	ed::SetCurrentEditor(m_context);
	ed::EnableShortcuts(true);
	ed::SetCurrentEditor(nullptr);
}

NodeGraphPanel::~NodeGraphPanel() {
	if (m_context) {
		ed::DestroyEditor(m_context);
		m_context = nullptr;
	}
}

// ============================================================================
// Drawing
// ============================================================================

// A pin is wired when any link touches it (either end).
static bool isPinConnected(const NodeGraph& graph, const PinId& pin) {
	for (const auto& link : graph.links) {
		if (link.startPin == pin || link.endPin == pin)
			return true;
	}
	return false;
}

ImVec2 NodeGraphPanel::drawPinIcon(ImColor color, bool connected) {
	const float size = kPinIconSize;
	ImVec2  cursor = ImGui::GetCursorScreenPos();
	ImVec2  center(cursor.x + size * 0.5f, cursor.y + size * 0.5f);

	const Ref<Texture2D>& icon = (connected && m_pinConnectedIcon) ? m_pinConnectedIcon : m_pinIcon;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	if (icon && icon->getRendererID()) {
		dl->AddImage((ImTextureID)(uintptr_t)icon->getRendererID(),
			ImVec2(cursor.x, cursor.y),
			ImVec2(cursor.x + size, cursor.y + size),
			ImVec2(0, 1), ImVec2(1, 0), color);
	} else {
		dl->AddCircleFilled(center, size * 0.36f, color, 12);
	}

	ImGui::Dummy(ImVec2(size, size));
	return center;
}

void NodeGraphPanel::drawNodePin(const GraphPin& pin, bool isInput, const ImVec2& nodeMin, const ImVec2& nodeSize, ImColor color, bool connected) {
	ed::PinKind kind = isInput ? ed::PinKind::Input : ed::PinKind::Output;

	ed::PushStyleColor(ed::StyleColor_PinRect, color.Value);
	ed::PushStyleColor(ed::StyleColor_PinRectBorder, color.Value);

	ed::BeginPin(toEdPinId(pin.id), kind);

	// Set when an output pin is placed flush right: the layout cursor would
	// otherwise grow to that x and widen the node, so it is clamped back after
	// EndPin to the row's natural width.
	bool  clamp = false;
	float clampMaxX = 0.0f;

	// Anchor the pivot (link endpoint) on the dot rather than the default
	// centre of the whole pin row, which would let links leave from mid-node.
	if (isInput) {
		ImVec2 center = drawPinIcon(color, connected);
		ed::PinPivotRect(center, center);
		ImGui::SameLine();
		ImGui::Text("%s", pin.displayName.c_str());
	} else if (nodeSize.x > 0.0f) {
		auto& ns = ed::GetStyle();
		const float icon = kPinIconSize;
		const float gap = ImGui::GetStyle().ItemSpacing.x;
		float labelW = ImGui::CalcTextSize(pin.displayName.c_str()).x;

		float contentLeft = nodeMin.x + ns.NodePadding.x;
		float contentRight = nodeMin.x + nodeSize.x - ns.NodePadding.z;
		float y = ImGui::GetCursorScreenPos().y;
		float prevMaxX = ImGui::GetCurrentWindow()->DC.CursorMaxPos.x;

		float labelX = ImMax(contentLeft, contentRight - icon - gap - labelW);

		ImGui::SetCursorScreenPos(ImVec2(labelX, y));
		ImGui::Text("%s", pin.displayName.c_str());
		ImGui::SameLine();
		ImGui::SetCursorScreenPos(ImVec2(contentRight - icon, y));
		ImVec2 center = drawPinIcon(color, connected);
		ed::PinPivotRect(center, center);

		clamp = true;
		clampMaxX = ImMax(prevMaxX, contentLeft + labelW + gap + icon);
	} else {
		// First frame: the size is not known yet, lay the row out naturally.
		ImGui::Dummy(ImVec2(90, 0));
		ImGui::SameLine();
		ImGui::Text("%s", pin.displayName.c_str());
		ImGui::SameLine();
		ImVec2 center = drawPinIcon(color, connected);
		ed::PinPivotRect(center, center);
	}

	ed::EndPin();

	if (clamp)
		ImGui::GetCurrentWindow()->DC.CursorMaxPos.x = clampMaxX;

	ed::PopStyleColor(2);
}

void NodeGraphPanel::drawNode(Ref<GraphNode> node, const NodeGraph& graph, NodeGraphEditorSchema& schema) {
	ed::NodeId nid = toEdNodeId(node->id);

	// Node rect in canvas-local space. The size is only settled at EndNode, so
	// this is last frame's size; it is used to right-align output pins.
	ImVec2 nMin = ed::GetNodePosition(nid);
	ImVec2 nSize = ed::GetNodeSize(nid);

	// UE5-style two-tone node: deep black-grey body (drawn by the library as
	// NodeBg) with a coloured header painted on top of it below.
	ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.09f, 0.09f, 0.10f, 1.0f));
	ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(0, 0, 0, 255).Value);
	ed::PushStyleColor(ed::StyleColor_HovNodeBorder, ImColor(180, 180, 60, 255).Value);
	ed::PushStyleColor(ed::StyleColor_SelNodeBorder, ImColor(255, 200, 40, 255).Value);

	ed::BeginNode(nid);

	// Title bar. No ImGui::Separator here: a horizontal separator spans
	// window->WorkRect.Max.x, which inside a node is the whole canvas, so its rule
	// would streak to the canvas edge. The header rule is drawn into the node's
	// background channel below instead, where the node's real width is known.
	ImGui::TextColored(ImVec4(0.08f, 0.08f, 0.08f, 1.0f), "%s", node->m_displayName.c_str());
	ImGui::Spacing();

	// Pins are stacked vertically.  ed::PinKind::Input / Output controls which
	// side of the node they attach to — no SameLine / column layout needed.
	for (const auto& pin : node->m_inputPins) {
		drawNodePin(pin, true, nMin, nSize, schema.getPinColor(pin.type),
		            isPinConnected(graph, pin.id));
	}
	for (const auto& pin : node->m_outputPins) {
		drawNodePin(pin, false, nMin, nSize, schema.getPinColor(pin.type),
		            isPinConnected(graph, pin.id));
	}

	// Inline widgets are app-specific. PushID(node->id) makes the ImGui widget
	// IDs unique across nodes since ed::BeginNode does not push an ImGui ID.
	ImGui::PushID(static_cast<int>(node->id));
	schema.drawNodeContent(node.get());
	ImGui::PopID();

	ed::EndNode();

	// Header: primary colour fading to light grey, left to right. Painted into
	// the node's user-background channel so it sits above the stock NodeBg but
	// below pins/content. Corner arcs are filled with the gradient end colours
	// so the header keeps the node's rounded top corners.
	// Must run after EndNode: during the build the node builder swaps in its own
	// single-channel splitter, on which the channel index does not exist.
	if (ImDrawList* bg = ed::GetNodeBackgroundDrawList(nid)) {
		ImVec2 hMin = ed::GetNodePosition(nid);   // canvas-local space
		ImVec2 hSize = ed::GetNodeSize(nid);       // settled for this frame by EndNode
		ImVec2 nMax(hMin.x + hSize.x, hMin.y + hSize.y);

		auto& ns = ed::GetStyle();
		float headerH = ns.NodePadding.y + ImGui::GetTextLineHeight() + 7.0f;
		float r = ImMin(ns.NodeRounding, ImMin(hSize.x, headerH) * 0.5f);

		ImVec4 base = schema.getNodeColor(node->m_nodeType).Value;
		ImU32 left = ImColor(base.x, base.y, base.z, 1.0f);
		ImU32 right = IM_COL32(190, 190, 190, 255);

		float hb = hMin.y + headerH;
		bg->AddRectFilledMultiColor(ImVec2(hMin.x, hMin.y + r), ImVec2(nMax.x, hb), left, right, right, left);
		bg->AddRectFilledMultiColor(ImVec2(hMin.x + r, hMin.y), ImVec2(nMax.x - r, hMin.y + r), left, right, right, left);
		bg->AddRectFilled(hMin, ImVec2(hMin.x + r, hMin.y + r), left, r, ImDrawFlags_RoundCornersTopLeft);
		bg->AddRectFilled(ImVec2(nMax.x - r, hMin.y), ImVec2(nMax.x, hMin.y + r), right, r, ImDrawFlags_RoundCornersTopRight);
		bg->AddLine(ImVec2(hMin.x, hb), ImVec2(nMax.x, hb), IM_COL32(0, 0, 0, 130));

		// Thin black outline around the whole node. Drawn here (above the stock
		// border) so it also runs along the top edge, which the header covers.
		bg->AddRect(hMin, nMax, IM_COL32(0, 0, 0, 255), r, ImDrawFlags_RoundCornersAll, 1.0f);
	}

	ed::PopStyleColor(4);
}

// ============================================================================
// Link creation
// ============================================================================

void NodeGraphPanel::handleLinkCreation(NodeGraph& graph, NodeGraphEditorSchema& schema) {
	if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f)) {
		ed::PinId startPinId, endPinId;
		if (ed::QueryNewLink(&startPinId, &endPinId)) {
			if (startPinId && endPinId) {
				PinId veStart = fromEdPinId(startPinId);
				PinId veEnd = fromEdPinId(endPinId);

				GraphNode* startNode = graph.findNode(veStart.id);
				GraphNode* endNode = graph.findNode(veEnd.id);

				if (startNode && endNode) {
					bool startIsOutput = (veStart.pinIndex >= startNode->m_inputPins.size() && veStart.pinIndex < startNode->m_inputPins.size() + startNode->m_outputPins.size());
					bool endIsInput = (veEnd.pinIndex < endNode->m_inputPins.size());

					if (startIsOutput && endIsInput && schema.canConnect(*startNode, veStart, *endNode, veEnd)) {
						if (ed::AcceptNewItem()) {
							// Remove existing link to the same input only after the user confirms
							graph.removeLinksTo(veEnd);
							graph.addLink(
								{ veStart.id, veStart.pinIndex },
								{ veEnd.id,   veEnd.pinIndex }
							);
						}
					} else {
						ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
					}
				}
			}
		}
		ed::EndCreate();
	}
}

// ============================================================================
// Deletion
// ============================================================================

void NodeGraphPanel::handleDeletion(NodeGraph& graph, NodeGraphEditorSchema& schema) {
	if (ed::BeginDelete()) {
		ed::LinkId deletedLinkId;
		while (ed::QueryDeletedLink(&deletedLinkId)) {
			if (ed::AcceptDeletedItem()) {
				graph.removeLink(fromEdLinkId(deletedLinkId));
			}
		}

		ed::NodeId deletedNodeId;
		while (ed::QueryDeletedNode(&deletedNodeId)) {
			uint32_t nid = fromEdNodeId(deletedNodeId);
			GraphNode* node = graph.findNode(nid);
			if (!node || !schema.canDeleteNode(*node)) {
				ed::RejectDeletedItem();
			} else if (ed::AcceptDeletedItem()) {
				schema.onNodeDeleted(nid);
				graph.removeNode(nid);
			}
		}
		ed::EndDelete();
	}
}

// ============================================================================
// Main draw
// ============================================================================

void NodeGraphPanel::draw(NodeGraph& graph, NodeGraphEditorSchema& schema) {
	ed::SetCurrentEditor(m_context);
	ImGui::PushID(static_cast<int>(m_instanceId));

	ed::Begin(m_canvasLabel.c_str(), ImVec2(0, 0));

	for (auto& node : graph.nodes) {
		ed::SetNodePosition(toEdNodeId(node->id), ImVec2(node->m_pos.x, node->m_pos.y));
		drawNode(node, graph, schema);
	}

	// Draw existing links
	for (const auto& link : graph.links) {
		ed::Link(
			toEdLinkId(link.id),
			toEdPinId(link.startPin),
			toEdPinId(link.endPin),
			ImVec4(1, 1, 1, 1),
			2.0f
		);
	}

	handleLinkCreation(graph, schema);
	handleDeletion(graph, schema);

	// --- Create-node popup (inside suspended zone so it receives input) ---
	ed::Suspend();

	auto addNode = [&](Ref<GraphNode> node, const glm::vec2& pos) {
		graph.addNode(node, pos);
		// Register the node with the editor right away. The end-of-frame readback
		// asks for the position of every node in the graph, and for one the editor
		// has never seen GetNodePosition returns FLT_MAX — which would overwrite the
		// position we just gave it and park the node out of view.
		ed::SetNodePosition(toEdNodeId(node->id), ImVec2(pos.x, pos.y));
	};

	// Capture the placement from the frame the menu opens: by the time an entry is
	// clicked the mouse is over the popup, not the canvas, so reading it then would
	// drop the node wherever the menu happens to be.
	glm::vec2 createPos(0.0f);
	if (ed::ShowBackgroundContextMenu()) {
		ImVec2 p = ed::ScreenToCanvas(ImGui::GetMousePos());
		createPos = glm::vec2(p.x, p.y);
		ImGui::OpenPopup("CreateNodePopup");
	}

	if (ImGui::BeginPopup("CreateNodePopup")) {
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Create Node");
		ImGui::Separator();

		schema.buildCreateMenu(graph, createPos, addNode);

		ImGui::EndPopup();
	}

	// App-specific popups that need the suspended zone (e.g. texture picker).
	schema.drawSuspendedContent(graph);

	ed::Resume();

	ed::End();

	// Latch the selection only between gestures. A marquee box or a node drag is
	// recomputed every frame, so reading the selection then would make anything drawn
	// from it (a details panel) flip state while the mouse is held.
	if (!ImGui::IsAnyMouseDown()) {
		ed::NodeId selected[1];
		m_selectedNodeId = (ed::GetSelectedNodes(selected, 1) > 0)
			? fromEdNodeId(selected[0]) : 0;
	}

	for (auto& node : graph.nodes) {
		ImVec2 pos = ed::GetNodePosition(toEdNodeId(node->id));
		// FLT_MAX is the editor's "unknown node" answer; keep the position we hold.
		if (pos.x != FLT_MAX && pos.y != FLT_MAX)
			node->m_pos = glm::vec2(pos.x, pos.y);
	}

	ed::SetCurrentEditor(nullptr);
	ImGui::PopID();
}

}
