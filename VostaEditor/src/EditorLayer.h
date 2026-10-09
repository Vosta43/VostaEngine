#pragma once
#include <VostaEngine.h>
#include <Core/Project.h>

#include <cstdint>
#include <filesystem>

#include "EditorViewController.h"
#include "Panels/AiPanel.h"
#include "Panels/FileBrowser.h"
#include "Panels/MaterialGraphPanel.h"
#include "Panels/NoisePanel.h"
#include "Panels/TerrainMapPanel.h"
#include "Panels/TexturePreviewPanel.h"
#include "SubEditors/MaterialLayerEditor.h"
#include "SubEditors/LayeredMaterialEditor.h"
#include "SubEditors/TerrainEditor.h"

// TODO :Remove it
#include <glm.hpp>

namespace ve {

    class EditorLayer : public Layer {
    public:
        EditorLayer();

        void onAttach() override;
        void onDetach() override;
        void onImGuiRender() override;

        void renderMenuBar();

        // Returns false when nothing was written (no file name yet, or the write
        // failed).
        bool saveScene();
        void loadScene();

        // Current document, for the editor-side adapter (EditorMcpTools) to bind
        // its scene provider and report the saved path. Ordinary accessors -- no
        // protocol vocabulary.
        Ref<Scene> currentScene();
        const std::filesystem::path& currentScenePath() const;
        // Double-click entry point from the file browser: dispatches on the
        // file's kind (a .veworld loads as a scene, a material .veasset opens
        // the node editor).
        void openFile(const std::string& path);

        void bootstrapProject();
        void applyProject(const std::string& projectDir);

        void onEntitySelected(uint32_t entityID);
        void openMaterialEditor(AssetHandle handle);
        void openMaterialLayerEditor(AssetHandle handle);
        // Opens the standalone layer-stack editor on a LayeredMaterial asset.
        void openLayeredMaterialEditor(AssetHandle handle);
        // Opens the texture viewer on a texture .veasset path.
        void openTexturePreview(const std::string& path);
        void performPicking();
        // Raycast the terrain under the viewport cursor and paint one brush dab.
        void paintTerrain();
        void onUpdate() override;

    private:
        // One row in the Load dialog: a .veworld file found in the scenes folder.
        struct SceneFile {
            std::string name;
            uintmax_t size = 0;
        };

        void refreshSceneFileList();
        void refreshProjectList();

        // Loads a scene from an absolute path and rebinds the editor to it.
        bool openScene(const std::filesystem::path& absolutePath);

        // "<sceneStem>_terrain.veasset" next to the scene, the one file a terrain's
        // painted weights are written to.
        static std::string terrainDataPathFor(const std::filesystem::path& scenePath);
        // Write the painted weights and point the system's handle at the file.
        void writeTerrainDataFor(const std::filesystem::path& scenePath);
        // Mark the terrain-data file dirty for the File Browser's Save (N) button.
        void syncTerrainDataDirty();

        // Editor viewport camera, persisted as a "<scene>.camera.json" sidecar
        // next to the scene file so reopening a scene resumes where the user left
        // off. Session state, not document state -- deliberately not written into
        // the .veworld.
        void restoreCameraFor(const std::filesystem::path& scenePath);
        // Writes the current viewport camera under m_currentSceneName. Returns
        // true if anything was written. force skips the "unchanged" short-circuit
        // (needed on Save As, where the name changed but the camera did not).
        bool saveCurrentCamera(bool force = false);

        static glm::mat4 makeBillboard(const glm::vec3& position, const glm::mat4& viewMatrix);

        void renderLightBillboards(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, const glm::vec3& cameraPos);

    private:
        Ref<Shader> m_Shader;
        Ref<Shader> m_TextureShader;
        Ref<Shader> m_pickingShader;

        CameraController m_cameraController = CameraController(-1.6f, 1.6f, -0.9f, 0.9f);

