#include "EditorLayer.h"

#include "imgui.h"

#include <filesystem>
#include <cstring>

#include "Core/ResourceManager.h"
#include "Core/AssetConfig.h"

#include "Scene/Components.h"
#include "Renderer/Material.h"
#include "Renderer/Preprocess/IBLBaker.h"
#include "Renderer/Preprocess/WorleyNoiseBaker.h"
#include "Renderer/Preprocess/AtmosphereBaker.h"
#include "Renderer/Preprocess/WeatherMapBaker.h"
#include "imguizmo.h"

namespace ve {

    EditorLayer::EditorLayer()
        : m_editorView(CreateRef<Scene>(),
                       [this](AssetHandle handle) { openMaterialEditor(handle); })
    {
        m_fileBrowser.setRootPath(toAbsolute("SandBox"));
    }

    void EditorLayer::onAttach(){

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
        m_renderPipeline->init(1280, 720);

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
        ve::ResourceManager::store<ve::StaticMesh>("SandBox/assets/models/sphere.obj");
        ve::ResourceManager::store<ve::StaticMesh>("SandBox/assets/models/cube.obj");

        // Bake BRDF LUT (scene-independent, needed by PBR pipeline)
        m_brdfLUT = IBLBaker::bakeBRDFLUT();
        if (!m_brdfLUT) {
            VE_CORE_ERROR_PRINT("%s", "Editor: BRDF LUT bake failed, specular IBL will be missing");
        }
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

        ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
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
            ImGui::PushItemWidth(200.0f);
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
            }
            ImGui::PopItemWidth();

            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();

