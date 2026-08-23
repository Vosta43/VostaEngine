#include "MaterialGraphPanel.h"

#include "Core/Application.h"
#include "Core/Log.h"
#include "Renderer/ThumbnailRenderer.h"
#include "Scene/Archive.h"
#include "Core/AssetConfig.h"

#include <imgui_internal.h>

namespace ve {

// ============================================================================
// ID Encoding
// ============================================================================

ed::PinId MaterialGraphPanel::toEdPinId(const ve::PinId& pid) {
    uint64_t packed = (static_cast<uint64_t>(pid.id) << 32) | static_cast<uint64_t>(pid.pinIndex);
    return ed::PinId(static_cast<uintptr_t>(packed));
}

ve::PinId MaterialGraphPanel::fromEdPinId(ed::PinId epid) {
    uint64_t v = static_cast<uint64_t>(epid.Get());
    return { static_cast<uint32_t>(v >> 32), static_cast<uint32_t>(v & 0xFFFFFFFF) };
}

ed::NodeId MaterialGraphPanel::toEdNodeId(uint32_t id) {
    return ed::NodeId(static_cast<uintptr_t>(id));
}

uint32_t MaterialGraphPanel::fromEdNodeId(ed::NodeId eid) {
    return static_cast<uint32_t>(eid.Get());
}

ed::LinkId MaterialGraphPanel::toEdLinkId(uint32_t id) {
    return ed::LinkId(static_cast<uintptr_t>(id));
}

uint32_t MaterialGraphPanel::fromEdLinkId(ed::LinkId eid) {
    return static_cast<uint32_t>(eid.Get());
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

MaterialGraphPanel::MaterialGraphPanel() {
    ed::Config config;
    config.SettingsFile = nullptr; // No persistence for now
    m_context = ed::CreateEditor(&config);
    ed::SetCurrentEditor(m_context);
    ed::EnableShortcuts(true);
    ed::SetCurrentEditor(nullptr);
}

MaterialGraphPanel::~MaterialGraphPanel() {
    if (m_context) {
        ed::DestroyEditor(m_context);
        m_context = nullptr;
    }
}

// ============================================================================
// Styling helpers
// ============================================================================

ImColor MaterialGraphPanel::getPinColor(PinType type) const {
    switch (type) {
        case PinType::Float:     return ImColor(200, 60,  60);   // red
        case PinType::Float2:    return ImColor(60,  200, 60);   // green
        case PinType::Float3:    return ImColor(60,  60,  200);  // blue
        case PinType::Float4:    return ImColor(200, 200, 60);   // yellow
        case PinType::Sampler2D: return ImColor(220, 220, 220);  // white
    }
    return ImColor(128, 128, 128);
}

ImColor MaterialGraphPanel::getNodeColor(NodeType type) const {
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

void MaterialGraphPanel::drawPinIcon(const MaterialPin& pin, bool connected) {
    (void)connected; // Reserved for future: filled vs hollow icon

    ImColor color = getPinColor(pin.type);
    ImVec2  cursor = ImGui::GetCursorScreenPos();
    float   radius = 5.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(
        ImVec2(cursor.x + radius + 2.0f, cursor.y + radius + 1.0f),
        radius, color, 12);

    ImGui::Dummy(ImVec2(radius * 2 + 4.0f, radius * 2 + 2.0f));
}

// ============================================================================
// Drawing
// ============================================================================

void MaterialGraphPanel::drawNodePin(const MaterialPin& pin, bool isInput) {
    ed::PinKind kind = isInput ? ed::PinKind::Input : ed::PinKind::Output;
    ImColor     color = getPinColor(pin.type);

    ed::PushStyleColor(ed::StyleColor_PinRect,       color.Value);
    ed::PushStyleColor(ed::StyleColor_PinRectBorder, color.Value);

    ed::BeginPin(toEdPinId(pin.id), kind);

    if (isInput) {
        drawPinIcon(pin, false);
        ImGui::SameLine();
        ImGui::Text("%s", pin.displayName.c_str());
    } else {
        ImGui::Dummy(ImVec2(90, 0));
        ImGui::SameLine();
        ImGui::Text("%s", pin.displayName.c_str());
        ImGui::SameLine();
        drawPinIcon(pin, false);
    }

    ed::EndPin();

    ed::PopStyleColor(2);
}

void MaterialGraphPanel::drawNodeContent(Ref<MaterialNode> node) {
    ed::PushStyleColor(ed::StyleColor_NodeBg,       ImColor(getNodeColor(node->m_nodeType)).Value);
    ed::PushStyleColor(ed::StyleColor_NodeBorder,   ImColor(100, 100, 110, 200).Value);
    ed::PushStyleColor(ed::StyleColor_HovNodeBorder,ImColor(180, 180, 60, 255).Value);
    ed::PushStyleColor(ed::StyleColor_SelNodeBorder,ImColor(255, 200, 40, 255).Value);

    ed::BeginNode(toEdNodeId(node->id));

    // Title bar
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "%s", node->m_displayName.c_str());
    ImGui::Separator();

    // Pins are stacked vertically.  ed::PinKind::Input / Output controls which
    // side of the node they attach to — no SameLine / column layout needed.
    for (const auto& pin : node->m_inputPins) {
        drawNodePin(pin, true);
    }
    for (const auto& pin : node->m_outputPins) {
        drawNodePin(pin, false);
    }

    // Inline value editors for constant nodes.
    // Texture nodes use a deferred popup rendered in the ed::Suspend zone
    // to avoid mouse interception and node-bound clipping.
    // PushID(node->id) ensures ImGui widget IDs are unique across nodes since
    // ed::BeginNode does not push an ImGui ID on the stack.
    ImGui::PushID(static_cast<int>(node->id));
    if (auto* cf = dynamic_cast<ConstantFloatNode*>(node.get())) {
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("##val", &cf->value, 0.01f);
    } else if (auto* c2 = dynamic_cast<Constant2VectorNode*>(node.get())) {
        ImGui::SetNextItemWidth(160);
        ImGui::DragFloat2("##val", &c2->value.x, 0.01f);
    } else if (auto* c3 = dynamic_cast<Constant3VectorNode*>(node.get())) {
        ImGui::SetNextItemWidth(240);
        ImGui::DragFloat3("##val", &c3->value.x, 0.01f);
    } else if (auto* c4 = dynamic_cast<Constant4VectorNode*>(node.get())) {
        ImGui::SetNextItemWidth(300);
        ImGui::DragFloat4("##val", &c4->value.x, 0.01f);
    } else if (auto* tc = dynamic_cast<TextureCoordinateNode*>(node.get())) {
        ImGui::SetNextItemWidth(160);
        ImGui::DragFloat2("##uvScale", &tc->uvScale.x, 0.1f);
    } else if (auto* texNode = dynamic_cast<TextureSamplerNode*>(node.get())) {
        std::string label = texNode->textureHandle.isValid()
            ? ResourceManager::getPathByHandle<Texture2D>(texNode->textureHandle)
            : "Select Texture...";
        if (ImGui::SmallButton(label.c_str())) {
            m_pendingTextureNode = texNode;
            m_texPickerPos = ImGui::GetItemRectMin();
        }
        // Show a thumbnail of the selected texture, or an "Empty" placeholder.
        if (texNode->textureHandle.isValid()) {
            auto tex = ResourceManager::get<Texture2D>(texNode->textureHandle);
            if (tex && tex->getRendererID() != 0) {
                ImTextureID tid = (ImTextureID)(uintptr_t)tex->getRendererID();
                ImGui::Image(tid, ImVec2(100, 100), ImVec2(0, 1), ImVec2(1, 0));
            } else {
                ImGui::BeginDisabled();
                ImGui::Button("Empty", ImVec2(100, 100));
                ImGui::EndDisabled();
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("Empty", ImVec2(100, 100));
            ImGui::EndDisabled();
        }
    }
    ImGui::PopID();

    ed::EndNode();
    ed::PopStyleColor(4);
}

// ============================================================================
// Link creation
// ============================================================================

void MaterialGraphPanel::handleLinkCreation(MaterialGraph& graph) {
    if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f)) {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {
            if (startPinId && endPinId) {
                ve::PinId veStart = fromEdPinId(startPinId);
                ve::PinId veEnd   = fromEdPinId(endPinId);

                MaterialNode* startNode = graph.findNode(veStart.id);
                MaterialNode* endNode   = graph.findNode(veEnd.id);

                if (startNode && endNode) {
                    bool startIsOutput = (veStart.pinIndex >= startNode->m_inputPins.size() && veStart.pinIndex < startNode->m_inputPins.size() + startNode->m_outputPins.size());
                    bool endIsInput = (veEnd.pinIndex < endNode->m_inputPins.size());

                    if (startIsOutput && endIsInput) {
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

void MaterialGraphPanel::handleDeletion(MaterialGraph& graph) {
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
            auto*    node = graph.findNode(nid);
            // Prevent deletion of the MaterialOutputNode — every graph needs one
            if (node && dynamic_cast<MaterialOutputNode*>(node)) {
                ed::RejectDeletedItem();
            } else if (ed::AcceptDeletedItem()) {
                // Clear deferred pointers if the pending node is being deleted.
                if (m_pendingTextureNode && m_pendingTextureNode->id == nid)
                    m_pendingTextureNode = nullptr;
                graph.removeNode(nid);
            }
        }
        ed::EndDelete();
    }
}

void MaterialGraphPanel::addNodeToGraph(MaterialGraph& graph, Ref<MaterialNode> node,glm::vec2 pos) {

    graph.addNode(node,pos);
    ed::SetNodePosition(toEdNodeId(node->id), ImVec2(pos.x, pos.y));
}

// ============================================================================
// Main render entry point
// ============================================================================

void MaterialGraphPanel::onGuiRender(Ref<Material> material, bool& needsRecompile,
                                     bool* openFlag, AssetHandle originalMaterialHandle) {
    if (!material) return;

    MaterialGraph& graph = material->graph;
    ed::SetCurrentEditor(m_context);

    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Material Editor", openFlag)) {
        ed::SetCurrentEditor(nullptr);
        ImGui::End();
        return;
    }

    // --- Toolbar ---
    if (ImGui::Button("Compile")) {

        material->compile();

        if (material->isCompiled) {
            needsRecompile = false;
            VE_CORE_SUCCESS_PRINT("Material compiled successfully");

            // Persist to .veasset file.
            TextArchive ar(toAbsolute(material->name), ArchiveMode::write);
            if (ar.isGood()) {
                material->serialize(ar);
            }

            if (originalMaterialHandle.isValid()) {
                ThumbnailRenderer::invalidate(originalMaterialHandle);
            }
        }
        else {
            VE_CORE_ERROR_PRINT("Material compilation failed: %s", material->compileError.c_str());
        }
    }
    ImGui::SameLine();
    if (material->isCompiled) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Compiled OK");
    } else if (!material->compileError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", material->compileError.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Not compiled");
    }

    ImGui::Separator();

    // --- Split layout: left panel (preview + details) | right panel (node editor) ---
    const float leftPanelWidth = 240.0f;
    const float previewHeight   = 200.0f;

    // ===== Left panel =====
    ImGui::BeginChild("##LeftPanel", ImVec2(leftPanelWidth, 0), ImGuiChildFlags_None);

    // Top: material preview sphere
    ImGui::Text("Preview");
    ImGui::Separator();
    if (originalMaterialHandle.isValid()) {
        auto thumbnail = ThumbnailRenderer::getMaterialThumbnail(originalMaterialHandle);
        if (thumbnail && thumbnail->getRendererID() != 0) {
            ImTextureID texId = (ImTextureID)(uintptr_t)thumbnail->getRendererID();
            float availW = ImGui::GetContentRegionAvail().x - 4.0f;
            float size = availW < previewHeight ? availW : previewHeight;
            ImGui::Image(texId, ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));
        }
    } else {
        ImGui::BeginChild("##NoPreview", ImVec2(previewHeight, previewHeight), ImGuiChildFlags_Borders);
        ImGui::EndChild();
    }

    ImGui::Spacing();

    // Bottom: reserved details panel (UE5-style)
    ImGui::Text("Details");
    ImGui::Separator();
    ImGui::BeginChild("##DetailsPanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::TextDisabled("(reserved)");
    ImGui::EndChild();

    ImGui::EndChild(); // ##LeftPanel

    ImGui::SameLine();

    // ===== Right panel: node editor =====
    ed::Begin("MaterialEditorCanvas", ImVec2(0, 0));

    m_idToNode.clear();
    for (auto& node : graph.nodes) {
        m_idToNode[node->id] = node;
        ed::SetNodePosition(toEdNodeId(node->id), ImVec2(node->m_pos.x, node->m_pos.y));
        drawNodeContent(node);
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

    // --- Interactive operations ---
    handleLinkCreation(graph);
    handleDeletion(graph);

    // --- Create-node popup (inside suspended zone so it receives input) ---
    ed::Suspend();

    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("CreateNodePopup");
    }

    if (ImGui::BeginPopup("CreateNodePopup")) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Create Node");
        ImGui::Separator();

        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 canvasPos = ed::ScreenToCanvas(mousePos);
        glm::vec2 nodePos(canvasPos.x, canvasPos.y);

        if (ImGui::BeginMenu("Input")) {
            if (ImGui::MenuItem("Texture Coordinate"))
                addNodeToGraph(graph, CreateRef<TextureCoordinateNode>(), nodePos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Math")) {
            if (ImGui::MenuItem("Add"))
                addNodeToGraph(graph, CreateRef<AddNode>(), nodePos);
            if (ImGui::MenuItem("Subtract"))
                addNodeToGraph(graph, CreateRef<SubtractNode>(), nodePos);
            if (ImGui::MenuItem("Multiply"))
                addNodeToGraph(graph, CreateRef<MultiplyNode>(), nodePos);
            if (ImGui::MenuItem("Divide"))
                addNodeToGraph(graph, CreateRef<DivideNode>(), nodePos);
            if (ImGui::MenuItem("Lerp"))
                addNodeToGraph(graph, CreateRef<LerpNode>(), nodePos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Vector")) {
            if (ImGui::MenuItem("Float"))
                addNodeToGraph(graph, CreateRef<ConstantFloatNode>(), nodePos);
            if (ImGui::MenuItem("Vector2"))
                addNodeToGraph(graph, CreateRef<Constant2VectorNode>(), nodePos);
            if (ImGui::MenuItem("Vector3"))
                addNodeToGraph(graph, CreateRef<Constant3VectorNode>(), nodePos);
            if (ImGui::MenuItem("Vector4"))
                addNodeToGraph(graph, CreateRef<Constant4VectorNode>(), nodePos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Texture")) {
            if (ImGui::MenuItem("Texture Sampler"))
                addNodeToGraph(graph, CreateRef<TextureSamplerNode>(), nodePos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Utility")) {
            if (ImGui::MenuItem("Clamp"))
                addNodeToGraph(graph, CreateRef<ClampNode>(), nodePos);
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    // --- Texture picker popup ---
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
            static char texFilter[128] = "";
            ImGui::InputText("Filter", texFilter, sizeof(texFilter));

            auto filtered = AssetBrowser::FilterResources<Texture2D>(texFilter);
            for (auto& [path, handle] : filtered) {
                ImGui::PushID(static_cast<int>(handle.index()));
                if (ImGui::Selectable(path.c_str())) {
                    if (m_pendingTextureNode) {
                        m_pendingTextureNode->textureHandle = handle;
                    }
                    m_pendingTextureNode = nullptr;
                    texFilter[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndPopup();
    }

    ed::Resume();

    ed::End();
    for (auto& node : graph.nodes) {
        ImVec2 pos = ed::GetNodePosition(toEdNodeId(node->id));
        node->m_pos = glm::vec2(pos.x, pos.y);
    }
    ed::SetCurrentEditor(nullptr);
    ImGui::End();

}

} // namespace ve
