#include "EditorLayer.h"

#include "EditorMcpTools.h"
#include "EditorPaths.h"

#include "imgui.h"
#include "imguizmo.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
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

        // Chart-space distance ahead of the eye that a new entity lands at when the
        // view axis misses the terrain.
        constexpr float kSpawnStandoff = 10.0f;

    }

    EditorLayer::EditorLayer()
        : m_editorView(CreateRef<Scene>(),
                       [this](AssetHandle handle) { m_assetEditors.openMaterial(handle); },
                       [this](AssetHandle handle) { m_assetEditors.openMaterialLayer(handle); },
                       [this](Scene& scene) { m_terrainEditor.drawInspector(m_showTerrainBrush, scene); },
                       [this]() { m_showTerrainMap = true; })
        , m_sceneView(m_editorView.getScene(), editorViewConfig())
    {
        m_editorView.setSpawnPlacement([this]() { return newEntityPlacement(); });

        m_toolbar.setCallbacks({
            [this]() { startPlay(m_toolbar.playerCount() > 1, m_toolbar.playerCount()); },
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
        // Root at the project folder so content/'s parent is reachable; open on
        // content/ so the panel looks unchanged until the user goes up.
        m_fileBrowser.setRootPath(getProjectRoot().string(), toProjectAbsolute("content"));
        m_fileBrowser.setOnFileSelect([this](const std::string& path) { openFile(path); });
        m_assetManager.setOnOpenAsset([this](const std::string& path) { openFile(toAbsolute(path)); });
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
                    if (m_showTerrainBrush || m_pie.isLive())
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
            m_toolbar.onGuiRender(m_sceneView, m_editorView.getScene(), m_pie);

            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();

            glm::vec2 pos(viewportPos.x, viewportPos.y);
            glm::vec2 size(viewportSize.x, viewportSize.y);
            Application::get().getGuiLayer()->setViewportBounds(pos, size);
            Application::get().getGuiLayer()->setViewportWindowHovered(ImGui::IsWindowHovered());
            // Camera input follows viewport hover, computed by the Gui layer. A
            // live PIE owns the viewport (its tiles read the keyboard), so the
            // editor camera stays parked.
            m_sceneView.setInputEnabled(!m_pie.isLive() && Application::get().isViewportHovered());

            if (m_pie.isLive()) {
                renderPieViewport(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);
            }
            else {
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
                if (!m_pie.isLive() && m_showTerrainBrush && mouseInViewport
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
                                            m_pie.isLive(), m_showTerrainBrush,
                                            m_terrainEditor, m_mouseViewportPos);
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::End();

        m_editorView.onGuiRender(m_selectedEntity);

        m_assetEditors.onGuiRender();
        m_jsonEditor.onGuiRender();

        if (m_showTerrainMap) {
            if (Ref<Scene> scene = m_editorView.getScene())
                m_terrainMap.onGuiRender(*scene, &m_showTerrainMap);
        }

        if (m_showAssetManager)
            m_assetManager.onGuiRender(&m_showAssetManager);

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
                ImGui::BeginDisabled(m_pie.isLive());

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

            if (ImGui::BeginMenu("Play"))
            {
                ImGui::PopFont();

                ImGui::BeginDisabled(m_pie.isLive());
                if (ImGui::MenuItem("Play (Offline)"))
                    startPlay(false, 1);
                if (ImGui::MenuItem("Play (2 Players)"))
                    startPlay(true, 2);
                if (ImGui::MenuItem("Play (3 Players)"))
                    startPlay(true, 3);
                if (ImGui::MenuItem("Play (4 Players)"))
                    startPlay(true, 4);
                ImGui::EndDisabled();

                ImGui::Separator();
                if (m_pie.isLive()) {
                    if (m_pie.state() == PieSession::State::Play) {
                        if (ImGui::MenuItem("Pause"))
                            m_pie.pause();
                    }
                    else if (ImGui::MenuItem("Resume")) {
                        m_pie.resume();
                    }
                    if (ImGui::MenuItem("Step"))
                        m_pie.stepOnce();
                    if (ImGui::MenuItem("Stop"))
                        stopPlay();
                }

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

                if (ImGui::MenuItem("Asset Manager")) {
                    m_assetManager.requestRefresh();
                    m_showAssetManager = true;
                }

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
        if (m_pie.isLive()) {
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

        if (m_pie.isLive()) {
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
        std::string ext = utils::getExtension(path);
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".veworld") {
            openScene(path);
            return;
        }

        // A plain JSON file is not an asset: the viewer/editor reads and writes
        // the file itself, with no AssetLibrary or ResourceManager entry.
        if (ext == ".json") {
            m_jsonEditor.openFile(path);
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

        m_fileBrowser.setRootPath(getProjectRoot().string(), toProjectAbsolute("content"));
        m_fileBrowser.setDefaultScenePath(m_document.project().defaultScene.empty()
            ? std::string() : toProjectAbsolute(m_document.project().defaultScene));
        m_assetManager.requestRefresh();

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

    ve::Vec3 EditorLayer::newEntityPlacement() {
        auto& camera = m_sceneView.getCamera();
        const glm::vec2 viewportSize = Application::get().getGuiLayer()->getViewportBounds().second;

        // Reuse the terrain brush's raycast: the point a stroke would land on is
        // also the point the user is looking at.
        glm::vec3 hit, normal;
        if (m_terrainEditor.hoverPoint(camera.getViewMatrix(), camera.getProjectionMatrix(),
                                       viewportSize * 0.5f, viewportSize,
                                       *m_editorView.getScene(), hit, normal))
            return ve::Vec3(hit);

        // Open sky, or a scene with no terrain: stand off the eye. The look axis
        // comes from the inverse view so a piloted scene camera works too -- its
        // yaw/pitch are stale, only the matrix is authoritative.
        const glm::vec3 front = -glm::vec3(glm::inverse(camera.getViewMatrix())[2]);
        return camera.getPosition() + ve::Vec3(front * kSpawnStandoff);
    }

    void EditorLayer::startPlay(bool listenServer, int players) {
        if (m_pie.isLive())
            return;

        PieConfig config;
        config.listenServer = listenServer;
        config.players = std::clamp(players, 1, 4);

        // The gameplay module is optional, exactly as in a module-less UE
        // project: offline play still works. A module that exists but fails to
        // load aborts the whole session instead of leaving a silent client.
        std::string modulePath = (getProjectRoot() / "Binaries" / "game.dll").string();
        std::error_code existsEc;
        if (!std::filesystem::exists(modulePath, existsEc)) {
            if (config.players > 1)
                VE_CORE_WARN_PRINT("%s", "PIE: no gameplay module, testing replication only");
            modulePath.clear();
        }

        SceneViewConfig viewConfig = editorViewConfig();
        const std::string& projectPipeline = m_document.project().pipeline;
        if (!projectPipeline.empty())
            viewConfig.pipelinePath = projectPipeline;

        // Seed the tiles with the camera the user is looking through, matching
        // UE: Play starts at the editor view, and only a scene camera takes over.
        if (!m_pie.start(Application::get(), m_editorView.getScene(), modulePath, config, viewConfig,
                         &m_sceneView.getCamera()))
            return;

        // Editor-only interaction points at the authored scene; the authored
        // view stays bound but is not drawn while the tiles are up.
        m_selectedEntity = UINT32_MAX;
        m_overlays.cancelPick();
        m_showTerrainBrush = false;
        m_terrainPainting = false;
        m_editorView.setLocked(true);
    }

    void EditorLayer::stopPlay() {
        if (!m_pie.isLive())
            return;

        m_pie.stop();

        m_editorView.setLocked(false);
        m_selectedEntity = UINT32_MAX;
    }

    void EditorLayer::renderPieViewport(float originX, float originY, float width, float height) {
        const size_t count = m_pie.worldCount();
        if (count == 0 || width <= 0.0f || height <= 0.0f)
            return;

        // One world fills the viewport; three become a column strip; anything
        // else is a 2-wide grid (2 -> one row, 4 -> two rows).
        const int cols = count == 1 ? 1 : (count == 3 ? 3 : 2);
        const int rows = static_cast<int>((count + cols - 1) / cols);
        const float gap = 2.0f;
        const float tileW = (width - gap * (cols - 1)) / static_cast<float>(cols);
        const float tileH = (height - gap * (rows - 1)) / static_cast<float>(rows);
        if (tileW <= 0.0f || tileH <= 0.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        for (size_t i = 0; i < count; ++i) {
            PieSession::World& world = m_pie.world(i);
            if (!world.view)
                continue;

            const int col = static_cast<int>(i) % cols;
            const int row = static_cast<int>(i) / cols;
            const ImVec2 tilePos(originX + col * (tileW + gap), originY + row * (tileH + gap));
            const ImVec2 tileSize(tileW, tileH);

            world.view->setSize(static_cast<uint32_t>(tileW), static_cast<uint32_t>(tileH));

            ImGui::SetCursorScreenPos(tilePos);
            ImGui::Image((void*)(intptr_t)world.view->getColorAttachmentId(), tileSize,
                         ImVec2(0, 1), ImVec2(1, 0));

            // Hover hands the keyboard to a tile: one keystroke must reach one
            // world, never every client at once.
            if (ImGui::IsItemHovered())
                m_pie.setFocusedWorld(i);

            const bool focused = m_pie.focusedWorld() == i;
            const ImU32 frameColor = focused ? IM_COL32(255, 180, 60, 255) : IM_COL32(0, 0, 0, 110);
            drawList->AddRect(tilePos, ImVec2(tilePos.x + tileW, tilePos.y + tileH), frameColor,
                              0.0f, 0, focused ? 2.0f : 1.0f);

            drawList->AddText(ImVec2(tilePos.x + 6, tilePos.y + 4),
                              IM_COL32(255, 255, 255, 230), world.label.c_str());

            if (NetSession* net = world.ctx->network()) {
                const NetStats& stats = net->stats();
                char line[160];
                std::snprintf(line, sizeof(line), "peers %zu  objs %llu  rpc %llu  err %llu",
                    net->peerCount(),
                    static_cast<unsigned long long>(stats.objectsApplied),
                    static_cast<unsigned long long>(stats.rpcsReceived),
                    static_cast<unsigned long long>(stats.parseErrors));
                drawList->AddText(ImVec2(tilePos.x + 6, tilePos.y + 22),
                                  IM_COL32(205, 225, 255, 210), line);
            }
        }
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
            m_assetManager.requestRefresh();
        }

        m_document.syncTerrainDataDirty(m_editorView.getScene());

        // Drive every PIE world. Runs before the tiles render so this frame shows
        // the tick's result.
        m_pie.update(DeltaTime::get().getDeltaTime());

        // While editing: particles preview on the authored scene. A live session
        // ticks its own cloned scenes, so nothing advances twice.
        if (!m_pie.isLive()) {
            if (Ref<Scene> scene = m_editorView.getScene())
                scene->tickParticles(DeltaTime::get().getDeltaTime());
        }

        // The authored view renders only while editing; its camera and the
        // camera sidecar are editor state, not part of the play session.
        if (m_pie.isLive())
            return;

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

        // Overlay: light + camera billboard icons (editor gizmos).
        m_overlays.renderBillboards(m_sceneView, m_editorView.getScene(), false);

        if (m_overlays.needsPick()) {
            const int32_t picked = m_overlays.performPick(m_sceneView, m_editorView.getScene());
            if (picked != -1)
                onEntitySelected((uint32_t)picked);
        }
    }

}
