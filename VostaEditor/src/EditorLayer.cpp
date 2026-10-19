#include "EditorLayer.h"

#include "EditorMcpTools.h"
#include "EditorPaths.h"

#include "imgui.h"
#include "imguizmo.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

#include "Core/ResourceManager.h"
#include "Core/AssetConfig.h"
#include "Asset/AssetLibrary.h"
#include "Asset/BuiltinReousrces.h"
#include "Asset/Utils.h"
#include "Renderer/ThumbnailRenderer.h"

namespace ve {

    namespace {

        // Push the default font at 1.5x for the menu-bar labels.
        // Matching PopFont() must follow. 1.92 bakes the size on demand.
        void pushFileMenuFont() {
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.5f);
        }

        // The editor view's scene scaffolding (shaders, pipeline, camera input)
        // is shared with SandBox through SceneView. Only the asset paths differ.
        SceneViewConfig editorViewConfig() {
            SceneViewConfig cfg;
            cfg.width        = 1280;
            cfg.height       = 720;
            cfg.pipelinePath = "VostaEngine/resources/pipelines/default.json";
            cfg.perspective  = true;
            cfg.sceneShaders = { "VostaEngine/resources/shaders/Model.glsl",
                                 "VostaEngine/resources/shaders/Texture.glsl" };
            return cfg;
        }

