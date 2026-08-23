#pragma once

#include <VostaEngine.h>
#include "Renderer/MaterialGraph.h"
#include "Renderer/MaterialNodes.h"
#include "Renderer/Material.h"
#include "AssetBrowserWidget.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <unordered_map>
#include <functional>

namespace ed = ax::NodeEditor;

namespace ve {

class MaterialGraphPanel {
public:
    MaterialGraphPanel();
    ~MaterialGraphPanel();

    // Render the full node-based material editor.
    // needsRecompile is set to true when graph topology changes.
    // openFlag is passed to ImGui::Begin to show the X close button.
    // originalMaterialHandle is used for the preview sphere thumbnail.
    void onGuiRender(Ref<Material> material, bool& needsRecompile,
                     bool* openFlag = nullptr, AssetHandle originalMaterialHandle = {});

private:
    // --- Pin / Node / Link ID encoding ---
    // Vosta uses {uint32_t nodeId, uint32_t pinIndex} for pins.
    // imgui-node-editor uses opaque uintptr_t-based IDs (ed::PinId, ed::NodeId, ed::LinkId).
    // On x64, uintptr_t is 64 bits, so we pack nodeId into the high 32 bits and
    // pinIndex into the low 32 bits.  This is lossless on 64-bit builds.
    static ed::PinId  toEdPinId(const ve::PinId& pid);
    static ve::PinId  fromEdPinId(ed::PinId epid);
    static ed::NodeId toEdNodeId(uint32_t id);
    static uint32_t   fromEdNodeId(ed::NodeId eid);
    static ed::LinkId toEdLinkId(uint32_t id);
    static uint32_t   fromEdLinkId(ed::LinkId eid);

    // --- Helpers ---
    ImColor getPinColor(PinType type) const;
    ImColor getNodeColor(NodeType type) const;
    void    drawPinIcon(const MaterialPin& pin, bool connected);
    void    drawNodeContent(Ref<MaterialNode> node);
    void    drawNodePin(const MaterialPin& pin, bool isInput);

    // --- Interaction handlers ---
    void handleLinkCreation(MaterialGraph& graph);
    void handleDeletion(MaterialGraph& graph);
    void addNodeToGraph(MaterialGraph& graph, Ref<MaterialNode> node, glm::vec2 pos);
    // --- Editor context ---
    ed::EditorContext* m_context = nullptr;

    // Map ed::NodeId -> node pointer for quick lookup in callbacks.
    std::unordered_map<uintptr_t, Ref<MaterialNode>> m_idToNode;

    // Deferred texture picker — TextureSamplerNode awaiting texture selection
    // rendered inside the ed::Suspend zone to avoid node-bound clipping.
    TextureSamplerNode* m_pendingTextureNode = nullptr;
    ImVec2              m_texPickerPos;       // screen position to anchor the picker
};

} // namespace ve
