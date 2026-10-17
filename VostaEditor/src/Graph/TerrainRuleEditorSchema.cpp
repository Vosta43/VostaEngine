#include "TerrainRuleEditorSchema.h"

#include "Asset/AssetLibrary.h"
#include "Terrain/RuleNodes.h"
#include "Renderer/MaterialLayer.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <filesystem>

namespace ve {

namespace {
	constexpr float kItemWidth = 180.0f;
}

ImColor TerrainRuleEditorSchema::getPinColor(PinType type) const {
	(void)type;   // rule graphs carry scalars only
	return ImColor(190, 190, 190);
}

ImColor TerrainRuleEditorSchema::getNodeColor(NodeType type) const {
	switch (type) {
		case NodeType::Input:  return ImColor(70, 140, 70, 200);
		case NodeType::Output: return ImColor(128, 128, 128, 200);
		default:               return ImColor(128, 128, 128, 200);
	}
}

bool TerrainRuleEditorSchema::canDeleteNode(const GraphNode& node) const {
	// Any band can go; the panel warns when the graph is left with no band at all.
	(void)node;
	return true;
}

void TerrainRuleEditorSchema::enumDropdown(const char* label, const char* const* names, int count,
	int current, std::function<void(int)> onPick, bool canvasSpace)
{
	ImGui::PushID(label);

	if (ImGui::Button(names[current], ImVec2(kItemWidth, 0.0f))) {
		const ImVec2 rect = ImGui::GetItemRectMin();
		m_dropdown.requestOpen = true;
		m_dropdown.anchor      = canvasSpace ? ax::NodeEditor::CanvasToScreen(rect) : rect;
		m_dropdown.names       = names;
		m_dropdown.count       = count;
		m_dropdown.current     = current;
		m_dropdown.onPick      = onPick;
	}

	// Down caret, so the row reads as a dropdown rather than a plain button.
	{
		const ImVec2 mn   = ImGui::GetItemRectMin();
		const ImVec2 mx   = ImGui::GetItemRectMax();
		const float  half = ImGui::GetFrameHeight() * 0.16f;
		const float  cx   = mx.x - ImGui::GetStyle().FramePadding.x - half;
		const float  cy   = (mn.y + mx.y) * 0.5f;
		ImGui::GetWindowDrawList()->AddTriangleFilled(
			ImVec2(cx - half, cy - half * 0.6f),
			ImVec2(cx + half, cy - half * 0.6f),
			ImVec2(cx, cy + half * 0.8f),
			ImGui::GetColorU32(ImGuiCol_Text));
	}

	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Text("%s", label);
}

void TerrainRuleEditorSchema::assetPicker(const char* label, const std::string& currentPath,
	bool canvasSpace, std::function<void(const std::string&)> onPick)
{
	ImGui::PushID(label);

	const std::string shown = currentPath.empty()
		? std::string("(none)")
		: std::filesystem::path(currentPath).stem().string();

	if (ImGui::Button(shown.c_str(), ImVec2(kItemWidth, 0.0f))) {
		const ImVec2 rect = ImGui::GetItemRectMin();
		m_assetPicker.requestOpen = true;
		m_assetPicker.anchor      = canvasSpace ? ax::NodeEditor::CanvasToScreen(rect) : rect;
		m_assetPicker.onPick      = std::move(onPick);

		// Snapshot the project's noise graphs now: the deferred popup draws a frame
		// later, outside the canvas, and must not walk the library again then.
		m_assetPicker.items.clear();
		AssetLibrary::get().refresh();
		for (const AssetEntry& entry : AssetLibrary::get().list())
			if (entry.typeKey == "noise")
				m_assetPicker.items.push_back(entry.path);
	}

	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Text("%s", label);
}

bool TerrainRuleEditorSchema::drawParams(GraphNode* node, bool canvasSpace) {
	if (auto* src = dynamic_cast<FieldSourceNode*>(node)) {
		static const char* kModeNames[] = { "Noise Asset", "Height", "Slope", "Curvature" };
		ImGui::PushItemWidth(kItemWidth);
		enumDropdown("Source", kModeNames, IM_ARRAYSIZE(kModeNames),
			static_cast<int>(src->mode),
			[src](int i) { src->mode = static_cast<FieldSourceNode::Mode>(i); }, canvasSpace);
		if (src->mode == FieldSourceNode::Mode::NoiseAsset)
			assetPicker("Field", src->noisePath, canvasSpace,
				[src](const std::string& p) { src->noisePath = p; });
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* bands = dynamic_cast<LayerBandsNode*>(node)) {
		// One row per slice of the source's range: [Min..Max] with a soft edge on each
		// side, routed to a layer. Rows are added and removed here, so the count is free.
		ImGui::PushItemWidth(64.0f);
		int removeIndex = -1;
		for (int i = 0; i < (int)bands->segments.size(); ++i) {
			LayerSegment& s = bands->segments[i];
			ImGui::PushID(i);
			ImGui::DragFloat("Min", &s.minValue, 0.005f, 0.0f, 1.0f, "%.2f");
			ImGui::SameLine();
			ImGui::DragFloat("Max", &s.maxValue, 0.005f, 0.0f, 1.0f, "%.2f");

			ImGui::DragFloat("Soft", &s.soft, 0.002f, 0.0f, 1.0f, "%.2f");
			ImGui::SameLine();
			ImGui::DragInt("Layer", &s.layer, 0.1f, 1, kMaxMaterialLayers - 1);
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) removeIndex = i;
			ImGui::PopID();
		}
		if (ImGui::SmallButton("+ Segment"))
			bands->segments.push_back(LayerSegment{});
		ImGui::SameLine();
		if (ImGui::SmallButton("Distribute") && !bands->segments.empty()) {
			// Even slices of the [0,1] field across the rows, in row order; each row
			// keeps its layer and soft edge, only the range is rewritten.
			const float n = (float)bands->segments.size();
			for (int i = 0; i < (int)bands->segments.size(); ++i) {
				bands->segments[i].minValue = (float)i / n;
				bands->segments[i].maxValue = (float)(i + 1) / n;
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Split 0..1 into equal bands, one per row");
		if (removeIndex >= 0 && bands->segments.size() > 1)
			bands->segments.erase(bands->segments.begin() + removeIndex);
		ImGui::PopItemWidth();
		ImGui::TextDisabled("Layer 1..%d", kMaxMaterialLayers - 1);
		return true;
	}

	return false;
}

void TerrainRuleEditorSchema::drawNodeContent(GraphNode* node) {
	drawParams(node, true);
}

void TerrainRuleEditorSchema::drawDetailsContent(GraphNode* node) {
	ImGui::TextUnformatted(node->m_displayName.c_str());
	ImGui::Separator();
	if (!drawParams(node, false))
		ImGui::TextDisabled("No parameters");
}

void TerrainRuleEditorSchema::resetNodeState() {
	m_dropdown = PendingDropdown{};
	m_assetPicker = PendingAssetPicker{};
}

void TerrainRuleEditorSchema::drawSuspendedContent(NodeGraph&) {
	if (m_dropdown.requestOpen) {
		ImGui::SetCursorScreenPos(m_dropdown.anchor);
		ImGui::OpenPopup("##ruleEnum");
		m_dropdown.requestOpen = false;
	}

	if (ImGui::BeginPopup("##ruleEnum")) {
		for (int i = 0; i < m_dropdown.count && m_dropdown.names; ++i) {
			if (ImGui::Selectable(m_dropdown.names[i], i == m_dropdown.current)) {
				if (m_dropdown.onPick)
					m_dropdown.onPick(i);
				m_dropdown.names = nullptr;
				m_dropdown.onPick = nullptr;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	} else if (m_dropdown.names) {
		m_dropdown.names = nullptr;
		m_dropdown.onPick = nullptr;
	}

	if (m_assetPicker.requestOpen) {
		ImGui::SetCursorScreenPos(m_assetPicker.anchor);
		ImGui::OpenPopup("##ruleAsset");
		m_assetPicker.requestOpen = false;
		m_assetPicker.open = true;
	}

	if (ImGui::BeginPopup("##ruleAsset")) {
		if (ImGui::Selectable("None", false)) {
			if (m_assetPicker.onPick) m_assetPicker.onPick(std::string());
			m_assetPicker.onPick = nullptr;
			m_assetPicker.open = false;
			ImGui::CloseCurrentPopup();
		}
		for (const std::string& item : m_assetPicker.items) {
			if (ImGui::Selectable(item.c_str(), false)) {
				if (m_assetPicker.onPick) m_assetPicker.onPick(item);
				m_assetPicker.onPick = nullptr;
				m_assetPicker.open = false;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	} else if (m_assetPicker.open) {
		m_assetPicker.open = false;
		m_assetPicker.onPick = nullptr;
	}
}

void TerrainRuleEditorSchema::buildCreateMenu(NodeGraph&, const glm::vec2& canvasPos,
	const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) {
	if (ImGui::MenuItem("Field Source"))
		addNode(CreateRef<FieldSourceNode>(), canvasPos);
	if (ImGui::MenuItem("Layer Bands"))
		addNode(CreateRef<LayerBandsNode>(), canvasPos);
}

}