        FileBrowser m_fileBrowser;
        // Last AssetLibrary revision the file browser was told about. Anything that
        // writes an asset bumps the library's counter, so comparing it here catches
        // writes the editor did not make itself.
        uint64_t m_seenAssetRevision = 0;
        EditorViewController m_editorView;
        MaterialGraphPanel m_materialGraphPanel;
        NoisePanel m_noisePanel;
        bool m_showNoiseEditor = false;

        Ref<SingleMaterial> m_openMaterial = nullptr;
        AssetHandle m_openMaterialOriginalHandle;
        bool m_showMaterialEditor = false;
        bool m_needsMaterialRecompile = false;

        MaterialLayerEditor m_materialLayerEditor;
        bool m_showMaterialLayerEditor = false;

        LayeredMaterialEditor m_layeredMaterialEditor;
        bool m_showLayeredMaterialEditor = false;

        TexturePreviewPanel m_texturePreview;
        bool m_showTexturePreview = false;

        // Chat + tool console. Owns the AgentSession, so its shutdown() is the
        // one thing that must run before the layer goes away.
        AiPanel m_aiPanel;
        bool m_showAi = false;

        // Bird's-eye tile grid, opened from the terrain system inspector or the
        // Window menu. Holds no scene; the current one is passed in each frame.
        TerrainMapPanel m_terrainMap;
        bool m_showTerrainMap = false;

        uint32_t m_selectedEntity = UINT32_MAX;

        Ref<Framebuffer> m_framebuffer;
        Ref<Framebuffer> m_pickingFramebuffer;

        bool m_needsPicking = false;
        glm::vec2 m_pickPos;

        glm::vec2 m_mouseViewportPos = { 0.0f, 0.0f };

        bool m_showSavePopup = false;
        bool m_showLoadPopup = false;
        // Scene the editor is currently working on. Set on save/load, restored
        // from the project's Saved/last_scene.txt at startup, and used to
        // prefill both dialogs.
        std::string m_currentSceneName = "";
        // Absolute path of the scene that is actually open, empty when no scene
        // is bound. The camera sidecar is derived from this, so a scene the
        // editor has not loaded can never have its saved view overwritten.
        std::filesystem::path m_currentScenePath;
        std::string m_saveFileName = "scene.veworld";
        std::string m_loadFileName = "";
        char m_saveFileNameBuffer[256] = "scene.veworld";
        char m_loadFileNameBuffer[256] = "";

        // Rescanned each time the Load dialog opens.
        std::vector<SceneFile> m_sceneFiles;

        // Last camera state written to disk, so saveCurrentCamera() can skip
        // no-op writes and onUpdate can throttle the idle save.
        glm::vec3 m_savedCamPos{ 0.0f };
        float m_savedCamYaw = 0.0f;
        float m_savedCamPitch = 0.0f;
        double m_lastCameraSaveTime = 0.0;

        // Currently open project (folder under <root>/Projects, or the built-in
        // SandBox). Switched through the File menu.
        ProjectInfo m_project;
        bool m_showNewProjectPopup = false;
        bool m_showOpenProjectPopup = false;
        char m_newProjectNameBuffer[256] = "NewProject";
        // Project folders rescanned each time the Open Project dialog opens.
        std::vector<std::string> m_projectDirs;
        std::string m_openProjectDir;

        Ref<RenderPipeline> m_renderPipeline;

        // Light icon billboard
        AssetHandle m_pointLightIcon;
        float m_lightIconSize = 0.5f;

        // Viewport toolbar toggle icons.
        Ref<Texture2D> m_wireframeIcon;
        Ref<Texture2D> m_groundGridIcon;

        // Terrain material paint brush, driven from the TerrainSystem inspector.
        // While m_showTerrainBrush is on, LMB in the viewport paints instead of
        // picking; m_terrainPainting tracks a stroke so drags keep painting.
        TerrainEditor m_terrainEditor;
        bool m_showTerrainBrush = false;
        bool m_terrainPainting = false;
    };

}
