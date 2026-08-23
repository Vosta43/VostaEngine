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
        bool m_usePBRPipeline = true;

        // Pre-baked IBL irradiance map for diffuse ambient lighting
        Ref<TextureCubeMap> m_irradianceMap;
        Ref<TextureCubeMap> m_prefilteredEnvMap;
        Ref<Texture2D> m_brdfLUT;
        AssetHandle m_bakedSkyboxHandle; // track skybox handle for IBL re-bake on change

        // Cloud noise volumes (baked once, same role as IBL). The shape volume
        // is a multi-octave RGBA Worley field; the detail volume is a second
        // independent Worley field (finer cells + different seed); the warp
        // volume holds two pre-summed FBM fields that bend the shape position.
        Ref<Texture3D> m_cloudNoiseTexture;
        uint32_t m_cloudWorleyCells = 1;
        Ref<Texture3D> m_cloudDetailTexture;
        uint32_t m_cloudDetailCells = 1;
        Ref<Texture3D> m_cloudWarpTexture;
        uint32_t m_cloudWarpCells = 1;
        // 2D weather map (R = per-position coverage, see WeatherMapBaker)
        Ref<Texture2D> m_cloudWeatherMap;

        // Atmosphere transmittance LUT, re-baked when the params change so the
        // inspector edits still react live (the analytic sky used to).
        Ref<Texture2D> m_transmittanceTexture;
        Ref<Texture3D> m_scatteringTexture;
        Ref<Texture3D> m_mieScatteringTexture;
        Ref<Texture3D> m_multipleScatteringTexture;
        AtmosphereParams m_cachedAtmosphereParams;
        bool m_hasCachedAtmosphere = false;

        // Light icon billboard
        AssetHandle m_pointLightIcon;
        float m_lightIconSize = 0.5f;
    };

}
