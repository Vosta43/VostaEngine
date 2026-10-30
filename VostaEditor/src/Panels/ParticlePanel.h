#pragma once

#include <VostaEngine.h>

#include <imgui_node_editor.h>

#include "Particle/EmitterAsset.h"
#include "Particle/ParticleEmitter.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Texture.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ve {

    // UE Niagara Emitter-style editor: a live viewport on the left, a node-editor
    // canvas with one Emitter node (the four stages and their modules live in the
    // node body) on the right. Structural edits recompile and reset particles;
    // parameter edits are pushed into the running instance without a reset.
    class ParticlePanel {
    public:
        ~ParticlePanel();

        // Binds the panel to a particle .veasset and opens it. Edits write back to
        // the same file.
        void openFile(const std::string& path);

        void onGuiRender(bool* openFlag = nullptr);

    private:
        void loadFromDisk();
        void saveToFile();
        void markDirty();
        // Compiles only when the module stack changed. On success the fresh layouts
        // and programs replace the asset's; the instance resets.
        void compileIfNeeded();
        void drawNodeCanvas();
        void drawStageStack();
        // Drawn inside ed::Suspend(): a popup opened in the node body is mapped
        // into canvas space and would land in the wrong place.
        void drawAddModulePopup();
        void drawDetails();
        void renderPreview();
        void ensurePreviewResources();

        bool findSelected(uint32_t& stage, size_t& index) const;

        EmitterAsset m_asset;
        ParticleEmitterInstance m_emitter;
        bool m_emitterReady = false;

        // Structure hash the asset was last compiled at, so a compile failure is not
        // retried every frame. m_compileFailed suppresses live param application.
        uint64_t m_compiledStruct = 0;
        bool m_compiledOnce = false;
        bool m_compileFailed = false;
        std::string m_compileError;
        std::string m_loadWarning;
        bool m_readOnly = false;

        bool m_playing = true;
        float m_timeScale = 1.0f;

        Ref<Framebuffer> m_target;
        Ref<Texture2D>   m_white;

        float m_yaw = -32.0f;
        float m_pitch = 14.0f;
        float m_distance = 2600.0f;
        glm::vec3 m_focus{ 0.0f, 0.0f, -700.0f };

        bool m_hasSelection = false;
        Guid16 m_selGuid;

        uint64_t m_assetHash = 0;
        uint64_t m_savedHash = 0;

        // Shared with the registered saver so its own write does not read back as
        // an external change, same contract as NoisePanel.
        std::shared_ptr<int64_t> m_diskTicks = std::make_shared<int64_t>(0);
        bool m_externalChange = false;

        std::string m_filePath;

        // One emitter node on a node-editor canvas. Created lazily: panels exist
        // before the ImGui context does.
        ax::NodeEditor::EditorContext* m_nodeCanvas = nullptr;
        bool m_nodePlaced = false;

        // Add-module menu state; anchored in screen space at the "+" button.
        int m_addMenuStage = -1;
        float m_addMenuX = 0.0f;
        float m_addMenuY = 0.0f;
        bool m_openAddMenu = false;
        bool m_addMenuShowing = false;
    };

}
