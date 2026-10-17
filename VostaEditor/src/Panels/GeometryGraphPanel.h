#pragma once

#include <VostaEngine.h>

#include "Geometry/GeometryGraph.h"
#include "Graph/GeometryEditorSchema.h"
#include "Graph/NodeGraphPanel.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/RenderPipeline.h"
#include "Renderer/StaticMesh.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ve {

    // Procedural-geometry editor: a node graph whose evaluation yields a CPU mesh.
    // The canvas edits the graph; a live offscreen 3D preview shows the result and a
    // "Bake to Mesh" button writes it out as a static-mesh .veasset. Both the preview
    // and the bake read the same evaluator, so what you see is what gets baked.
    class GeometryGraphPanel {
    public:
        // Binds the panel to a geometry .veasset and opens it with that file's graph.
        // Edits write back to the same file.
        void openFile(const std::string& path);

        void onGuiRender(bool* openFlag = nullptr);

    private:
        void ensureDefaultGraph();
        void loadGraphFromDisk();
        // Evaluates the graph to a mesh, uploads it to a transient GPU mesh, and
        // re-renders the preview framebuffer. Doesn't touch the file.
        void rebuildPreview();
        // Renders the current preview mesh with m_yaw/m_pitch/m_distance into the
        // preview target. Cheap enough to run every frame the panel is open.
        void renderPreview();
        void bakeToMesh();
        void saveToFile();
        void markDirty();

        GeometryGraph         m_graph;
        NodeGraphPanel        m_canvas{ "GeometryCanvas" };
        GeometryEditorSchema  m_schema;

        static constexpr int kPreviewSize = 512;

        // Live preview: an offscreen pipeline renders the transient mesh into an FBO
        // shown with ImGui::Image. Initialized lazily on first draw (needs a GL context).
        RenderPipeline   m_pipeline;
        bool             m_pipelineReady = false;
        // The preview renders on demand rather than every frame: set on a graph or
        // camera change; cleared after each render.
        bool             m_previewDirty = true;
        Ref<Framebuffer> m_previewTarget;
        Ref<StaticMesh>  m_previewMesh;
        std::string      m_previewError;
        uint32_t         m_vertexCount = 0;
        uint32_t         m_triangleCount = 0;

        // Orbit camera, re-framed from the mesh bounds on every rebuild.
        float     m_yaw = -35.0f;
        float     m_pitch = 22.0f;
        glm::vec3 m_target = glm::vec3(0.0f);
        float     m_distance = 3.0f;

        // The asset this graph was baked into, so a re-bake overwrites it rather than
        // piling up "_mesh 1", "_mesh 2" copies. Empty until the first bake.
        std::string m_bakedPath;

        // Hash of the graph the preview currently shows / the file holds. Tracked
        // apart so a deferred save can lag the live preview.
        uint32_t m_graphHash = 0;
        uint32_t m_savedHash = 0;
        // Geometry hash folded with the referenced noise graphs' content hashes. Kept
        // separate so editing a field the graph points at refreshes the preview without
        // rewriting the geometry file, whose own contents did not change.
        uint32_t m_previewHash = 0;

        // The file's last-write time as this panel last saw it, compared each frame so
        // an outside write (MCP tool, another editor) is noticed. Shared with the
        // registered saver, whose own write must read back as ours.
        std::shared_ptr<int64_t> m_diskTicks = std::make_shared<int64_t>(0);
        bool        m_externalChange = false;
        std::string m_filePath;
    };

}
