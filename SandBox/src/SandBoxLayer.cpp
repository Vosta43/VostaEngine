#include "SandBoxLayer.h"
#include "Renderer/Preprocess/AtmosphereBaker.h"
#include "Renderer/Preprocess/IBLBaker.h"
#include "Renderer/Preprocess/WorleyNoiseBaker.h"
#include "Renderer/Preprocess/WeatherMapBaker.h"

SandboxLayer::SandboxLayer() {
    m_scene = ve::CreateRef<ve::Scene>();
    m_sceneRenderer = ve::CreateRef<ve::SceneRenderer>(m_scene);
}

void SandboxLayer::onAttach() {

    auto& shaderLib = ve::Application::get().getShaderLibrary();
    shaderLib.load("SandBox/assets/shaders/Model.glsl");
    shaderLib.load("SandBox/assets/shaders/Texture.glsl");

    ve::RenderCommand::init();
    ve::RenderCommand::setDepthTesting(true);
    ve::RenderCommand::setCursorVisible(true);
    ve::Renderer2D::init();
    ve::Renderer3D::init();

    m_renderPipeline = ve::CreateRef<ve::RenderPipeline>();
    m_renderPipeline->init(1280, 720);

    m_cameraController.getCamera().setProjectionType(true);

    ve::Application::get().getDispatcher()->subscribe(ve::Event::Type::MouseScrolled,
        [this](ve::Event& e) {
            m_cameraController.onEvent(e);
        });

    ve::Application::get().getDispatcher()->subscribe(ve::Event::Type::MouseButtonPressed,
        [this](ve::Event& e) {
            auto& me = static_cast<ve::MouseButtonPressedEvent&>(e);
            if (me.getButton() == VE_MOUSE_BUTTON_RIGHT) {
                m_cameraController.setActive(true);
                ve::RenderCommand::setCursorVisible(false);
            }
        });

    ve::Application::get().getDispatcher()->subscribe(ve::Event::Type::MouseButtonReleased,
        [this](ve::Event& e) {
            auto& me = static_cast<ve::MouseButtonReleasedEvent&>(e);
            if (me.getButton() == VE_MOUSE_BUTTON_RIGHT) {
                m_cameraController.setActive(false);
                ve::RenderCommand::setCursorVisible(true);
            }
        });

    ve::Application::get().getDispatcher()->subscribe(ve::Event::Type::WindowResize, [this](ve::Event& e) {
        auto& resizeEvent = static_cast<ve::WindowResizeEvent&>(e);
        uint32_t width  = static_cast<uint32_t>(resizeEvent.getWidth());
        uint32_t height = static_cast<uint32_t>(resizeEvent.getHeight());
        ve::RenderCommand::setViewport(0, 0, width, height);
        m_cameraController.getCamera().setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    });

    // Load light icon for billboards
    m_pointLightIcon = ve::ResourceManager::store<ve::Texture2D>("VostaEngine/resources/icons/point_light.png");

    // ── Test entities ────────────────────────────────────────────────────

    // Sprite
    auto crateHandle = ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/textures/crate.png");
    auto nick = m_scene->createEntity();
    m_scene->assignComponent<ve::NameComponent>(nick, "Nick");
    m_scene->assignComponent<ve::TransformComponent>(nick, glm::mat4(1.0f));
    m_scene->assignComponent<ve::LightComponent>(nick);
    m_scene->assignComponent<ve::SpriteRendererComponent>(nick, glm::vec2(5, 5), crateHandle);

    auto miHandle = ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/textures/mi.png");
    auto mi = m_scene->createEntity();
    m_scene->assignComponent<ve::NameComponent>(mi, "Vosta");
    m_scene->assignComponent<ve::TransformComponent>(mi, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 1.0f)));
    m_scene->assignComponent<ve::SpriteRendererComponent>(mi, glm::vec2(5, 5), miHandle);
    m_scene->assignComponent<ve::Player>(mi);

    // Static meshes
    auto sphereHandle = ve::ResourceManager::store<ve::StaticMesh>("SandBox/assets/models/sphere.obj");
    auto sphereMat = ve::ResourceManager::store<ve::Material>("s");
    auto sphere = m_scene->createEntity();
    m_scene->assignComponent<ve::NameComponent>(sphere, "sphere");
    m_scene->assignComponent<ve::StaticMeshComponent>(sphere, sphereHandle, sphereMat);
    m_scene->assignComponent<ve::TransformComponent>(sphere, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 5.0f, 3.0f)));

    auto sofaHandle = ve::ResourceManager::store<ve::StaticMesh>("SandBox/assets/models/sofa_test.obj");
    auto sofaMat = ve::ResourceManager::store<ve::Material>("n");
    auto et = m_scene->createEntity();
    m_scene->assignComponent<ve::NameComponent>(et, "sofa");
    m_scene->assignComponent<ve::TransformComponent>(et, glm::translate(glm::mat4(1.0f), glm::vec3(8.0f, 4.0f, 3.0f)));
    m_scene->assignComponent<ve::StaticMeshComponent>(et, sofaHandle, sofaMat);

    // Skybox
    auto skyboxHandle = ve::ResourceManager::store<ve::TextureCubeMap>("SandBox/assets/textures/bryanston_park_sunrise_2k.hdr");
    auto skybox = m_scene->createEntity();
    m_scene->assignComponent<ve::NameComponent>(skybox, "skybox");
    m_scene->assignComponent<ve::SkyBoxComponent>(skybox, skyboxHandle);
    m_scene->assignComponent<ve::TransformComponent>(skybox, glm::mat4(1.0f));

    bakeIBL(skyboxHandle);

    // PBR debug textures — preload into resource manager
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/lion_head_diff_1k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/lion_head_nor_dx_1k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/lion_head_rough_1k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/lion_head_ao_1k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/lion_head_metal_1k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/sofa_03_ao_2k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/sofa_03_rough_2k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/sofa_03_diff_2k.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/sofa_03_nor_dx_2k.png");

    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Bottom_BaseColor.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Bottom_Metallic.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Bottom_Normal.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Bottom_Occlusion.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Bottom_Roughness.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Top_BaseColor.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Top_Metallic.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Top_Normal.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Top_Occlusion.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Base_Top_Roughness.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Turret_BaseColor.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Turret_Metallic.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Turret_Normal.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Turret_Occlusion.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Turret_Roughness.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Engines_BaseColor.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Engines_Metallic.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Engines_Normal.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Engines_Occlusion.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Engines_Roughness.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Wings_BaseColor.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Wings_Metallic.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Wings_Normal.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Wings_Occlusion.png");
    ve::ResourceManager::store<ve::Texture2D>("SandBox/assets/models/hc/Wings_Roughness.png");

    ve::ResourceManager::store<ve::TextureCubeMap>("SandBox/assets/textures/skybox2.png");
    ve::ResourceManager::store<ve::TextureCubeMap>("SandBox/assets/textures/skybox3.png");
    ve::ResourceManager::store<ve::TextureCubeMap>("SandBox/assets/textures/skybox4.png");

    // Create initial framebuffer from window size
    auto* win = ve::Application::get().getWindow();
    m_framebuffer = ve::Framebuffer::create(win->getWidth(), win->getHeight());

    VE_CORE_SUCCESS_PRINT("%s", "Sandbox: test scene ready");
}