        constexpr size_t kSceneLoadChunk = 64;
        constexpr auto kSceneLoadBudget = std::chrono::milliseconds(8);

    }

    EditorLayer::EditorLayer()
        : m_editorView(CreateRef<Scene>(),
                       [this](AssetHandle handle) { m_assetEditors.openMaterial(handle); },
                       [this](AssetHandle handle) { m_assetEditors.openMaterialLayer(handle); },
                       [this](Scene& scene) { m_terrainEditor.drawInspector(m_showTerrainBrush, scene); },
                       [this]() { m_showTerrainMap = true; })
        , m_sceneView(m_editorView.getScene(), editorViewConfig())
    {
        m_toolbar.setCallbacks({
            [this]() { startPlay(); },
            [this]() { stopPlay(); }
        });

        m_dialogs.setCallbacks({
            [this](const std::string& name) -> bool {
                m_document.setSaveName(name);
                return saveScene();
            },
            [this](const std::string& name) { loadScene(name); },
            [this](const std::string& dir) { applyProject(dir); }
        });
    }

    void EditorLayer::onAttach() {

        bootstrapProject();
        m_fileBrowser.setRootPath(toProjectAbsolute("content"));
        m_fileBrowser.setOnFileSelect([this](const std::string& path) { openFile(path); });
        m_fileBrowser.setOnSetDefaultScene([this](const std::string& path) {
            m_document.project().defaultScene = toProjectRelative(path);
            Project::save(m_document.project());
            m_fileBrowser.setDefaultScenePath(path);
        });
        // Reflect the project's current default scene in the browser (check mark).
        m_fileBrowser.setDefaultScenePath(m_document.project().defaultScene.empty()
            ? std::string() : toProjectAbsolute(m_document.project().defaultScene));
        m_dialogs.setContext(m_document.currentName(), m_document.project());

        // SceneView owns the shared scene scaffolding: content shaders, the
        // renderer subsystem init, the render pipeline, and camera input.
        m_sceneView.onAttach();

        // Picking and the overlay billboards are editor-only; the overlays own them.
        m_toolbar.attach();
        m_overlays.attach();

        Application::get().getDispatcher()->subscribe(Event::Type::MouseMoved,
            [this](Event& e) {
                auto& me = static_cast<MouseMovedEvent&>(e);
                float screenX = me.getX();
                float screenY = me.getY();

                glm::vec2 viewportPos = Application::get().getGuiLayer()->getViewportBounds().first;
                m_mouseViewportPos.x = screenX - viewportPos.x;
                m_mouseViewportPos.y = screenY - viewportPos.y;
            });

        Application::get().getDispatcher()->subscribe(Event::Type::MouseButtonPressed,
            [this](Event& e) {
                auto& me = static_cast<MouseButtonPressedEvent&>(e);
                // Left click picks an entity; right-click camera control is
                // SceneView's job.
                if (me.getButton() == VE_MOUSE_BUTTON_LEFT && Application::get().isViewportHovered()) {

                    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;

                    // While the terrain brush is on, LMB paints instead of picking.
                    // The stroke itself is driven from the viewport's own mouse
                    // state below (see the ImGui brush block). Picking is off during
                    // a play session too.
                    if (m_showTerrainBrush || m_playSession.isLive())
                        return;

                    float fboY = (float)m_sceneView.getHeight() - m_mouseViewportPos.y;
                    m_overlays.requestPick({ m_mouseViewportPos.x, fboY });
                }

            });

        Application::get().getDispatcher()->subscribe(Event::Type::MouseButtonReleased,
            [this](Event& e) {
                auto& me = static_cast<MouseButtonReleasedEvent&>(e);
                if (me.getButton() == VE_MOUSE_BUTTON_LEFT) {
                    m_terrainPainting = false;
                }
            });

        // Shared geometry the editor can drop into a scene without an import.
        ve::BuiltinResources::getBuiltinSphere();
        for (const char* mesh : { "cube", "plane", "cylinder", "cone", "capsule", "torus" }) {
            ve::ResourceManager::store<ve::StaticMesh>(
                std::string("VostaEngine/resources/models/") + mesh + ".obj");
        }

        // The editor-side adapter owns every tool-layer wire detail (the scene
        // provider, the save_scene command, the {error} envelopes, the dispatch
        // queue). This layer stays protocol-agnostic: it just forwards.
        EditorMcpTools::attach(*this);
    }

    void EditorLayer::onDetach() {
        // A turn may be mid-flight on a JobSystem worker. Stop it first: it both
        // blocks on and drains the tool queue, so it must finish before the
        // adapter drops the provider.
        m_aiPanel.shutdown();

        EditorMcpTools::detach();
    }

    void EditorLayer::onUIRender() {
        static bool dockspaceOpen = true;
        static bool opt_fullscreen_persistant = true;
        bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        if (opt_fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        // Size the menu-bar row for the larger labels, then restore the normal
        // font for the window's content.
        pushFileMenuFont();
        ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
        ImGui::PopFont();
        ImGui::PopStyleVar();

        renderMenuBar();
        // The modal dialogs are opened from the menu bar, so they are drawn here,
        // inside the dock host window.
        m_dialogs.onGuiRender();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        Application& app = Application::get();
        float w = (float)app.getWindow()->getWidth();
        float h = (float)app.getWindow()->getHeight();

        ImGui::SetNextWindowSize(ImVec2(w, 250.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(0, h - 250.0f), ImGuiCond_FirstUseEver);
        m_fileBrowser.render();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        {
            m_toolbar.onGuiRender(m_sceneView, m_editorView.getScene(), m_playSession);

            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();

            glm::vec2 pos(viewportPos.x, viewportPos.y);
            glm::vec2 size(viewportSize.x, viewportSize.y);
            Application::get().getGuiLayer()->setViewportBounds(pos, size);
            Application::get().getGuiLayer()->setViewportWindowHovered(ImGui::IsWindowHovered());
            // Camera input follows viewport hover, computed by the Gui layer.
            m_sceneView.setInputEnabled(Application::get().isViewportHovered());

            // The terrain brush is driven from ImGui's mouse state, not the
            // MouseMoved event: the Gui layer marks mouse-move events handled
            // (io.WantCaptureMouse && !viewport-window-hovered) while a button is
            // held over the viewport, so an event-driven brush froze the cursor
            // ring and could not advance a drag. The rect test is geometric, so it
            // keeps working mid-drag.
            const bool mouseInViewport = viewportSize.x > 0.0f && viewportSize.y > 0.0f
                && ImGui::IsMouseHoveringRect(viewportPos,
                       ImVec2(viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y), false);
            if (mouseInViewport) {
                const ImVec2 mp = ImGui::GetMousePos();
                m_mouseViewportPos = { mp.x - viewportPos.x, mp.y - viewportPos.y };
            }
            // Hold-to-paint: only a physically held LMB advances the stroke, so a
            // click paints one dab and releasing ends it.
            if (!m_playSession.isLive() && m_showTerrainBrush && mouseInViewport
                && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()
                && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                m_terrainPainting = true;
                paintTerrain();
            } else {
                m_terrainPainting = false;
            }

            if (viewportSize.x > 0 && viewportSize.y > 0) {
                m_sceneView.setSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
                m_overlays.ensurePickTarget((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);

                ImGui::Image((void*)(intptr_t)m_sceneView.getColorAttachmentId(),
                    viewportSize, ImVec2(0, 1), ImVec2(1, 0));

                m_overlays.drawOverlays(m_sceneView, m_editorView.getScene(), m_selectedEntity,
                                        viewportPos, viewportSize, mouseInViewport,
                                        m_playSession.isLive(), m_showTerrainBrush,
                                        m_terrainEditor, m_mouseViewportPos);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::End();

        m_editorView.onGuiRender(m_selectedEntity);

        m_assetEditors.onGuiRender();

        if (m_showTerrainMap) {
            if (Ref<Scene> scene = m_editorView.getScene())
                m_terrainMap.onGuiRender(*scene, &m_showTerrainMap);
        }

        if (m_showAi) {
            m_aiPanel.onGuiRender(&m_showAi);
        }

        // The brush toggle lives in the TerrainSystem inspector now, so painting
        // can be switched off mid-stroke; drop the stroke state with it.
        if (!m_showTerrainBrush)
            m_terrainPainting = false;

        if (m_pendingLoad)
            renderSceneLoadOverlay();
    }

    void EditorLayer::renderMenuBar() {
        // "File" is drawn larger; its dropdown items stay at the normal size.
        pushFileMenuFont();

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::PopFont();

                // Project/scene switching and saving are disabled while a play
                // session is live: the viewport renders the runtime duplicate and
                // the authored document must not move.
                ImGui::BeginDisabled(m_playSession.isLive());

                if (ImGui::MenuItem("New Project..."))
                    m_dialogs.openNewProject();

                if (ImGui::MenuItem("Open Project..."))
                    m_dialogs.openOpenProject();

                if (ImGui::MenuItem("Package Project..."))
                    m_dialogs.openPackage();

                ImGui::Separator();

                if (ImGui::MenuItem("Save", "Ctrl+S"))
                    m_dialogs.openSave();

                if (ImGui::MenuItem("Load", "Ctrl+O"))
                    m_dialogs.openLoad();

                ImGui::EndDisabled();

                ImGui::Separator();

                pushFileMenuFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                ImGui::PopFont();

                if (ImGui::MenuItem("Noise Editor"))
                    m_assetEditors.showNoiseEditor();

                if (ImGui::MenuItem("Geometry Editor"))
                    m_assetEditors.showGeometryEditor();

                if (ImGui::MenuItem("Terrain Map"))
                    m_showTerrainMap = true;

                if (ImGui::MenuItem("AI Assistant"))
                    m_showAi = true;

                pushFileMenuFont();
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::PopFont();
    }

    Ref<Scene> EditorLayer::currentScene() {
        return m_editorView.getScene();
    }

    const std::filesystem::path& EditorLayer::currentScenePath() const {
        return m_document.currentPath();
    }

    bool EditorLayer::saveScene()
    {
        if (m_playSession.isLive()) {
            VE_CORE_WARN_PRINT("%s", "Cannot save the scene while a play session is running.");
            return false;
        }

        if (!m_document.save(m_editorView.getScene()))
            return false;

        // Forced: a Save As renames the file while the camera is unchanged, so
        // the change-detect would otherwise skip the write for the new name.
        m_cameraStore.save(m_sceneView, m_document.currentPath(), /*force=*/true);
        m_dialogs.setContext(m_document.currentName(), m_document.project());
        return true;
    }

    void EditorLayer::loadScene(const std::string& name)
    {
        if (name.empty()) {
            VE_CORE_ERROR("No filename specified for loading!");
            return;
        }

        std::string fileName = name;
        if (fileName.find(".veworld") == std::string::npos)
            fileName += ".veworld";

        openScene(scenesDir() / fileName);
    }

    bool EditorLayer::openScene(const std::filesystem::path& absolutePath)
    {
        if (m_pendingLoad)
            return false;

        if (m_playSession.isLive()) {
            VE_CORE_WARN_PRINT("%s", "Cannot open a scene while a play session is running.");
            return false;
        }

        // Persist the outgoing scene's camera before the name flips; the
        // incoming scene's saved view is restored once it has landed.
        m_cameraStore.save(m_sceneView, m_document.currentPath());

        beginSceneLoad(absolutePath, absolutePath.filename().string());
        return true;
    }

    void EditorLayer::bindEditorView(Ref<Scene> scene) {
        m_editorView.rebind(scene,
            [this](AssetHandle handle) { m_assetEditors.openMaterial(handle); },
            [this](AssetHandle handle) { m_assetEditors.openMaterialLayer(handle); },
            [this](Scene& s) { m_terrainEditor.drawInspector(m_showTerrainBrush, s); },
            [this]() { m_showTerrainMap = true; });
        m_sceneView.setScene(m_editorView.getScene());
    }

    void EditorLayer::openFile(const std::string& path)
    {
        if (utils::getExtension(path) == ".veworld") {
            openScene(path);
            return;
        }

        // Everything else is an asset the AssetEditorHost owns (.veasset kinds and
        // source images whose baked texture exists).
        m_assetEditors.openFile(path);
    }

    void EditorLayer::bootstrapProject()
    {
        m_document.bootstrap();
    }

    void EditorLayer::applyProject(const std::string& projectDir)
    {
        if (m_pendingLoad)
            return;

        if (!m_document.apply(projectDir))
            return;

        m_fileBrowser.setRootPath(toProjectAbsolute("content"));
        m_fileBrowser.setDefaultScenePath(m_document.project().defaultScene.empty()
            ? std::string() : toProjectAbsolute(m_document.project().defaultScene));

        // Save before dropping the binding: the camera sidecar hangs off the
        // absolute scene path, so this still lands next to the outgoing scene
        // even though the project root has already switched.
        m_cameraStore.save(m_sceneView, m_document.currentPath());

        // Start on the project's default scene, or a fresh empty one.
        const std::string& defaultScene = m_document.project().defaultScene;
        if (defaultScene.empty())
            finishSceneLoad(CreateRef<Scene>(), std::string(), std::filesystem::path());
        else
            beginSceneLoad(toProjectAbsolute(defaultScene),
                           std::filesystem::path(defaultScene).filename().string());

        VE_CORE_SUCCESS_PRINT("Project opened: %s (%s)", m_document.project().name.c_str(), projectDir.c_str());
    }

    void EditorLayer::beginSceneLoad(const std::filesystem::path& absolutePath, const std::string& name)
    {
        PendingSceneLoad task(CreateRef<Scene>(), name, absolutePath);
        if (!task.loader.beginLoad(absolutePath.string())) {
            VE_CORE_ERROR("Failed to load scene from: %s", absolutePath.string().c_str());
            finishSceneLoad(CreateRef<Scene>(), std::string(), std::filesystem::path());
            return;
        }
        m_pendingLoad = std::move(task);
    }

    void EditorLayer::stepSceneLoad()
    {
        if (!m_pendingLoad)
            return;

        PendingSceneLoad& task = *m_pendingLoad;
        const auto deadline = std::chrono::steady_clock::now() + kSceneLoadBudget;

        bool done = false;
        do {
            done = task.loader.loadChunk(kSceneLoadChunk);
        } while (!done && std::chrono::steady_clock::now() < deadline);

        if (!done)
            return;

        PendingSceneLoad completed = std::move(*m_pendingLoad);
        m_pendingLoad.reset();

        VE_CORE_SUCCESS_PRINT("Scene loaded from: %s", completed.path.string().c_str());
        finishSceneLoad(completed.loader.scene(), std::move(completed.name), std::move(completed.path));

        // Scene load is one of the two proactive preview-bake points (the other
        // is import), so the inspector/browser have thumbnails without a panel
        // having to trigger the first bake itself.
        ThumbnailRenderer::bakeAllLoaded();
    }

    void EditorLayer::finishSceneLoad(Ref<Scene> scene, std::string name, std::filesystem::path path)
    {
        m_document.setCurrent(std::move(name), std::move(path));
        bindEditorView(std::move(scene));
        m_selectedEntity = UINT32_MAX;
        m_cameraStore.restore(m_sceneView, m_document.currentPath());
        m_dialogs.setContext(m_document.currentName(), m_document.project());
    }

    void EditorLayer::renderSceneLoadOverlay()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.07f, 1.0f));

        ImGui::Begin("##SceneLoad", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        {
            const PendingSceneLoad& task = *m_pendingLoad;
            const size_t entityTotal = task.loader.entityCount();
            const float fraction = entityTotal > 0
                ? float(task.loader.loadedCount()) / float(entityTotal) : 1.0f;

            const ImVec2 size = ImGui::GetWindowSize();
            const float barWidth = std::min(420.0f, size.x - 40.0f);
            const float x = (size.x - barWidth) * 0.5f;

            ImGui::SetCursorPos(ImVec2(x, size.y * 0.45f));
            ImGui::Text("Opening %s", task.name.c_str());
            ImGui::SetCursorPosX(x);
            ImGui::ProgressBar(fraction, ImVec2(barWidth, 0.0f));
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void EditorLayer::onEntitySelected(uint32_t entityID) {
        m_selectedEntity = entityID;
    }

    void EditorLayer::paintTerrain() {

        auto& camera = m_sceneView.getCamera();
        const glm::vec2 viewportSize = Application::get().getGuiLayer()->getViewportBounds().second;

        m_terrainEditor.paintAt(camera.getViewMatrix(), camera.getProjectionMatrix(),
                                m_mouseViewportPos, viewportSize, *m_editorView.getScene());
    }

    void EditorLayer::startPlay() {
        if (m_playSession.isLive())
            return;

        // Gameplay module, if the project ships one. Absent is not an error: the
        // session still clones and ticks the scene, just with no gameplay code.
        const std::string dllPath = (getProjectRoot() / "Binaries" / "game.dll").string();

        if (!m_playSession.play(Application::get(), m_editorView.getScene(), dllPath))
            return;

        m_prePlayCameraSource = m_sceneView.getCameraSource();
        m_prePlayCameraEntity = m_sceneView.getSceneCameraEntity();

        // Editor-only interaction points at the authored scene; drop it and point
        // the viewport at the runtime duplicate.
        m_selectedEntity = UINT32_MAX;
        m_overlays.cancelPick();
        m_showTerrainBrush = false;
        m_terrainPainting = false;

        m_sceneView.setScene(m_playSession.runtimeScene());
        m_sceneView.setCameraSource(SceneView::CameraSource::Scene);
        m_editorView.setLocked(true);
    }

    void EditorLayer::stopPlay() {
        if (!m_playSession.isLive())
            return;

        m_playSession.stop();

        m_editorView.setLocked(false);
        m_sceneView.setScene(m_editorView.getScene());
        m_sceneView.setCameraSource(m_prePlayCameraSource);
        m_sceneView.setSceneCameraEntity(m_prePlayCameraEntity);
        m_selectedEntity = UINT32_MAX;
    }

    void EditorLayer::onUpdate() {

        // Run queued tool calls here, on the render thread, BEFORE any early-out
        // below. A submitter blocked on its future has no other way to progress.
        EditorMcpTools::pump();

        // A scene is being committed across frames: step it and leave the rest
        // of the editor frozen until it lands.
        if (m_pendingLoad) {
            stepSceneLoad();
            return;
        }

        // A tool may have created or rewritten an asset. The library counts those
        // writes, so a moved counter means the browser's listing is stale.
        if (AssetLibrary::get().revision() != m_seenAssetRevision) {
            m_seenAssetRevision = AssetLibrary::get().revision();
            m_fileBrowser.requestRefresh();
        }

        m_document.syncTerrainDataDirty(m_editorView.getScene());

        // Drive the runtime scene while a play session is live. Runs before the
        // view renders so this frame shows the tick's result.
        m_playSession.tick(DeltaTime::get().getDeltaTime());

        // Camera update + aspect/viewport + scene render, all owned by the view.
        // The fly-speed boost scales the speed for just this update, then the base
        // is put back -- so the controller always holds the un-boosted value the
        // slider edits, which is also what the camera store persists.
        auto& controller = m_sceneView.getCameraController();
        const bool boost = m_toolbar.isCameraBoostEnabled();
        const float baseSpeed = controller.getMoveSpeed();
        if (boost)
            controller.setMoveSpeed(baseSpeed * ViewportToolbar::kCameraBoostMultiplier);
        m_sceneView.onUpdate(DeltaTime::get().getDeltaTime());
        if (boost)
            controller.setMoveSpeed(baseSpeed);

        // Session-state camera autosave: throttled, and only while the mouse is
        // idle so an in-progress fly-around or drag does not hammer the disk.
        m_cameraStore.maybeAutosave(m_sceneView, m_document.currentPath(),
                                    ImGui::GetTime(), ImGui::IsAnyMouseDown());

        // Overlay: light + camera billboard icons (editor gizmos). Skipped while a
        // play session is live.
        m_overlays.renderBillboards(m_sceneView, m_editorView.getScene(), m_playSession.isLive());

        if (m_overlays.needsPick()) {
            const int32_t picked = m_overlays.performPick(m_sceneView, m_editorView.getScene());
            if (picked != -1)
                onEntitySelected((uint32_t)picked);
        }
    }

}
