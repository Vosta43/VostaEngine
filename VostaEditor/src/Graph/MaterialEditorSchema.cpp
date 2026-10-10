#include "MaterialEditorSchema.h"

#include "Core/ResourceManager.h"
#include "Panels/AssetBrowserWidget.h"

#include <string>

namespace ve {

// Trims text to fit maxWidth, keeping the tail since the file name is the part
// worth reading in a path.
static std::string ellipsizeFront(const std::string& text, float maxWidth) {
	if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
		return text;

	const float ellipsisW = ImGui::CalcTextSize("...").x;

	size_t start = text.size();
	while (start > 0) {
		size_t prev = start - 1;
		while (prev > 0 && (static_cast<unsigned char>(text[prev]) & 0xC0) == 0x80)
			--prev;
		if (ImGui::CalcTextSize(text.c_str() + prev).x + ellipsisW > maxWidth)
			break;
		start = prev;
	}
	return "..." + text.substr(start);
}

ImColor MaterialEditorSchema::getPinColor(PinType type) const {
	switch (type) {
		case PinType::Float:     return ImColor(200, 60, 60);   // red
		case PinType::Float2:    return ImColor(60, 200, 60);   // green
		case PinType::Float3:    return ImColor(60, 60, 200);   // blue
		case PinType::Float4:    return ImColor(200, 200, 60);  // yellow
		case PinType::Sampler2D: return ImColor(220, 220, 220); // white
	}
	return ImColor(128, 128, 128);
}

ImColor MaterialEditorSchema::getNodeColor(NodeType type) const {
	switch (type) {
	case NodeType::Input:   return ImColor(60, 60, 200, 200);
	case NodeType::Math:    return ImColor(60, 200, 60, 200);
	case NodeType::Vector:  return ImColor(200, 60, 60, 200);
	case NodeType::Texture: return ImColor(160, 60, 200, 200);
	case NodeType::Output:  return ImColor(128, 128, 128, 200);
	case NodeType::Utility: return ImColor(200, 200, 60, 200);
	}
	return ImColor(128, 128, 128);
}

bool MaterialEditorSchema::canDeleteNode(const GraphNode& node) const {
	// Every material graph needs its MaterialOutput node.
	return dynamic_cast<const MaterialOutputNode*>(&node) == nullptr;
}

void MaterialEditorSchema::onNodeDeleted(uint32_t nodeId) {
	if (m_pendingTextureNode && m_pendingTextureNode->id == nodeId)
		m_pendingTextureNode = nullptr;
}

void MaterialEditorSchema::drawNodeContent(GraphNode* node) {
	if (auto* cf = dynamic_cast<ConstantFloatNode*>(node)) {
		ImGui::SetNextItemWidth(80);
		ImGui::DragFloat("##val", &cf->value, 0.01f);
	} else if (auto* c2 = dynamic_cast<Constant2VectorNode*>(node)) {
		ImGui::SetNextItemWidth(160);
		ImGui::DragFloat2("##val", &c2->value.x, 0.01f);
	} else if (auto* c3 = dynamic_cast<Constant3VectorNode*>(node)) {
		ImGui::SetNextItemWidth(240);
		ImGui::DragFloat3("##val", &c3->value.x, 0.01f);
	} else if (auto* c4 = dynamic_cast<Constant4VectorNode*>(node)) {
		ImGui::SetNextItemWidth(300);
		ImGui::DragFloat4("##val", &c4->value.x, 0.01f);
	} else if (auto* tc = dynamic_cast<TextureCoordinateNode*>(node)) {
		ImGui::SetNextItemWidth(160);
		ImGui::DragFloat2("##uvScale", &tc->uvScale.x, 0.1f);
	} else if (auto* pan = dynamic_cast<PannerNode*>(node)) {
		ImGui::SetNextItemWidth(160);
		ImGui::DragFloat2("##speed", &pan->speed.x, 0.01f);
	} else if (auto* texNode = dynamic_cast<TextureSamplerNode*>(node)) {
		const float previewSize = 130.0f;   // 1.3x the previous 100px

		std::string label = texNode->textureHandle.isValid()
			? ResourceManager::getPathByHandle<Texture2D>(texNode->textureHandle)
			: "Select Texture...";
		label = ellipsizeFront(label, previewSize);

		if (ImGui::SmallButton(label.c_str())) {
			m_pendingTextureNode = texNode;
			m_texPickerPos = ImGui::GetItemRectMin();
		}
		// Show a thumbnail of the selected texture, or an "Empty" placeholder.
		if (texNode->textureHandle.isValid()) {
			auto tex = ResourceManager::get<Texture2D>(texNode->textureHandle);
			if (tex && tex->getRendererID() != 0) {
				ImTextureID tid = (ImTextureID)(uintptr_t)tex->getRendererID();
				ImGui::Image(tid, ImVec2(previewSize, previewSize), ImVec2(0, 1), ImVec2(1, 0));
			} else {
				ImGui::BeginDisabled();
				ImGui::Button("Empty", ImVec2(previewSize, previewSize));
				ImGui::EndDisabled();
			}
		} else {
			ImGui::BeginDisabled();
			ImGui::Button("Empty", ImVec2(previewSize, previewSize));
			ImGui::EndDisabled();
		}
	}
}

void MaterialEditorSchema::buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
	const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) {
	if (ImGui::BeginMenu("Input")) {
		if (ImGui::MenuItem("Texture Coordinate"))
			addNode(CreateRef<TextureCoordinateNode>(), canvasPos);
		if (ImGui::MenuItem("Time"))
			addNode(CreateRef<TimeNode>(), canvasPos);
		if (ImGui::MenuItem("Panner"))
			addNode(CreateRef<PannerNode>(), canvasPos);
		if (ImGui::MenuItem("World Position"))
			addNode(CreateRef<WorldPositionNode>(), canvasPos);
		if (ImGui::MenuItem("Vertex Normal"))
			addNode(CreateRef<VertexNormalNode>(), canvasPos);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Math")) {
		if (ImGui::MenuItem("Add"))
			addNode(CreateRef<AddNode>(), canvasPos);
		if (ImGui::MenuItem("Subtract"))
			addNode(CreateRef<SubtractNode>(), canvasPos);
		if (ImGui::MenuItem("Multiply"))
			addNode(CreateRef<MultiplyNode>(), canvasPos);
		if (ImGui::MenuItem("Divide"))
			addNode(CreateRef<DivideNode>(), canvasPos);
		if (ImGui::MenuItem("Power"))
			addNode(CreateRef<PowerNode>(), canvasPos);
		if (ImGui::MenuItem("Lerp"))
			addNode(CreateRef<LerpNode>(), canvasPos);
		if (ImGui::MenuItem("Frac"))
			addNode(CreateRef<FracNode>(), canvasPos);
		if (ImGui::MenuItem("One Minus"))
			addNode(CreateRef<OneMinusNode>(), canvasPos);
		if (ImGui::MenuItem("Sin"))
			addNode(CreateRef<SinNode>(), canvasPos);
		if (ImGui::MenuItem("Cos"))
			addNode(CreateRef<CosNode>(), canvasPos);
		if (ImGui::MenuItem("Floor"))
			addNode(CreateRef<FloorNode>(), canvasPos);
		if (ImGui::MenuItem("Step"))
			addNode(CreateRef<StepNode>(), canvasPos);
		if (ImGui::MenuItem("Smoothstep"))
			addNode(CreateRef<SmoothstepNode>(), canvasPos);
		if (ImGui::MenuItem("Append"))
			addNode(CreateRef<AppendNode>(), canvasPos);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Vector")) {
		if (ImGui::MenuItem("Float"))
			addNode(CreateRef<ConstantFloatNode>(), canvasPos);
		if (ImGui::MenuItem("Vector2"))
			addNode(CreateRef<Constant2VectorNode>(), canvasPos);
		if (ImGui::MenuItem("Vector3"))
			addNode(CreateRef<Constant3VectorNode>(), canvasPos);
		if (ImGui::MenuItem("Vector4"))
			addNode(CreateRef<Constant4VectorNode>(), canvasPos);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Texture")) {
		if (ImGui::MenuItem("Texture Sampler"))
			addNode(CreateRef<TextureSamplerNode>(), canvasPos);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Utility")) {
		if (ImGui::MenuItem("Clamp"))
			addNode(CreateRef<ClampNode>(), canvasPos);
		if (ImGui::MenuItem("Saturate"))
			addNode(CreateRef<SaturateNode>(), canvasPos);
		if (ImGui::MenuItem("Component Mask"))
			addNode(CreateRef<ComponentMaskNode>(), canvasPos);
		ImGui::EndMenu();
	}
}

void MaterialEditorSchema::drawSuspendedContent(NodeGraph& graph) {
	(void)graph;

	if (m_pendingTextureNode) {
		ImGui::SetCursorScreenPos(m_texPickerPos);
		ImGui::OpenPopup("TexPickerPopup");
	}
	if (ImGui::BeginPopup("TexPickerPopup")) {
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			m_pendingTextureNode = nullptr;
		} else {
			ImGui::Text("Select Texture");
			ImGui::Separator();
			ImGui::InputText("Filter", m_texFilter, sizeof(m_texFilter));

			auto filtered = AssetBrowser::FilterResources<Texture2D>(m_texFilter);
			for (auto& [path, handle] : filtered) {
				ImGui::PushID(static_cast<int>(handle.index()));
				if (ImGui::Selectable(path.c_str())) {
					if (m_pendingTextureNode) {
						m_pendingTextureNode->textureHandle = handle;
					}
					m_pendingTextureNode = nullptr;
					m_texFilter[0] = '\0';
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopID();
			}
		}
		ImGui::EndPopup();
	}
}

}
