#pragma once
#include <VostaEngine.h>

#include "EditorViewController.h"
#include "Panels/FileBrowser.h"
#include "Panels/MaterialGraphPanel.h"

// TODO :Remove it
#include <glm.hpp>

namespace ve {

    class EditorLayer : public Layer {
    public:
        EditorLayer();

        void onAttach() override;
        void onImGuiRender() override;

        void renderMenuBar();

        void saveScene();
        void loadScene();

        void onEntitySelected(uint32_t entityID);
        void openMaterialEditor(AssetHandle handle);
        void performPicking();
        void onUpdate() override;

    private:
        static glm::mat4 makeBillboard(const glm::vec3& position, const glm::mat4& viewMatrix);

        void renderLightBillboards(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, const glm::vec3& cameraPos);

    private:
        Ref<Shader> m_Shader;
        Ref<Shader> m_TextureShader;
        Ref<Shader> m_pickingShader;

        CameraController m_cameraController = CameraController(-1.6f, 1.6f, -0.9f, 0.9f);

        FileBrowser m_fileBrowser;
        EditorViewController m_editorView;
        MaterialGraphPanel m_materialGraphPanel;

        Ref<Material> m_openMaterial = nullptr;
        AssetHandle m_openMaterialOriginalHandle;
        bool m_showMaterialEditor = false;
        bool m_needsMaterialRecompile = false;

        uint32_t m_selectedEntity = UINT32_MAX;

        Ref<Framebuffer> m_framebuffer;
        Ref<Framebuffer> m_pickingFramebuffer;

        bool m_needsPicking = false;
        glm::vec2 m_pickPos;

        glm::vec2 m_mouseViewportPos = { 0.0f, 0.0f };

        bool m_showSavePopup = false;
        bool m_showLoadPopup = false;
        std::string m_saveFileName = "scene.veworld";
        std::string m_loadFileName = "";
        char m_saveFileNameBuffer[256] = "scene.veworld";
        char m_loadFileNameBuffer[256] = "";

        Ref<RenderPipeline> m_renderPipeline;

        // Light icon billboard
        AssetHandle m_pointLightIcon;
        float m_lightIconSize = 0.5f;
    };

}
