#include "EditorLayer.h"

#include "EditorMcpTools.h"

#include "imgui.h"

#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "Core/ResourceManager.h"
#include "Core/AssetConfig.h"
#include "Asset/AssetLibrary.h"
#include "Asset/BuiltinReousrces.h"
#include "Core/Json.h"
#include "Asset/Utils.h"
#include "Renderer/Material.h"
#include "Renderer/LayeredMaterial.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/ThumbnailRenderer.h"
#include "Noise/NoiseGraphResource.h"

#include "AssetSaveRegistry.h"

#include "imguizmo.h"

namespace ve {

    namespace {

        // Session state, not document content: which scene / project the editor
        // had open. Lives under Saved/ so it can be gitignored.

        std::filesystem::path engineSavedDir() {
            return getAssetRoot() / "Saved";
        }

        std::filesystem::path projectSavedDir() {
            return getProjectRoot() / "Saved";
        }

        std::string readLine(const std::filesystem::path& file) {
            std::ifstream in(file);
            std::string line;
            std::getline(in, line);
            return line;
        }

        void writeLine(const std::filesystem::path& file, const std::string& text) {
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);
            std::ofstream out(file, std::ios::trunc);
            if (out.is_open())
                out << text << "\n";
        }

        std::string readLastSceneName() {
            return hasProjectRoot() ? readLine(projectSavedDir() / "last_scene.txt") : "";
        }

        void writeLastSceneName(const std::string& name) {
            if (hasProjectRoot())
                writeLine(projectSavedDir() / "last_scene.txt", name);
        }

        std::string readLastProjectDir() {
            return readLine(engineSavedDir() / "last_project.txt");
        }

        void writeLastProjectDir(const std::string& dir) {
            writeLine(engineSavedDir() / "last_project.txt", dir);
        }

        // Editor camera per scene, as a sidecar next to the scene file:
        // "terrain1.veworld" -> "terrain1.veworld.camera.json". Kept out of the
        // .veworld itself so moving the viewport never dirties the document, but
        // tied to the scene path rather than to a name-keyed bucket in Saved/, so
        // it follows the scene through renames, moves and copies. Only the control
        // state is stored (position + yaw/pitch), not the derived matrices, so it
        // round-trips through setYawPitch exactly.
        std::filesystem::path editorCameraFile(const std::filesystem::path& scenePath) {
            return std::filesystem::path(scenePath.string() + ".camera.json");
        }

        bool readEditorCamera(const std::filesystem::path& scenePath, Camera& cam, float& moveSpeed) {
            if (scenePath.empty())
                return false;
            const std::filesystem::path file = editorCameraFile(scenePath);
            // Absent is the normal case for a scene the editor has never shown,
            // so do not let JsonReader log it as an error.
            std::error_code ec;
            if (!std::filesystem::exists(file, ec))
                return false;

            JsonReader r;
            if (!JsonReader::load(file.string(), r) || !r.valid())
                return false;
            cam.setPosition(r.getVec3("position", cam.getPosition()));
            cam.setYawPitch(r.getFloat("yaw", cam.getYaw()), r.getFloat("pitch", cam.getPitch()));
            moveSpeed = r.getFloat("moveSpeed", moveSpeed);
            return true;
        }

        void writeEditorCamera(const std::filesystem::path& scenePath, const Camera& cam, float moveSpeed) {
            if (scenePath.empty())
                return;
            // JsonWriter's root is already an object; beginObject() with no key
            // appends to an enclosing array and throws here.
            JsonWriter w;
            w.set("position", cam.getPosition());
            w.set("yaw", cam.getYaw());
            w.set("pitch", cam.getPitch());
            w.set("moveSpeed", moveSpeed);
            w.writeToFile(editorCameraFile(scenePath).string());
        }

        // Project content root: scenes + imported assets, resolved against the
        // project root. Engine resources (shaders, meshes, icons) stay under the
        // engine root in SandBox/assets.
        std::filesystem::path scenesDir() {
            return std::filesystem::path(toProjectAbsolute("content/scenes"));
        }