void SandboxLayer::bakeIBL(ve::AssetHandle skyboxHandle) {
    auto cubemap = ve::ResourceManager::get<ve::TextureCubeMap>(skyboxHandle);
    if (!cubemap) {
        VE_CORE_ERROR_PRINT("%s", "Sandbox: skybox cubemap is null, cannot bake IBL");
        return;
    }

    VE_CORE_SUCCESS_PRINT("%s", "Sandbox: baking IBL maps...");
    m_irradianceMap = ve::IBLBaker::bakeIrradianceMap(cubemap);
    m_prefilteredEnvMap = ve::IBLBaker::bakePrefilteredEnvMap(cubemap);

    if (!m_irradianceMap) {
        VE_CORE_ERROR_PRINT("%s", "Sandbox: irradiance bake failed, fallback to raw skybox");
        m_irradianceMap = cubemap;
    }
    if (!m_prefilteredEnvMap) {
        VE_CORE_ERROR_PRINT("%s", "Sandbox: prefilter bake failed, fallback to raw skybox");
        m_prefilteredEnvMap = cubemap;
    }

    m_brdfLUT = ve::IBLBaker::bakeBRDFLUT();
    if (!m_brdfLUT) {
        VE_CORE_ERROR_PRINT("%s", "Sandbox: BRDF LUT bake failed");
    }

    m_bakedSkyboxHandle = skyboxHandle;
}

