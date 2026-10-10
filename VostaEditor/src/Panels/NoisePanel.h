#pragma once

#include <VostaEngine.h>
#include "Noise/NoiseGraph.h"
#include "Graph/NodeGraphPanel.h"
#include "Graph/NoiseEditorSchema.h"
#include "Renderer/Texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ve {

    // Standalone noise editor: a node graph (Noise Unit / math / Output) plus a live
    // CPU preview baked by evaluating the graph. Editing happens in the canvas; the
    // preview is a hash-gated bake — a parameter or wiring change re-evaluates.
    class NoisePanel {
    public:
        // Binds the panel to a noise .veasset and opens it with that file's graph.
        // Edits write back to the same file.
        void openFile(const std::string& path);

        void onGuiRender(bool* openFlag = nullptr);

    private:
        // Seeds the default Noise Unit -> Output graph when none is loaded.
        void ensureDefaultGraph();
        // Reads m_filePath into m_graph (empty/legacy files fall back to the default
        // graph) and rebuilds the preview. Shared by open and by a reload after an
        // external write.
        void loadGraphFromDisk();
        void rebuildPreview();
        void saveToFile();
        // Registers a self-contained saver (path + graph snapshot) with the dirty
        // registry, so the global Save covers the interval before the deferred
        // auto-save fires.
        void markDirty();
        // Bakes the preview field out as a texture .veasset next to the source.
        void exportTexture();

        NoiseGraph      m_graph;
        NodeGraphPanel  m_canvas{ "NoiseCanvas" };
        NoiseEditorSchema m_schema;

        static constexpr int kPreviewSize = 256;
        Ref<Texture2D> m_preview;
        std::vector<uint8_t> m_pixels;
        // Hash of the graph the preview currently shows / the file holds. The two are
        // tracked apart so a deferred save can lag the live preview.
        uint32_t m_graphHash = 0;
        uint32_t m_savedHash = 0;

        // The file's last-write time as this panel last saw it, compared against a
        // fresh read each frame so a write landing on disk while the graph is open —
        // from an MCP tool, another editor, or a program outside this one — is
        // noticed. Shared with the registered saver (which writes the file without
        // the panel running), so its own write reads back as ours, not a conflict.
        std::shared_ptr<int64_t> m_diskTicks = std::make_shared<int64_t>(0);
        bool     m_externalChange = false;

        // Empty when the panel is a scratch pad (opened from the Window menu rather
        // than by double-clicking an asset); edits are then not persisted.
        std::string m_filePath;
    };

}
