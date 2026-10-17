#pragma once
#include <VostaEngine.h>

#include <cstdint>
#include <filesystem>
#include <string>

#include "EditorCameraStore.h"
#include "EditorViewController.h"
#include "PlaySession.h"
#include "SceneDocument.h"

#include "Panels/AiPanel.h"
#include "Panels/AssetEditorHost.h"
#include "Panels/EditorDialogs.h"
#include "Panels/FileBrowser.h"
#include "Panels/TerrainMapPanel.h"
#include "SubEditors/TerrainEditor.h"
#include "Viewport/ViewportOverlays.h"
#include "Viewport/ViewportToolbar.h"

#include <glm.hpp>

namespace ve {

    // The editor's orchestrator: it owns the dockspace, the viewport window and
    // the shared state (selection, viewport cursor, brush flags, play session)
    // that the editor's modules read and write by reference, and it decides the
    // ORDER of operations that cross modules -- the camera sidecar around a scene
    // switch, terrain data before scene serialization, the play-session guards.
    // The work itself lives in the modules it hosts.
    class EditorLayer : public Layer {
    public:
        EditorLayer();

        void onAttach() override;
        void onDetach() override;
        void onUIRender() override;
        void onUpdate() override;

        // Save the open scene under the current save name. Returns false when
        // nothing was written (no file name yet, or the write failed).
        bool saveScene();
        // Load the .veworld named `name` from the project's scenes folder.
        void loadScene(const std::string& name);

        // Current document, for the editor-side adapter (EditorMcpTools) to bind
        // its scene provider and report the saved path. Ordinary accessors -- no
        // protocol vocabulary.
        Ref<Scene> currentScene();
        const std::filesystem::path& currentScenePath() const;

        // Double-click entry point from the file browser: dispatches on the
        // file's kind (a .veworld loads as a scene, a .veasset opens the matching
        // asset editor).
        void openFile(const std::string& path);

        void bootstrapProject();
        void applyProject(const std::string& projectDir);

        void onEntitySelected(uint32_t entityID);
        // Raycast the terrain under the viewport cursor and paint one brush dab.
        void paintTerrain();

        // Play-in-Editor transport. startPlay duplicates the scene into a runtime
        // session and points the viewport at it; stopPlay discards it and restores
        // the authored view.
        void startPlay();
        void stopPlay();

    private:
        // Draws the File / Window menu bar, into the current window.
        void renderMenuBar();

        // Loads a scene from an absolute path and rebinds the editor to it.
        bool openScene(const std::filesystem::path& absolutePath);

        // Points the editor view at `scene` and wires its asset / inspector
        // callbacks to this layer's modules, then hands the scene to the viewport.
        void bindEditorView(Ref<Scene> scene);

    private:
        FileBrowser m_fileBrowser;
        // Last AssetLibrary revision the file browser was told about. Anything that
        // writes an asset bumps the library's counter, so comparing it here catches
        // writes the editor did not make itself.
        uint64_t m_seenAssetRevision = 0;

        EditorViewController m_editorView;
        SceneView m_sceneView;

        // Hosts the .veasset editors (material graph, layer stack, material layer,
        // noise graph, texture viewer).
        AssetEditorHost m_assetEditors;
        // The editor's modal dialogs (Save / Load scene, New / Open project,
        // Package). Drives its work back through callbacks this layer sets.
        EditorDialogs m_dialogs;

        ViewportToolbar m_toolbar;
        ViewportOverlays m_overlays;

        // Scene + project identity and terrain-data persistence, plus the editor
        // camera sidecar. Neither decides the ORDER of camera saves around a scene
        // switch -- this layer does.
        SceneDocument m_document;
        EditorCameraStore m_cameraStore;

        // Chat + tool console. Owns the AgentSession, so its shutdown() is the
        // one thing that must run before the layer goes away.
        AiPanel m_aiPanel;
        bool m_showAi = false;

        // Bird's-eye tile grid, opened from the terrain system inspector or the
        // Window menu. Holds no scene; the current one is passed in each frame.
        TerrainMapPanel m_terrainMap;
        bool m_showTerrainMap = false;

        uint32_t m_selectedEntity = UINT32_MAX;

        // Viewport-relative cursor, in the space the overlays use for picking and
        // the terrain brush ring.
        glm::vec2 m_mouseViewportPos = { 0.0f, 0.0f };

        // Terrain material paint brush, driven from the TerrainSystem inspector.
        // While m_showTerrainBrush is on, LMB in the viewport paints instead of
        // picking; m_terrainPainting tracks a stroke so drags keep painting.
        TerrainEditor m_terrainEditor;
        bool m_showTerrainBrush = false;
        bool m_terrainPainting = false;

        // Play-in-Editor session. While live the viewport renders its runtime
        // duplicate and the editor panels are locked.
        PlaySession m_playSession;
        // Viewport camera selection captured on Play and restored on Stop.
        SceneView::CameraSource m_prePlayCameraSource = SceneView::CameraSource::Editor;
        Entity m_prePlayCameraEntity;
    };

}