void SandboxLayer::onUpdate() {
    // Resize framebuffer on window resize
    auto* win = ve::Application::get().getWindow();
    uint32_t winW = win->getWidth();
    uint32_t winH = win->getHeight();
    if (!m_framebuffer || m_framebuffer->getWidth() != winW || m_framebuffer->getHeight() != winH) {
        m_framebuffer = ve::Framebuffer::create(winW, winH);
    }

    uint32_t fbW = m_framebuffer->getWidth();
    uint32_t fbH = m_framebuffer->getHeight();

    m_cameraController.onUpdate(ve::DeltaTime::get().getDeltaTime());
    m_cameraController.getCamera().setAspectRatio(static_cast<float>(fbW) / static_cast<float>(fbH));

    if (!m_renderPipeline) return;

    m_renderPipeline->resize(fbW, fbH);
    ve::RenderContext ctx;
    ctx.viewPortWidth  = fbW;
    ctx.viewPortHeight = fbH;
    ctx.viewMatrix     = m_cameraController.getCamera().getViewMatrix();
    ctx.projMatrix     = m_cameraController.getCamera().getProjectionMatrix();
    ctx.cameraPosition = m_cameraController.getCamera().getPosition();
    ctx.totalTime      = ve::DeltaTime::get().getCurrentTime();
    ctx.frameIndex     = m_cameraController.getCamera().getFrameIndex();

    m_scene->setCameraMatrices(ctx.viewMatrix, ctx.projMatrix);
    m_scene->setCameraPosition(ctx.cameraPosition);

    ctx.drawMeshCommands.clear();
    ctx.drawLightCommands.clear();

    m_sceneRenderer->collectAllMesh(ctx);
    m_sceneRenderer->collectAllLight(ctx);

    // Skybox & IBL
    ve::Ref<ve::TextureCubeMap> cubemap;
    auto skyView = m_scene->getRegistry().view<ve::SkyBoxComponent>();
    if (!skyView.empty()) {
        auto entity = *skyView.begin();
        auto& skyComp = m_scene->getComponent<ve::SkyBoxComponent>(entity);
        cubemap = ve::ResourceManager::get<ve::TextureCubeMap>(skyComp.textureCubeMapHandle);

        if (skyComp.textureCubeMapHandle != m_bakedSkyboxHandle) {
            bakeIBL(skyComp.textureCubeMapHandle);
        }
    }

    ctx.skyboxTexture     = cubemap;
    ctx.irradianceMap     = m_irradianceMap;
    ctx.prefilteredEnvMap = m_prefilteredEnvMap;
    ctx.brdfLUT           = m_brdfLUT;

    // Atmosphere (single global entity, like the skybox)
    auto atmosphereView = m_scene->getRegistry().group<ve::TransformComponent, ve::AtmosphereComponent>();
    if (!atmosphereView.empty()) {
        auto entity = *atmosphereView.begin();
        auto& transform = m_scene->getComponent<ve::TransformComponent>(entity);
        auto& atmosphereComp = m_scene->getComponent<ve::AtmosphereComponent>(entity);
        ctx.atmosphere = atmosphereComp.atmosphere;
        ctx.clouds = atmosphereComp.clouds;
        ctx.planetCenter = glm::vec3(transform.transform[3]);
        ctx.hasAtmosphere = true;

        if (!m_transmittanceTexture) {
            m_transmittanceTexture = ve::AtmosphereBaker::bakeTransmittanceLUT(atmosphereComp.atmosphere);
        }
        ctx.transmittanceTexture = m_transmittanceTexture;

        if (!m_scatteringTexture) {
            ve::AtmosphereBaker::ScatteringLUTPair pair = ve::AtmosphereBaker::bakeScatteringLUT(atmosphereComp.atmosphere);
            m_scatteringTexture = pair.rayleighTexture;
            m_mieScatteringTexture = pair.mieTexture;
        }
        ctx.scatteringTexture = m_scatteringTexture;
        ctx.mieScatteringTexture = m_mieScatteringTexture;

        if (!m_multipleScatteringTexture) {
            m_multipleScatteringTexture = ve::AtmosphereBaker::bakeMultipleScatteringLUT(atmosphereComp.atmosphere);
        }
        ctx.multipleScatteringTexture = m_multipleScatteringTexture;

        if (!m_cloudNoiseTexture) {
            m_cloudNoiseTexture = ve::WorleyNoiseBaker::bakeMultiOctave(8, 128, 0, 4, m_cloudWorleyCells);
        }
        ctx.cloudNoiseTexture = m_cloudNoiseTexture;
        ctx.cloudWorleyCells = (float)m_cloudWorleyCells;

        if (!m_cloudDetailTexture) {
            m_cloudDetailTexture = ve::WorleyNoiseBaker::bakeDetailWorley(4, 64, 1, 3, m_cloudDetailCells);
        }
        ctx.cloudDetailTexture = m_cloudDetailTexture;
        ctx.cloudDetailCells = (float)m_cloudDetailCells;

        if (!m_cloudWarpTexture) {
            m_cloudWarpTexture = ve::WorleyNoiseBaker::bakeWarp(4, 64, 2, m_cloudWarpCells);
        }
        ctx.cloudWarpTexture = m_cloudWarpTexture;
        ctx.cloudWarpCells = (float)m_cloudWarpCells;

        if (!m_cloudWeatherMap) {
            m_cloudWeatherMap = ve::WeatherMapBaker::bake(512, 4, 3);
        }
        ctx.cloudWeatherMap = m_cloudWeatherMap;
    }

    m_renderPipeline->render(ctx);

    m_framebuffer->bind();
    ve::RenderCommand::setClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    ve::RenderCommand::clear();

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
        ve::Renderer3D::setSkyboxTexture(cubemap);
        ve::Renderer3D::drawSkybox(ctx.viewMatrix, ctx.projMatrix);
    }

    // Light billboards
    {
        auto lightView = m_scene->getRegistry().view<ve::LightComponent>();
        if (!lightView.empty() && m_pointLightIcon.isValid()) {
            const auto& viewMatrix = m_cameraController.getCamera().getViewMatrix();
            const auto& cameraPos  = m_cameraController.getCamera().getPosition();

            ve::Renderer2D::setBlending(true);
            ve::RenderCommand::setDepthTesting(false);

            glm::mat4 vp = ctx.projMatrix * viewMatrix;
            ve::Renderer2D::beginScene(vp);

            for (auto entity : lightView) {
                auto& transform = m_scene->getComponent<ve::TransformComponent>(entity);
                glm::vec3 lightPos = glm::vec3(transform.transform[3]);

                float dist = glm::length(lightPos - cameraPos);
                float scale = glm::clamp(dist * 0.08f, 0.3f, 3.0f) * m_lightIconSize;

                glm::mat4 billboardMat(1.0f);
                billboardMat[0] = glm::vec4(glm::normalize(glm::vec3(viewMatrix[0])), 0.0f);
                billboardMat[1] = glm::vec4(glm::normalize(glm::vec3(viewMatrix[1])), 0.0f);
                billboardMat[2] = glm::vec4(glm::normalize(glm::vec3(viewMatrix[2])), 0.0f);
                billboardMat[3] = glm::vec4(lightPos, 1.0f);

                ve::Renderer2D::drawQuad(billboardMat, glm::vec2(scale), m_pointLightIcon);
            }

            ve::Renderer2D::endScene();
            ve::RenderCommand::setDepthTesting(true);
            ve::Renderer2D::setBlending(false);
        }
    }

    m_framebuffer->unbind();
}
