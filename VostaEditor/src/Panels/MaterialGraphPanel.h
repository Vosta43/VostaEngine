#pragma once

#include "Graph/NodeGraphPanel.h"
#include "Graph/MaterialEditorSchema.h"
#include "Renderer/SingleMaterial.h"

#include <cstdint>
#include <string>

namespace ve {

// Material editor shell: owns the window, the compile toolbar and the preview
// panel, and hosts a generic NodeGraphPanel for the graph canvas. It edits the
// graph, which only the single-surface kind carries.
class MaterialGraphPanel {
public:
    // needsRecompile is set to true when graph topology changes.
    // openFlag is passed to ImGui::Begin to show the X close button.
    // originalMaterialHandle is used for the preview sphere thumbnail.
    void onGuiRender(Ref<SingleMaterial> material, bool& needsRecompile,
                     bool* openFlag = nullptr, AssetHandle originalMaterialHandle = {});

private:
    // Registers a self-contained saver (path + material Ref) so the global Save
    // still works after this material is closed or another one is opened.
    // assetKey is the registry-relative path the material is filed under.
    void markDirty(const Ref<SingleMaterial>& material, const std::string& assetKey);

    NodeGraphPanel       m_canvas;
    MaterialEditorSchema m_schema;

    // Hash of the graph as last seen, so a frame that changes the graph marks the
    // material dirty once. m_lastName rebaselines it when a different material is
    // opened, so opening a material is not itself an edit.
    uint32_t    m_graphHash = 0;
    std::string m_lastName;
};

} // namespace ve
