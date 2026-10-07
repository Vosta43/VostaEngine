#include "SandBoxLayer.h"

SandboxLayer::SandboxLayer() {
    m_scene = ve::CreateRef<ve::Scene>();
    m_viewRenderer = ve::CreateRef<ve::SceneViewRenderer>(m_scene);
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
    m_renderPipeline->init(1280, 720, "SandBox/assets/pipelines/default.json");

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

    // Player initial template — blank starting point for the game
    auto player = m_scene->createEntity();
    m_scene->assignComponent<ve::NameComponent>(player, "Player");
    m_scene->assignComponent<ve::TransformComponent>(player, glm::mat4(1.0f));
    m_scene->assignComponent<ve::Player>(player);

    // Create initial framebuffer from window size
    auto* win = ve::Application::get().getWindow();
    m_framebuffer = ve::Framebuffer::create(win->getWidth(), win->getHeight());

    VE_CORE_SUCCESS_PRINT("%s", "Sandbox: player template ready");
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
    m_cameraController.getCamera().setViewportSize(fbW, fbH);

    if (!m_renderPipeline) return;

    m_viewRenderer->render(m_cameraController.getCamera(), m_renderPipeline, m_framebuffer);
}