            glm::vec2 pos(viewportPos.x, viewportPos.y);
            glm::vec2 size(viewportSize.x, viewportSize.y);
            Application::get().getGuiLayer()->setViewportBounds(pos, size);
            Application::get().getGuiLayer()->setViewportWindowHovered(ImGui::IsWindowHovered());

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
                    auto entityID = m_selectedEntity;
                    auto& tc = m_editorView.getScene()->getComponent<TransformComponent>(entityID);

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
                    }
                }

                bool isOver = ImGuizmo::IsOver();

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

    }

    void EditorLayer::openMaterialEditor(AssetHandle handle) {
        if (!handle.isValid()) return;

        m_openMaterial = ResourceManager::get<Material>(handle);
        if (!m_openMaterial) return;

        m_openMaterialOriginalHandle = handle;
        m_showMaterialEditor = true;
    }

    void EditorLayer::renderMenuBar()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    m_showSavePopup = true;
                }

                if (ImGui::MenuItem("Load", "Ctrl+O"))
                {
                    m_showLoadPopup = true;
                }

                ImGui::Separator();

                //if (ImGui::MenuItem("Exit", "Alt+F4"))
                //{
                //    Application::get().close();
                //}

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Save popup
        if (m_showSavePopup)
        {
            ImGui::OpenPopup("Save Scene");
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
            if (ImGui::BeginPopupModal("Load Scene", &m_showLoadPopup))
            {
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
    }

    void EditorLayer::saveScene()
    {
        if (m_saveFileName.empty())
        {
            VE_CORE_ERROR("No filename specified for saving!");
            return;
        }

        if (m_saveFileName.find(".veworld") == std::string::npos)
        {
            m_saveFileName += ".veworld";
        }

        std::string relativeScenePath = std::string("SandBox/assets/scenes/") + m_saveFileName;
        std::filesystem::path absolutePath = toAbsolute(relativeScenePath);
        std::filesystem::create_directories(absolutePath.parent_path());

        SceneSerializer serializer(m_editorView.getScene());
        if (!serializer.saveToFile(absolutePath.string())) {
            VE_CORE_ERROR("Failed to save scene to: %s", absolutePath.string().c_str());
            return;
        }

        VE_CORE_SUCCESS_PRINT("Scene saved to: %s", absolutePath.string().c_str());
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

        std::string relativeScenePath = std::string("SandBox/assets/scenes/") + m_loadFileName;
        std::filesystem::path absolutePath = toAbsolute(relativeScenePath);

        auto newScene = CreateRef<Scene>();

        SceneSerializer loader(newScene);
        if (!loader.loadFromFile(absolutePath.string())) {
            VE_CORE_ERROR("Failed to load scene from: %s", absolutePath.string().c_str());
            return;
        }

        m_editorView.rebind(newScene,
            [this](AssetHandle handle) { openMaterialEditor(handle); });
        m_selectedEntity = UINT32_MAX;

        VE_CORE_SUCCESS_PRINT("Scene loaded from: %s", absolutePath.string().c_str());
    }

    void EditorLayer::onEntitySelected(uint32_t entityID){

        auto& cp = m_editorView.getScene()->getComponent<NameComponent>(entityID);

        m_selectedEntity = entityID;
        VE_CORE_SUCCESS_PRINT("Clicked entity %s", cp.name);
    }

    void EditorLayer::performPicking() {
        m_pickingFramebuffer->bind();
        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        RenderCommand::clear();

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

        VE_CORE_SUCCESS_PRINT("Pick pos: (%d, %d), Raw read: %d", (int)m_pickPos.x, (int)m_pickPos.y, entityId);
        m_pickingFramebuffer->unbind();

        if (entityId != -1) {
            onEntitySelected(entityId);
        }

    }

    void EditorLayer::onUpdate() {

        uint32_t fbW = 0, fbH = 0;
        if (!m_framebuffer) return;

        fbW = m_framebuffer->getWidth();
        fbH = m_framebuffer->getHeight();

        m_cameraController.onUpdate(DeltaTime::get().getDeltaTime());
        m_cameraController.getCamera().setAspectRatio((float)fbW / (float)fbH);
        m_cameraController.getCamera().setViewportSize(fbW, fbH);

        if (m_usePBRPipeline && m_renderPipeline) {

            m_renderPipeline->resize(fbW, fbH);
            RenderContext ctx;
            ctx.viewPortWidth = fbW;
            ctx.viewPortHeight = fbH;
            ctx.viewMatrix = m_cameraController.getCamera().getViewMatrix();
            ctx.projMatrix = m_cameraController.getCamera().getJitteredProjectionMatrix();
            ctx.prevViewProjMatrix = m_cameraController.getCamera().getPrevViewProjectionMatrix();
            ctx.cameraPosition = m_cameraController.getCamera().getPosition();
            ctx.totalTime = DeltaTime::get().getCurrentTime();
            ctx.frameIndex = m_cameraController.getCamera().getFrameIndex();

            m_editorView.getScene()->setCameraMatrices(ctx.viewMatrix, ctx.projMatrix);
            m_editorView.getScene()->setCameraPosition(ctx.cameraPosition);

            // Collect all Mesh & Light draw command.
            ctx.drawMeshCommands.clear();
            ctx.drawLightCommands.clear();

            m_editorView.getSceneRenderer().collectAllMesh(ctx);
            m_editorView.getSceneRenderer().collectAllLight(ctx);
            m_editorView.getSceneRenderer().collectAllSprites(ctx);

            // Retrieve skybox cubemap and pass IBL data into the render context
            Ref<TextureCubeMap> cubemap;
            auto skyView = m_editorView.getScene()->getRegistry().view<SkyBoxComponent>();
            if (!skyView.empty()) {
                auto entity = *skyView.begin();
                auto& skyComp = m_editorView.getScene()->getComponent<SkyBoxComponent>(entity);
                cubemap = ResourceManager::get<TextureCubeMap>(skyComp.textureCubeMapHandle);

                // Re-bake IBL maps when the skybox texture changes
                if (skyComp.textureCubeMapHandle != m_bakedSkyboxHandle) {
                    m_bakedSkyboxHandle = skyComp.textureCubeMapHandle;
                    if (cubemap) {
                        VE_CORE_SUCCESS_PRINT("%s", "Editor: skybox changed, re-baking IBL...");
                        m_irradianceMap = IBLBaker::bakeIrradianceMap(cubemap);
                        m_prefilteredEnvMap = IBLBaker::bakePrefilteredEnvMap(cubemap);
                        if (!m_irradianceMap)  m_irradianceMap = cubemap;
                        if (!m_prefilteredEnvMap) m_prefilteredEnvMap = cubemap;
                    }
                }
            }

            ctx.skyboxTexture     = cubemap;
            ctx.irradianceMap     = m_irradianceMap;
            ctx.prefilteredEnvMap = m_prefilteredEnvMap;
            ctx.brdfLUT           = m_brdfLUT;

            // Atmosphere (single global entity, like the skybox)
            auto atmosphereView = m_editorView.getScene()->getRegistry().group<TransformComponent, AtmosphereComponent>();
            if (!atmosphereView.empty()) {
                auto entity = *atmosphereView.begin();
                auto& transform = m_editorView.getScene()->getComponent<TransformComponent>(entity);
                auto& atmosphereComp = m_editorView.getScene()->getComponent<AtmosphereComponent>(entity);
                ctx.atmosphere = atmosphereComp.atmosphere;
                ctx.clouds = atmosphereComp.clouds;
                ctx.planetCenter = glm::vec3(transform.transform[3]);
                ctx.hasAtmosphere = true;

                // Re-bake the transmittance LUT only when the atmosphere params
                // change; a single bake is <10ms but the LUT would otherwise go
                // stale while the inspector edits stay live.
                if (!m_hasCachedAtmosphere ||
                    memcmp(&m_cachedAtmosphereParams, &atmosphereComp.atmosphere, sizeof(AtmosphereParams)) != 0) {
                    m_cachedAtmosphereParams = atmosphereComp.atmosphere;
                    m_hasCachedAtmosphere = true;
                    m_transmittanceTexture = AtmosphereBaker::bakeTransmittanceLUT(atmosphereComp.atmosphere);
                }
                ctx.transmittanceTexture = m_transmittanceTexture;

                // The single-scattering LUT is baked once (2-5s on the CPU), so it
                // is not re-baked on param edits like the cheap transmittance LUT;
                // restart the editor to re-bake with new atmosphere params.
                if (!m_scatteringTexture) {
                    AtmosphereBaker::ScatteringLUTPair pair = AtmosphereBaker::bakeScatteringLUT(atmosphereComp.atmosphere);
                    m_scatteringTexture = pair.rayleighTexture;
                    m_mieScatteringTexture = pair.mieTexture;
                }
                ctx.scatteringTexture = m_scatteringTexture;
                ctx.mieScatteringTexture = m_mieScatteringTexture;

                // Multiple scattering: baked once like the single-scattering LUT.
                if (!m_multipleScatteringTexture) {
                    m_multipleScatteringTexture = AtmosphereBaker::bakeMultipleScatteringLUT(atmosphereComp.atmosphere);
                }
                ctx.multipleScatteringTexture = m_multipleScatteringTexture;

                if (!m_cloudNoiseTexture) {
                    m_cloudNoiseTexture = WorleyNoiseBaker::bakeMultiOctave(8, 128, 0, 4, m_cloudWorleyCells);
                }
                ctx.cloudNoiseTexture = m_cloudNoiseTexture;
                ctx.cloudWorleyCells = (float)m_cloudWorleyCells;

                if (!m_cloudDetailTexture) {
                    m_cloudDetailTexture = WorleyNoiseBaker::bakeDetailWorley(4, 64, 1, 3, m_cloudDetailCells);
                }
                ctx.cloudDetailTexture = m_cloudDetailTexture;
                ctx.cloudDetailCells = (float)m_cloudDetailCells;

                if (!m_cloudWarpTexture) {
                    m_cloudWarpTexture = WorleyNoiseBaker::bakeWarp(4, 64, 2, m_cloudWarpCells);
                }
                ctx.cloudWarpTexture = m_cloudWarpTexture;
                ctx.cloudWarpCells = (float)m_cloudWarpCells;

                if (!m_cloudWeatherMap) {
                    m_cloudWeatherMap = WeatherMapBaker::bake(512, 4, 3);
                }
                ctx.cloudWeatherMap = m_cloudWeatherMap;
            }

            m_renderPipeline->render(ctx);
            m_cameraController.getCamera().endFrame();

            m_framebuffer->bind();
            RenderCommand::setClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            RenderCommand::clear();

            auto hdrIt = ctx.inputTextures.find("hdrColor");
            if (hdrIt != ctx.inputTextures.end()) {
                m_renderPipeline->bindScreenShader();
                auto depthIt = ctx.inputTextures.find("depth");
                if (depthIt != ctx.inputTextures.end()) {
                    m_renderPipeline->getScreenShader()->setTexture("u_DepthMap", depthIt->second, 1);
                }
                m_renderPipeline->getScreenShader()->setTexture("u_ScreenTexture", hdrIt->second, 0);
                m_renderPipeline->drawFullscreenQuad();
                m_renderPipeline->unbindScreenShader();
            }

            if (cubemap) {
                Renderer3D::setSkyboxTexture(cubemap);
                Renderer3D::drawSkybox(ctx.viewMatrix, ctx.projMatrix);
            }

            // Overlay: light billboard icons
            renderLightBillboards(ctx.viewMatrix, ctx.projMatrix, ctx.cameraPosition);

            // Overlay: sprite quads (Renderer2D independent of PBR pipeline)
            if (!ctx.drawSpriteCommands.empty()) {
                Renderer2D::beginScene(ctx.projMatrix * ctx.viewMatrix);
                for (auto& cmd : ctx.drawSpriteCommands) {
                    Renderer2D::drawQuad(cmd.transform, cmd.size, cmd.textureHandle);
                }
                Renderer2D::endScene();
            }

            m_framebuffer->unbind();
        }
        else {
            if (m_framebuffer) {
                m_framebuffer->bind();
                RenderCommand::setClearColor(0.1f, 0.1f, 0.1f, 1.0f);
                RenderCommand::clear();
                Renderer2D::beginScene(m_cameraController.getCamera().getViewProjectionMatrix());
                Renderer3D::beginScene(m_cameraController.getCamera().getViewProjectionMatrix());
                m_editorView.getScene()->setCameraMatrices(m_cameraController.getCamera().getViewMatrix(), m_cameraController.getCamera().getProjectionMatrix());
                m_editorView.getScene()->onUpdate(DeltaTime::get().getDeltaTime());
                Renderer2D::endScene();
                Renderer3D::endScene();

                // Overlay: light billboard icons
                renderLightBillboards(
                    m_cameraController.getCamera().getViewMatrix(),
                    m_cameraController.getCamera().getProjectionMatrix(),
                    m_cameraController.getCamera().getPosition()
                );

                m_framebuffer->unbind();
            }
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