        // Push the default font at 1.5x for the "File" menu label.
        // Matching PopFont() must follow. 1.92 bakes the size on demand.
        void pushFileMenuFont() {
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.5f);
        }

        std::string formatFileSize(uintmax_t bytes) {
            char buf[32];
            if (bytes >= 1024ull * 1024ull)
                std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
            else if (bytes >= 1024ull)
                std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
            else
                std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
            return buf;
        }

        // The texture .veasset baked for a source image: same stem, or stem with an
        // auto-suffix ("sofa_1"). "" when the source has not been imported.
        std::string findBakedTexture(const std::string& sourcePath) {
            const std::filesystem::path src(sourcePath);
            const std::string stem = src.stem().string();

            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(src.parent_path(), ec)) {
                if (!entry.is_regular_file(ec) || entry.path().extension() != ".veasset")
                    continue;
                if (utils::peekAssetToken(entry.path().string()) != "texture")
                    continue;

                const std::string cand = entry.path().stem().string();
                if (cand == stem || cand.rfind(stem + "_", 0) == 0)
                    return entry.path().string();
            }
            return "";
        }

        // Square PNG-icon toggle button for the viewport toolbar. Active state
        // inverts Unity-style: light background with a black-tinted icon;
        // inactive is the dark button with the icon at full colour.
        bool iconToggleButton(const char* id, bool& value, const char* tooltip, const Ref<Texture2D>& icon) {
            const float h = ImGui::GetFrameHeight();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton(id, ImVec2(h, h)))
                value = !value;
            const bool hovered = ImGui::IsItemHovered();
            ImU32 bg = value ? IM_COL32(215, 215, 215, 255)
                     : hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                               : ImGui::GetColorU32(ImGuiCol_Button);
            ImU32 tint = value ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, ImVec2(p0.x + h, p0.y + h), bg, 3.0f);
            if (icon && icon->getRendererID()) {
                // Icon covers the whole button face; the tint alone carries the
                // pressed state, and the background shows through any alpha.
                dl->AddImageRounded((ImTextureID)(uintptr_t)icon->getRendererID(),
                                    p0, ImVec2(p0.x + h, p0.y + h),
                                    ImVec2(0, 1), ImVec2(1, 0), tint, 3.0f);
            }
            if (hovered)
                ImGui::SetTooltip("%s", tooltip);
            return value;
        }

        // Project a world point into viewport pixel coordinates (y-down — the
        // space ImGui draw lists and m_mouseViewportPos both use). False when the
        // point is behind the camera.
        bool projectToViewport(const glm::mat4& viewProj, const glm::vec3& world,
                               const ImVec2& viewportPos, const ImVec2& viewportSize,
                               ImVec2& outScreen) {
            const glm::vec4 clip = viewProj * glm::vec4(world, 1.0f);
            if (clip.w <= 1e-5f)
                return false;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            outScreen.x = viewportPos.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x;
            outScreen.y = viewportPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.y;
            return true;
        }

    }

    EditorLayer::EditorLayer()
        : m_editorView(CreateRef<Scene>(),
                       [this](AssetHandle handle) { openMaterialEditor(handle); },
                       [this](AssetHandle handle) { openMaterialLayerEditor(handle); },
                       [this](Scene& scene) { m_terrainEditor.drawInspector(m_showTerrainBrush, scene); },
                       [this]() { m_showTerrainMap = true; })
    {
        // A layer stack opens the individual layer assets in their own window.
        m_layeredMaterialEditor.setOnOpenLayer(
            [this](AssetHandle handle) { openMaterialLayerEditor(handle); });
    }

    void EditorLayer::onAttach(){

        bootstrapProject();
        m_fileBrowser.setRootPath(toProjectAbsolute("content"));
        m_fileBrowser.setOnFileSelect([this](const std::string& path) { openFile(path); });
        // Prefills the save/load dialogs only; no scene is bound until one is
        // opened, so nothing camera-related keys off this.
        m_currentSceneName = readLastSceneName();

        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("SandBox/assets/shaders/Model.glsl");
        m_Shader = shaderLib.get("Model");

        shaderLib.load("SandBox/assets/shaders/Texture.glsl");
        m_TextureShader = shaderLib.get("Texture");

        shaderLib.load("SandBox/assets/shaders/Picking.glsl");
        m_pickingShader = shaderLib.get("Picking");

        RenderCommand::init();
        RenderCommand::setDepthTesting(true);
        RenderCommand::setCursorVisible(true);
        Renderer2D::init();
        Renderer3D::init();

        m_renderPipeline = CreateRef<RenderPipeline>();
        m_renderPipeline->init(1280, 720, "SandBox/assets/pipelines/default.json");

        m_cameraController.getCamera().setProjectionType(true);

        Application::get().getDispatcher()->subscribe(Event::Type::MouseScrolled,
            [this](Event& e) {
                if (Application::get().isViewportHovered()) {
                    m_cameraController.onEvent(e);
                }
            });

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
                // Right click to control the camera
                if (me.getButton() == VE_MOUSE_BUTTON_RIGHT && Application::get().isViewportHovered()) {
                    m_cameraController.setActive(true);
                    RenderCommand::setCursorVisible(false);
                }
                // Left click to pick entity in scene
                else if (me.getButton() == VE_MOUSE_BUTTON_LEFT && Application::get().isViewportHovered()) {

                    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())return;

                    // While the terrain brush is on, LMB paints instead of picking.
                    // The stroke itself is driven from the viewport's own mouse
                    // state below (see the ImGui brush block).
                    if (m_showTerrainBrush)
                        return;

                    float fboY = m_framebuffer->getHeight() - m_mouseViewportPos.y;
                    m_pickPos = { m_mouseViewportPos.x, fboY };
                    m_needsPicking = true;
                }

            });

        Application::get().getDispatcher()->subscribe(Event::Type::MouseButtonReleased,
            [this](Event& e) {
                auto& me = static_cast<MouseButtonReleasedEvent&>(e);
                if (me.getButton() == VE_MOUSE_BUTTON_RIGHT) {
                    m_cameraController.setActive(false);
                    RenderCommand::setCursorVisible(true);
                }
                else if (me.getButton() == VE_MOUSE_BUTTON_LEFT) {
                    m_terrainPainting = false;
                }
            });

        Application::get().getDispatcher()->subscribe(Event::Type::WindowResize, [this](Event& e) {
            auto& resizeEvent = static_cast<WindowResizeEvent&>(e);
            uint32_t width = static_cast<uint32_t>(resizeEvent.getWidth());
            uint32_t height = static_cast<uint32_t>(resizeEvent.getHeight());
            RenderCommand::setViewport(0, 0, width, height);
            m_cameraController.getCamera().setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            });

        // Load editor infrastructure assets (icons for light billboards, etc.)
        m_pointLightIcon = ResourceManager::store<Texture2D>("VostaEngine/resources/icons/point_light.png");
        m_wireframeIcon  = Texture2D::create("VostaEngine/resources/icons/cube_tri.png");
        m_groundGridIcon = Texture2D::create("VostaEngine/resources/icons/ground_grid.png");
        ve::BuiltinResources::getBuiltinSphere();
        ve::ResourceManager::store<ve::StaticMesh>("SandBox/assets/models/cube.obj");

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

    void EditorLayer::onImGuiRender() {
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

        // Size the menu-bar row for the larger "File" label, then restore the
        // normal font for the window's content.
        pushFileMenuFont();
        ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
        ImGui::PopFont();
        ImGui::PopStyleVar();

        renderMenuBar();

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
            // Viewport toolbar drawn 1.2x above the base UI scale.
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.2f);
            ImGui::PushItemWidth(240.0f);
            {
                // FPS overlay, frame-averaged over a 0.5s window (refresh each window).
                static float fpsTimer  = 0.0f;
                static int   fpsFrames = 0;
                static float fpsValue  = 0.0f;
                fpsTimer += DeltaTime::get().getDeltaTime();
                fpsFrames++;
                if (fpsTimer >= 0.5f) {
                    fpsValue = (float)fpsFrames / fpsTimer;
                    fpsTimer = 0.0f;
                    fpsFrames = 0;
                }
                ImGui::Text("FPS: %.0f", fpsValue);
                ImGui::SameLine();

                static float cameraMoveSpeed = 12.0f;
                ImGui::SliderFloat("Camera Speed", &cameraMoveSpeed, 0.1f, 64.0f);
                m_cameraController.setMoveSpeed(cameraMoveSpeed);
                ImGui::SameLine();
                static float farPlane = m_cameraController.getCamera().getFarPlane();
                float oldFarPlane = farPlane;
                ImGui::SliderFloat("View Distance", &farPlane, 0.1f, 29600.0f);
                if (farPlane != oldFarPlane) {
                    m_cameraController.getCamera().setFarPlane(farPlane);
                }
                ImGui::SameLine(0.0f, 12.0f);
                static bool wireframe = false;
                iconToggleButton("##wireframe", wireframe, "Wireframe", m_wireframeIcon);
                m_editorView.getViewRenderer()->setWireframe(wireframe);

                ImGui::SameLine(0.0f, 12.0f);
                static bool groundGrid = false;
                iconToggleButton("##groundGrid", groundGrid, "Ground Grid", m_groundGridIcon);
                m_editorView.getViewRenderer()->setGroundGrid(groundGrid);

                ImGui::SameLine(0.0f, 24.0f);
                static bool vsync = false;
                if (ImGui::Checkbox("VSync", &vsync))
                    Application::get().getWindow()->setVSync(vsync);
            }
            ImGui::PopItemWidth();
            ImGui::PopFont();

            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();

            glm::vec2 pos(viewportPos.x, viewportPos.y);
            glm::vec2 size(viewportSize.x, viewportSize.y);
            Application::get().getGuiLayer()->setViewportBounds(pos, size);
            Application::get().getGuiLayer()->setViewportWindowHovered(ImGui::IsWindowHovered());

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
            if (m_showTerrainBrush && mouseInViewport
                && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()
                && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                m_terrainPainting = true;
                paintTerrain();
            } else {
                m_terrainPainting = false;
            }

            if (viewportSize.x > 0 && viewportSize.y > 0) {
                if (!m_framebuffer || m_framebuffer->getWidth() != (uint32_t)viewportSize.x ||
                    m_framebuffer->getHeight() != (uint32_t)viewportSize.y) {
                    m_framebuffer = Framebuffer::create((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
                }
                if (!m_pickingFramebuffer || m_pickingFramebuffer->getWidth() != (uint32_t)viewportSize.x ||
                    m_pickingFramebuffer->getHeight() != (uint32_t)viewportSize.y) {
                    m_pickingFramebuffer = Framebuffer::create((uint32_t)viewportSize.x, (uint32_t)viewportSize.y,Framebuffer::Format::Picking);
                }

                ImGui::Image((void*)(intptr_t)m_framebuffer->getColorAttachmentRendererID(),
                    viewportSize, ImVec2(0, 1), ImVec2(1, 0));

                ImGuizmo::BeginFrame();
                if (m_selectedEntity != UINT32_MAX) {
                    Ref<Scene> scene = m_editorView.getScene();
                    auto entityID = m_selectedEntity;

                    // The selection is a raw id with no generation, and entities the
                    // editor/AI create need not carry a transform. Only drive the gizmo
                    // for a live entity that actually owns a TransformComponent.
                    if (scene->getEntity(entityID).getId() != 0xFFFFFFFFu &&
                        scene->getRegistry().has<TransformComponent>(entityID)) {
                        auto& tc = scene->getComponent<TransformComponent>(entityID);

                        ImGuizmo::SetOrthographic(false);
                        ImGuizmo::SetDrawlist();
                        ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

                        static ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                        if (ImGui::IsKeyPressed(ImGuiKey_W)) op = ImGuizmo::TRANSLATE;
                        if (ImGui::IsKeyPressed(ImGuiKey_E)) op = ImGuizmo::ROTATE;
                        if (ImGui::IsKeyPressed(ImGuiKey_R)) op = ImGuizmo::SCALE;

                        auto& camera = m_cameraController.getCamera();
                        glm::mat4 view = camera.getViewMatrix();
                        glm::mat4 proj = camera.getProjectionMatrix();
                        glm::mat4 transform = tc.transform;

                        ImGuizmo::Manipulate(
                            glm::value_ptr(view), glm::value_ptr(proj),
                            op, ImGuizmo::LOCAL,
                            glm::value_ptr(transform)
                        );

                        if (ImGuizmo::IsUsing()) {
                            tc.transform = transform;
                            // A terrain tile sits on an integer grid, so translation is
                            // snapped every frame of the drag rather than on release.
                            if (scene->getRegistry().has<TerrainComponent>(entityID))
                                Terrain::snapToGrid(*scene, entityID);
                        }
                    }
                }

                bool isOver = ImGuizmo::IsOver();

                // Terrain brush cursor: a ring on the surface where a stroke
                // would land. It lies in the tangent plane so it follows a slope
                // instead of reading as a flat top-down disc, and its radius
                // matches the world-space paint ball.
                if (m_showTerrainBrush && mouseInViewport) {
                    auto& camera = m_cameraController.getCamera();
                    glm::vec3 hit, hitNormal(0.0f, 1.0f, 0.0f);
                    if (m_terrainEditor.hoverPoint(camera.getViewMatrix(), camera.getProjectionMatrix(),
                            m_mouseViewportPos, size, *m_editorView.getScene(),
                            hit, hitNormal)) {
                        const glm::mat4 viewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
                        const float r = m_terrainEditor.radius();
                        const int segs = 72;

                        // Tangent basis around the normal; the fallback avoids a
                        // degenerate cross when the surface faces straight up.
                        const glm::vec3 up = std::abs(hitNormal.y) < 0.99f
                            ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                        const glm::vec3 T = glm::normalize(glm::cross(up, hitNormal));
                        const glm::vec3 B = glm::cross(hitNormal, T);

                        std::vector<ImVec2> ring;
                        ring.reserve(segs);
                        for (int i = 0; i < segs; ++i) {
                            const float a = (float)i / (float)segs * 6.2831853f;
                            const glm::vec3 p = hit + (std::cos(a) * T + std::sin(a) * B) * r;
                            ImVec2 s;
                            if (projectToViewport(viewProj, p, viewportPos, viewportSize, s))
                                ring.push_back(s);
                        }

                        if (ring.size() >= 2) {
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            dl->AddPolyline(ring.data(), (int)ring.size(),
                                            IM_COL32(0, 0, 0, 160), ImDrawFlags_Closed, 4.0f);
                            dl->AddPolyline(ring.data(), (int)ring.size(),
                                            IM_COL32(255, 255, 255, 230), ImDrawFlags_Closed, 2.0f);
                        }
                    }
                }

            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::End();

        m_editorView.onGuiRender(m_selectedEntity);

        // --- Material editor window ---
        if (m_showMaterialEditor && m_openMaterial) {
            m_materialGraphPanel.onGuiRender(m_openMaterial, m_needsMaterialRecompile,
                                             &m_showMaterialEditor, m_openMaterialOriginalHandle);
        }

        if (m_showNoiseEditor) {
            m_noisePanel.onGuiRender(&m_showNoiseEditor);
        }

        if (m_showMaterialLayerEditor) {
            m_materialLayerEditor.onGuiRender(&m_showMaterialLayerEditor);
        }

        if (m_showLayeredMaterialEditor) {
            m_layeredMaterialEditor.onGuiRender(&m_showLayeredMaterialEditor);
        }

        if (m_showTexturePreview) {
            m_texturePreview.onGuiRender(&m_showTexturePreview);
        }

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

    }

    void EditorLayer::openMaterialEditor(AssetHandle handle) {
        if (!handle.isValid()) return;

        Ref<Material> material = ResourceManager::get<Material>(handle);

        // A layered material has no node graph: the stack editor edits it instead.
        if (std::dynamic_pointer_cast<LayeredMaterial>(material)) {
            openLayeredMaterialEditor(handle);
            return;
        }

        // Otherwise the graph editor only edits the single-surface kind.
        m_openMaterial = std::dynamic_pointer_cast<SingleMaterial>(material);
        if (!m_openMaterial) return;

        m_openMaterialOriginalHandle = handle;
        m_showMaterialEditor = true;
    }

    void EditorLayer::openLayeredMaterialEditor(AssetHandle handle) {
        if (!handle.isValid()) return;
        m_layeredMaterialEditor.openHandle(handle);
        m_showLayeredMaterialEditor = true;
    }

    void EditorLayer::openMaterialLayerEditor(AssetHandle handle) {
        if (!handle.isValid()) return;

        const std::string rel = ResourceManager::getPath<MaterialLayerAsset>(handle);
        if (rel.empty()) return;
        m_materialLayerEditor.openFile(toAbsolute(rel));
        m_showMaterialLayerEditor = true;
    }

    void EditorLayer::refreshSceneFileList()
    {
        m_sceneFiles.clear();

        const std::filesystem::path dir = scenesDir();
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec))
            return;

        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec))
                continue;
            if (entry.path().extension() != ".veworld")
                continue;

            SceneFile file;
            file.name = entry.path().filename().string();
            file.size = entry.file_size(ec);
            m_sceneFiles.push_back(file);
        }

        std::sort(m_sceneFiles.begin(), m_sceneFiles.end(),
            [](const SceneFile& a, const SceneFile& b) { return a.name < b.name; });
    }

    void EditorLayer::refreshProjectList()
    {
        m_projectDirs.clear();

        std::error_code ec;
        const std::filesystem::path root = Project::projectsDir();
        if (std::filesystem::is_directory(root, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
                if (entry.is_directory(ec))
                    m_projectDirs.push_back(entry.path().string());
            }
        }

        // The built-in SandBox project lives at the engine root, not under
        // Projects/, but is still openable.
        const std::filesystem::path sandbox = getAssetRoot() / "SandBox";
        if (std::filesystem::is_directory(sandbox, ec))
            m_projectDirs.push_back(sandbox.string());

        std::sort(m_projectDirs.begin(), m_projectDirs.end());
    }

    void EditorLayer::renderMenuBar()
    {
        // "File" is drawn larger; its dropdown items stay at the normal size.
        pushFileMenuFont();

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::PopFont();

                if (ImGui::MenuItem("New Project..."))
                {
                    m_showNewProjectPopup = true;
                }

                if (ImGui::MenuItem("Open Project..."))
                {
                    refreshProjectList();
                    m_showOpenProjectPopup = true;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    if (!m_currentSceneName.empty())
                        std::snprintf(m_saveFileNameBuffer, sizeof(m_saveFileNameBuffer), "%s", m_currentSceneName.c_str());
                    m_showSavePopup = true;
                }

                if (ImGui::MenuItem("Load", "Ctrl+O"))
                {
                    if (!m_currentSceneName.empty())
                        std::snprintf(m_loadFileNameBuffer, sizeof(m_loadFileNameBuffer), "%s", m_currentSceneName.c_str());
                    refreshSceneFileList();
                    m_showLoadPopup = true;
                }

                ImGui::Separator();

                //if (ImGui::MenuItem("Exit", "Alt+F4"))
                //{
                //    Application::get().close();
                //}

                pushFileMenuFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                ImGui::PopFont();

                if (ImGui::MenuItem("Noise Editor"))
                {
                    m_showNoiseEditor = true;
                }

                if (ImGui::MenuItem("Terrain Map"))
                {
                    m_showTerrainMap = true;
                }

                if (ImGui::MenuItem("AI Assistant"))
                {
                    m_showAi = true;
                }

                pushFileMenuFont();
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::PopFont();

        // Save popup
        if (m_showSavePopup)
        {
            ImGui::OpenPopup("Save Scene");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Save Scene", &m_showSavePopup))
            {
                ImGui::InputText("Filename", m_saveFileNameBuffer, sizeof(m_saveFileNameBuffer));

                if (ImGui::Button("Save"))
                {
                    m_saveFileName = m_saveFileNameBuffer;
                    saveScene();
                    m_showSavePopup = false;
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    m_showSavePopup = false;
                }

                ImGui::EndPopup();
            }
        }

        // Load popup
        if (m_showLoadPopup)
        {
            ImGui::OpenPopup("Load Scene");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Load Scene", &m_showLoadPopup))
            {
                ImGui::TextDisabled("%s", toProjectAbsolute("content/scenes").c_str());
                ImGui::Separator();

                if (m_sceneFiles.empty())
                {
                    ImGui::TextDisabled("No .veworld files found");
                }
                else
                {
                    // Size the list to its contents so no row is clipped away, but
                    // cap it so a large folder still scrolls instead of filling the screen.
                    const float listHeight = std::min(
                        ImGui::GetFrameHeightWithSpacing() * static_cast<float>(m_sceneFiles.size())
                            + ImGui::GetStyle().WindowPadding.y * 2.0f,
                        ImGui::GetMainViewport()->WorkSize.y * 0.6f);

                    ImGui::BeginChild("##sceneList", ImVec2(460.0f, listHeight), true);
                    ImGui::Columns(2, "##sceneColumns", false);
                    ImGui::SetColumnWidth(0, 330.0f);

                    for (const auto& file : m_sceneFiles)
                    {
                        const bool selected = (m_loadFileName == file.name);

                        // SpanAllColumns so the whole row is clickable, not just the name.
                        if (ImGui::Selectable(file.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            m_loadFileName = file.name;
                            std::snprintf(m_loadFileNameBuffer, sizeof(m_loadFileNameBuffer), "%s", file.name.c_str());
                        }

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            m_loadFileName = file.name;
                            loadScene();
                            m_showLoadPopup = false;
                        }

                        ImGui::NextColumn();
                        ImGui::TextDisabled("%s", formatFileSize(file.size).c_str());
                        ImGui::NextColumn();
                    }

                    ImGui::Columns(1);
                    ImGui::EndChild();
                }

                ImGui::InputText("Filename", m_loadFileNameBuffer, sizeof(m_loadFileNameBuffer));

                if (ImGui::Button("Load"))
                {
                    m_loadFileName = m_loadFileNameBuffer;
                    loadScene();
                    m_showLoadPopup = false;
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    m_showLoadPopup = false;
                }

                ImGui::EndPopup();
            }
        }

        // New Project popup
        if (m_showNewProjectPopup)
        {
            ImGui::OpenPopup("New Project");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("New Project", &m_showNewProjectPopup))
            {
                ImGui::InputText("Name", m_newProjectNameBuffer, sizeof(m_newProjectNameBuffer));
                ImGui::TextDisabled("Location: %s", Project::projectsDir().c_str());

                if (ImGui::Button("Create"))
                {
                    ProjectInfo info;
                    const std::string dir = (std::filesystem::path(Project::projectsDir())
                                             / m_newProjectNameBuffer).string();
                    if (Project::create(m_newProjectNameBuffer, info))
                    {
                        applyProject(dir);
                        m_showNewProjectPopup = false;
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    m_showNewProjectPopup = false;
                }

                ImGui::EndPopup();
            }
        }

        // Open Project popup
        if (m_showOpenProjectPopup)
        {
            ImGui::OpenPopup("Open Project");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Open Project", &m_showOpenProjectPopup))
            {
                ImGui::TextDisabled("%s", Project::projectsDir().c_str());
                ImGui::Separator();

                if (m_projectDirs.empty())
                {
                    ImGui::TextDisabled("No projects found");
                }
                else
                {
                    const float listHeight = std::min(
                        ImGui::GetFrameHeightWithSpacing() * static_cast<float>(m_projectDirs.size())
                            + ImGui::GetStyle().WindowPadding.y * 2.0f,
                        ImGui::GetMainViewport()->WorkSize.y * 0.6f);

                    ImGui::BeginChild("##projectList", ImVec2(360.0f, listHeight), true);
                    for (const auto& dir : m_projectDirs)
                    {
                        const std::string name = std::filesystem::path(dir).filename().string();
                        const bool selected = (m_openProjectDir == dir);

                        if (ImGui::Selectable(name.c_str(), selected))
                            m_openProjectDir = dir;

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            m_openProjectDir = dir;
                            applyProject(dir);
                            m_showOpenProjectPopup = false;
                        }
                    }
                    ImGui::EndChild();
                }

                if (ImGui::Button("Open"))
                {
                    if (!m_openProjectDir.empty())
                    {
                        applyProject(m_openProjectDir);
                        m_showOpenProjectPopup = false;
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    m_showOpenProjectPopup = false;
                }

                ImGui::EndPopup();
            }
        }
    }

    void EditorLayer::restoreCameraFor(const std::filesystem::path& scenePath)
    {
        auto& cam = m_cameraController.getCamera();
        float speed = m_cameraController.getMoveSpeed();
        readEditorCamera(scenePath, cam, speed);
        m_cameraController.setMoveSpeed(speed);

        // Remember what is on disk so the idle save below does not immediately
        // rewrite an unchanged camera.
        m_savedCamPos = cam.getPosition();
        m_savedCamYaw = cam.getYaw();
        m_savedCamPitch = cam.getPitch();
    }

    bool EditorLayer::saveCurrentCamera(bool force)
    {
        auto& cam = m_cameraController.getCamera();
        if (!force && cam.getPosition() == m_savedCamPos
            && cam.getYaw() == m_savedCamYaw && cam.getPitch() == m_savedCamPitch)
            return false;

        writeEditorCamera(m_currentScenePath, cam, m_cameraController.getMoveSpeed());
        m_savedCamPos = cam.getPosition();
        m_savedCamYaw = cam.getYaw();
        m_savedCamPitch = cam.getPitch();
        return true;
    }

    Ref<Scene> EditorLayer::currentScene() {
        return m_editorView.getScene();
    }

    const std::filesystem::path& EditorLayer::currentScenePath() const {
        return m_currentScenePath;
    }

    bool EditorLayer::saveScene()
    {
        if (m_saveFileName.empty())
        {
            VE_CORE_ERROR("No filename specified for saving!");
            return false;
        }

        if (m_saveFileName.find(".veworld") == std::string::npos)
        {
            m_saveFileName += ".veworld";
        }

        std::filesystem::path absolutePath = scenesDir() / m_saveFileName;
        std::filesystem::create_directories(absolutePath.parent_path());

        // Before the scene is written, so its JSON carries the terrain-data link.
        writeTerrainDataFor(absolutePath);

        SceneSerializer serializer(m_editorView.getScene());
        if (!serializer.saveToFile(absolutePath.string())) {
            VE_CORE_ERROR("Failed to save scene to: %s", absolutePath.string().c_str());
            return false;
        }

        m_currentSceneName = m_saveFileName;
        m_currentScenePath = absolutePath;
        std::snprintf(m_saveFileNameBuffer, sizeof(m_saveFileNameBuffer), "%s", m_currentSceneName.c_str());
        writeLastSceneName(m_currentSceneName);

        // Forced: a Save As renames the file while the camera is unchanged, so
        // the change-detect would otherwise skip the write for the new name.
        saveCurrentCamera(true);

        VE_CORE_SUCCESS_PRINT("Scene saved to: %s", absolutePath.string().c_str());
        return true;
    }

    std::string EditorLayer::terrainDataPathFor(const std::filesystem::path& scenePath) {
        return (scenePath.parent_path() / (scenePath.stem().string() + "_terrain.veasset")).string();
    }

    void EditorLayer::writeTerrainDataFor(const std::filesystem::path& scenePath) {

        Ref<Scene> scene = m_editorView.getScene();
        if (!scene)
            return;
        TerrainSystemComponent* sys = Terrain::findSystem(*scene);
        if (!sys)
            return;

        Ref<TerrainDataResource> data = Terrain::ensureTerrainData(*scene);
        if (!data)
            return;

        // A terrain that was never painted leaves no file and no reference.
        const std::string path = terrainDataPathFor(scenePath);
        if (!data->anyPainted() && !sys->terrainDataHandle.isValid())
            return;

        auto& storage = ResourceManager::getStorage<TerrainDataResource>();
        const std::string rel = toRelative(path);
        if (sys->terrainDataHandle.isValid()
            && ResourceManager::getPath<TerrainDataResource>(sys->terrainDataHandle) != rel) {
            // Save As renamed the scene; drop the stale registration.
            ResourceManager::remove<TerrainDataResource>(sys->terrainDataHandle);
        }
        sys->terrainDataHandle = storage.store(rel, data);

        data->write(path);
        data->bDirty = false;
        AssetSaveRegistry::get().markClean(path);
    }

    void EditorLayer::syncTerrainDataDirty() {

        Ref<Scene> scene = m_editorView.getScene();
        if (!scene)
            return;
        TerrainSystemComponent* sys = Terrain::findSystem(*scene);
        if (!sys || !sys->terrainDataHandle.isValid())
            return;

        Ref<TerrainDataResource> data = Terrain::findTerrainData(*scene);
        if (!data || !data->bDirty)
            return;

        const std::string path = ResourceManager::getPath<TerrainDataResource>(sys->terrainDataHandle);
        AssetSaveRegistry::get().markDirty(path, [path, data] {
            data->write(toAbsolute(path));
            data->bDirty = false;
        });
    }

    void EditorLayer::loadScene()
    {
        if (m_loadFileName.empty())
        {
            VE_CORE_ERROR("No filename specified for loading!");
            return;
        }

        if (m_loadFileName.find(".veworld") == std::string::npos)
        {
            m_loadFileName += ".veworld";
        }

        openScene(scenesDir() / m_loadFileName);
    }

    bool EditorLayer::openScene(const std::filesystem::path& absolutePath)
    {
        auto newScene = CreateRef<Scene>();

        SceneSerializer loader(newScene);
        if (!loader.loadFromFile(absolutePath.string())) {
            VE_CORE_ERROR("Failed to load scene from: %s", absolutePath.string().c_str());
            return false;
        }

        m_editorView.rebind(newScene,
            [this](AssetHandle handle) { openMaterialEditor(handle); },
            [this](AssetHandle handle) { openMaterialLayerEditor(handle); },
            [this](Scene& scene) { m_terrainEditor.drawInspector(m_showTerrainBrush, scene); },
            [this]() { m_showTerrainMap = true; });
        m_selectedEntity = UINT32_MAX;

        // Persist the outgoing scene's camera before the name flips, then load
        // the incoming scene's saved view.
        saveCurrentCamera();

        m_currentSceneName = absolutePath.filename().string();
        m_currentScenePath = absolutePath;
        std::snprintf(m_loadFileNameBuffer, sizeof(m_loadFileNameBuffer), "%s", m_currentSceneName.c_str());
        writeLastSceneName(m_currentSceneName);
        restoreCameraFor(m_currentScenePath);

        // Scene load is one of the two proactive preview-bake points (the other
        // is import), so the inspector/browser have thumbnails without a panel
        // having to trigger the first bake itself.
        ThumbnailRenderer::bakeAllLoaded();

        VE_CORE_SUCCESS_PRINT("Scene loaded from: %s", absolutePath.string().c_str());
        return true;
    }

    void EditorLayer::openFile(const std::string& path)
    {
        const std::string ext = utils::getExtension(path);

        if (ext == ".veworld") {
            openScene(path);
            return;
        }

        if (ext == ".veasset" && utils::peekAssetToken(path) == "material") {
            AssetHandle handle = ResourceManager::find<Material>(path);
            if (!handle.isValid())
                handle = ResourceManager::store<Material>(path);
            openMaterialEditor(handle);
            return;
        }

        if (ext == ".veasset" && utils::peekAssetToken(path) == "layered_material") {
            // No node graph: the layer stack gets its own editor window, so a
            // layered material is opened the same way a single-surface one is.
            AssetHandle handle = ResourceManager::find<Material>(path);
            if (!handle.isValid())
                handle = ResourceManager::store<Material>(path);
            openLayeredMaterialEditor(handle);
            return;
        }

        if (ext == ".veasset" && utils::peekAssetToken(path) == "material_layer") {
            // A shared material layer: register it so material inspectors can list
            // it, then open it for editing.
            if (!ResourceManager::find<MaterialLayerAsset>(path).isValid())
                ResourceManager::store<MaterialLayerAsset>(path);
            m_materialLayerEditor.openFile(path);
            m_showMaterialLayerEditor = true;
            return;
        }

        if (ext == ".veasset" && utils::peekAssetToken(path) == "noise") {
            // Register with ResourceManager so the terrain's Noise Graph combo can
            // list it; NoisePanel keeps its own copy for editing.
            if (!ResourceManager::find<NoiseGraphResource>(path).isValid())
                ResourceManager::store<NoiseGraphResource>(path);
            m_noisePanel.openFile(path);
            m_showNoiseEditor = true;
            return;
        }

        if (ext == ".veasset" && utils::peekAssetToken(path) == "texture") {
            openTexturePreview(path);
            return;
        }

        // Double-clicking a source image previews its baked texture, so the
        // browser's image tile works as well as the .veasset tile.
        if (ext == ".png" || ext == ".jpg") {
            const std::string baked = findBakedTexture(path);
            if (baked.empty())
                VE_CORE_WARN_PRINT("No imported texture for %s", path.c_str());
            else
                openTexturePreview(baked);
        }
    }

    void EditorLayer::openTexturePreview(const std::string& path) {
        AssetHandle handle = ResourceManager::find<Texture2D>(path);
        if (!handle.isValid())
            handle = ResourceManager::store<Texture2D>(path);

        auto tex = ResourceManager::get<Texture2D>(handle);
        if (!tex)
            return;

        m_texturePreview.open(tex, std::filesystem::path(path).filename().string());
        m_showTexturePreview = true;
    }

    void EditorLayer::bootstrapProject()
    {
        // Reopen the last project; fall back to the built-in SandBox project.
        const std::string last = readLastProjectDir();
        if (!last.empty() && Project::open(last, m_project))
        {
            setProjectRoot(last);
            return;
        }

        const std::string sandbox = (getAssetRoot() / "SandBox").string();
        Project::open(sandbox, m_project);
        setProjectRoot(sandbox);
        writeLastProjectDir(sandbox);
    }

    void EditorLayer::applyProject(const std::string& projectDir)
    {
        if (!Project::open(projectDir, m_project))
            return;

        setProjectRoot(projectDir);
        writeLastProjectDir(projectDir);
        m_fileBrowser.setRootPath(toProjectAbsolute("content"));

        // Start on the project's default scene, or a fresh empty one.
        auto scene = CreateRef<Scene>();
        // Save before dropping the binding: the camera sidecar hangs off the
        // absolute scene path, so this still lands next to the outgoing scene
        // even though the project root has already switched.
        saveCurrentCamera();
        m_currentSceneName.clear();
        m_currentScenePath.clear();

        if (!m_project.defaultScene.empty())
        {
            SceneSerializer loader(scene);
            if (loader.loadFromFile(toProjectAbsolute(m_project.defaultScene)))
            {
                m_currentSceneName = std::filesystem::path(m_project.defaultScene).filename().string();
                m_currentScenePath = toProjectAbsolute(m_project.defaultScene);
                ThumbnailRenderer::bakeAllLoaded();
            }
        }

        m_editorView.rebind(scene,
            [this](AssetHandle handle) { openMaterialEditor(handle); },
            [this](AssetHandle handle) { openMaterialLayerEditor(handle); },
            [this](Scene& scene) { m_terrainEditor.drawInspector(m_showTerrainBrush, scene); },
            [this]() { m_showTerrainMap = true; });
        m_selectedEntity = UINT32_MAX;
        writeLastSceneName(m_currentSceneName);
        restoreCameraFor(m_currentScenePath);

        std::snprintf(m_saveFileNameBuffer, sizeof(m_saveFileNameBuffer), "%s",
                      m_currentSceneName.empty() ? "scene.veworld" : m_currentSceneName.c_str());

        VE_CORE_SUCCESS_PRINT("Project opened: %s (%s)", m_project.name.c_str(), projectDir.c_str());
    }

    void EditorLayer::onEntitySelected(uint32_t entityID){

        m_selectedEntity = entityID;
    }

    void EditorLayer::performPicking() {
        m_pickingFramebuffer->bind();
        // The picking target is R32I; -1 means "nothing" so entity id 0 is not
        // ambiguous with cleared background.
        RenderCommand::clearInt(-1);

        Renderer2D::beginPickingScene(m_cameraController.getCamera().getViewProjectionMatrix(), m_pickingShader);
        Renderer3D::beginPickingScene(m_cameraController.getCamera().getViewProjectionMatrix());

        m_editorView.getScene()->onPickingRender();

        // Pick light billboards
        {
            auto lightView = m_editorView.getScene()->getRegistry().view<LightComponent>();
            if (!lightView.empty() && m_pointLightIcon.isValid()) {
                const auto& viewMatrix = m_cameraController.getCamera().getViewMatrix();
                const auto& cameraPos = m_cameraController.getCamera().getPosition();

                for (auto entity : lightView) {
                    auto& transform = m_editorView.getScene()->getComponent<TransformComponent>(entity);
                    glm::vec3 lightPos = glm::vec3(transform.transform[3]);

                    float dist = glm::length(lightPos - cameraPos);
                    float scale = glm::clamp(dist * 0.08f, 0.3f, 3.0f) * m_lightIconSize;

                    glm::mat4 billboardMat = makeBillboard(lightPos, viewMatrix);
                    Renderer2D::drawPickingQuad(billboardMat, glm::vec2(scale), entity);
                }
            }
        }

        Renderer2D::endPickingScene();
        Renderer3D::endPickingScene();

        int32_t entityId = m_pickingFramebuffer->readPixel((uint32_t)m_pickPos.x, (uint32_t)m_pickPos.y);

        // VE_CORE_SUCCESS_PRINT("Pick pos: (%d, %d), Raw read: %d", (int)m_pickPos.x, (int)m_pickPos.y, entityId);
        m_pickingFramebuffer->unbind();

        if (entityId != -1) {
            onEntitySelected(entityId);
        }

    }

    void EditorLayer::paintTerrain() {

        auto& camera = m_cameraController.getCamera();
        const glm::vec2 viewportSize = Application::get().getGuiLayer()->getViewportBounds().second;

        m_terrainEditor.paintAt(camera.getViewMatrix(), camera.getProjectionMatrix(),
                                m_mouseViewportPos, viewportSize, *m_editorView.getScene());
    }

    void EditorLayer::onUpdate() {

        // Run queued tool calls here, on the render thread, BEFORE any early-out
        // below. A submitter blocked on its future has no other way to progress.
        EditorMcpTools::pump();

        // A tool may have created or rewritten an asset. The library counts those
        // writes, so a moved counter means the browser's listing is stale.
        if (AssetLibrary::get().revision() != m_seenAssetRevision) {
            m_seenAssetRevision = AssetLibrary::get().revision();
            m_fileBrowser.requestRefresh();
        }

        syncTerrainDataDirty();

        uint32_t fbW = 0, fbH = 0;
        if (!m_framebuffer) return;

        fbW = m_framebuffer->getWidth();
        fbH = m_framebuffer->getHeight();

        m_cameraController.onUpdate(DeltaTime::get().getDeltaTime());
        m_cameraController.getCamera().setAspectRatio((float)fbW / (float)fbH);
        m_cameraController.getCamera().setViewportSize(fbW, fbH);

        // Session-state camera autosave: throttled, and only while the mouse is
        // idle so an in-progress fly-around or drag does not hammer the disk.
        // Gated on a bound scene: with none open the viewport is a scratch pad,
        // and its camera must not be written over some other scene's saved view.
        if (!m_currentScenePath.empty() && !ImGui::IsAnyMouseDown()
            && ImGui::GetTime() - m_lastCameraSaveTime > 0.5)
        {
            m_lastCameraSaveTime = ImGui::GetTime();
            saveCurrentCamera();
        }

        if (m_renderPipeline) {
            m_editorView.getViewRenderer()->render(
                m_cameraController.getCamera(), m_renderPipeline, m_framebuffer);

            // Overlay: light billboard icons (editor gizmo)
            renderLightBillboards(m_cameraController.getCamera().getViewMatrix(),
                                  m_cameraController.getCamera().getProjectionMatrix(),
                                  m_cameraController.getCamera().getPosition());
        }

        if (m_needsPicking && m_pickingFramebuffer) {
            performPicking();
            m_needsPicking = false;
        }
    }

    // =================================================================
    // Billboard helpers
    // =================================================================

    glm::mat4 EditorLayer::makeBillboard(const glm::vec3& position, const glm::mat4& viewMatrix) {

        glm::vec3 cameraPos = glm::vec3(glm::inverse(viewMatrix)[3]);

        glm::vec3 forward = glm::normalize(cameraPos - position);

        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
        glm::vec3 up = glm::normalize(glm::cross(forward, right));

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right, 0.0f);
        billboard[1] = glm::vec4(up, 0.0f);
        billboard[2] = glm::vec4(forward, 0.0f);
        billboard[3] = glm::vec4(position, 1.0f);

        return billboard;
    }

    void EditorLayer::renderLightBillboards(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, const glm::vec3& cameraPos) {
        auto lightView = m_editorView.getScene()->getRegistry().view<LightComponent>();
        if (lightView.empty() || !m_pointLightIcon.isValid()) return;

        glm::mat4 vp = projMatrix * viewMatrix;

        Renderer2D::setBlending(true);
        RenderCommand::setDepthTesting(false);
        Renderer2D::beginScene(vp);

        for (auto entity : lightView) {
            auto& transform = m_editorView.getScene()->getComponent<TransformComponent>(entity);
            glm::vec3 lightPos = glm::vec3(transform.transform[3]);

            float dist = glm::length(lightPos - cameraPos);
            float scale = glm::clamp(dist * 0.2f, 0.3f, 3.0f) * m_lightIconSize;

            glm::mat4 billboardMat = makeBillboard(lightPos, viewMatrix);
            Renderer2D::drawQuad(billboardMat, glm::vec2(scale), m_pointLightIcon);
        }

        Renderer2D::endScene();
        RenderCommand::setDepthTesting(true);
        Renderer2D::setBlending(false);
    }


}
