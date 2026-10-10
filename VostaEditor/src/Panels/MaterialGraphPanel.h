#pragma once

#include "Graph/NodeGraphPanel.h"
#include "Graph/MaterialEditorSchema.h"
#include "Renderer/SingleMaterial.h"

#include <cstdint>
#include <memory>
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

    // The file's last-write time as this panel last saw it, compared against a
    // fresh read each frame so a change landing on disk while the graph is open —
    // from an MCP tool, another editor, or a program outside this one — is noticed.
    // Shared with the registered saver (which writes the file without the panel
    // running), so its own write is recognised as ours rather than an external change.
    std::shared_ptr<int64_t> m_diskTicks = std::make_shared<int64_t>(0);
    // Set when the asset changed on disk while this panel has unsaved edits: the
    // toolbar offers reload (discard mine) or keep-mine. Never auto-resolved, so
    // neither side is silently thrown away.
    bool m_externalChange = false;
};

} // namespace ve
