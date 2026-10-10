#include "vepch.h"
#include "SceneView.h"

#include "MouseButtonCodes.h"
#include "Core/Application.h"
#include "Core/Events/Event.h"
#include "Core/Events/EventDispatcher.h"
#include "Core/Events/MouseButtonPressedEvent.h"
#include "Core/Events/MouseButtonReleasedEvent.h"
#include "Core/Events/MouseScrolledEvent.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderPipeline.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/SceneViewRenderer.h"
#include "Renderer/Shader.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"

namespace ve {

    namespace {

        // The renderer subsystems are process-global, so every view shares one init.
        void initRendererGlobalsOnce() {
            static bool initialized = false;
            if (initialized) return;
            initialized = true;

            RenderCommand::init();
            RenderCommand::setDepthTesting(true);
            RenderCommand::setCursorVisible(true);
            Renderer2D::init();
            Renderer3D::init();
        }

    }

    SceneView::SceneView(const Ref<Scene>& scene, const SceneViewConfig& config)
        : m_scene(scene)
        , m_viewRenderer(scene ? CreateRef<SceneViewRenderer>(scene) : nullptr)
        , m_cameraController(config.orthoLeft, config.orthoRight, config.orthoBottom, config.orthoTop)
        , m_sceneCamera(config.orthoLeft, config.orthoRight, config.orthoBottom, config.orthoTop)
        , m_config(config)
        , m_width(config.width)
        , m_height(config.height)
    {
        // Deterministic start: the fly-cam is active only while the host drives it
        // (right-drag in both hosts), never from an uninitialized m_active read.
        m_cameraController.setActive(false);
    }

    SceneView::~SceneView() = default;

    void SceneView::onAttach() {
        if (m_attached) return;
        m_attached = true;

        initRendererGlobalsOnce();

        auto& shaderLib = Application::get().getShaderLibrary();
        for (const std::string& path : m_config.sceneShaders)
            shaderLib.load(path);

        if (!m_config.pipelinePath.empty()) {
            m_pipeline = CreateRef<RenderPipeline>();
            m_pipeline->init(m_width, m_height, m_config.pipelinePath);
        }

        m_cameraController.getCamera().setProjectionType(m_config.perspective);

        auto* dispatcher = Application::get().getDispatcher();
        dispatcher->subscribe(Event::Type::MouseScrolled,
            [this](Event& e) { handleScroll(e); });
        dispatcher->subscribe(Event::Type::MouseButtonPressed,
            [this](Event& e) { handleMousePressed(e); });
        dispatcher->subscribe(Event::Type::MouseButtonReleased,
            [this](Event& e) { handleMouseReleased(e); });
    }

    void SceneView::setInputEnabled(bool enabled) {
        m_inputEnabled = enabled;
        // Releasing input also releases the gripper flag so re-entering the view
        // does not resume from a stale cursor delta.
        if (!enabled) m_cameraController.setActive(false);
    }

    void SceneView::setSize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return;
        if (m_framebuffer && m_framebuffer->getWidth() == width && m_framebuffer->getHeight() == height)
            return;

        m_width  = width;
        m_height = height;
        m_framebuffer = Framebuffer::create(width, height);
    }

    void SceneView::onUpdate(float deltaTime) {
        if (!m_framebuffer) return;

        const bool sceneDriven = isSceneDriven();
        Camera& camera = sceneDriven ? m_sceneCamera : m_cameraController.getCamera();

        if (sceneDriven) {
            // The free camera is suspended while a scene camera is piloted;
            // release it so returning to Editor mode does not resume a stale drag.
            m_cameraController.setActive(false);
            applySceneCamera(m_sceneCamera);
        } else {
            m_cameraController.onUpdate(deltaTime);
        }

        camera.setAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
        camera.setViewportSize(m_width, m_height);

        if (m_pipeline && m_viewRenderer)
            m_viewRenderer->render(camera, m_pipeline, m_framebuffer);
    }

    void SceneView::applySceneCamera(Camera& camera) {
        if (!m_scene) return;

        // The piloted entity, or the scene's primary when none was chosen. A
        // stale id (entity deleted since it was picked) falls back to primary.
        Entity entity = m_sceneCameraEntity;
        if (entity.getId() == 0xFFFFFFFFu
            || m_scene->getEntity(entity.getId()).getId() == 0xFFFFFFFFu)
            entity = m_scene->getPrimaryCameraEntity();
        if (entity.getId() == 0xFFFFFFFFu) return;

        auto& registry = m_scene->getRegistry();
        if (!registry.has<CameraComponent>(entity) || !registry.has<TransformComponent>(entity))
            return;

        const CameraComponent& cam = m_scene->getComponent<CameraComponent>(entity);
        const TransformComponent& tf = m_scene->getComponent<TransformComponent>(entity);

        camera.setWorldTransform(tf.transform);
        camera.setProjectionType(cam.perspective);
        camera.setPerspectiveFov(cam.fov);
        camera.setNearPlane(cam.nearPlane);
        camera.setFarPlane(cam.farPlane);
    }

    SceneViewRenderer& SceneView::getViewRenderer() {
        return *m_viewRenderer;
    }

    void SceneView::setScene(const Ref<Scene>& scene) {
        m_scene = scene;
        m_viewRenderer = scene ? CreateRef<SceneViewRenderer>(scene) : nullptr;
    }

    uint32_t SceneView::getColorAttachmentId() const {
        return m_framebuffer ? m_framebuffer->getColorAttachmentRendererID() : 0;
    }

    void SceneView::setWireframe(bool wireframe) {
        if (m_viewRenderer) m_viewRenderer->setWireframe(wireframe);
    }

    void SceneView::setGroundGrid(bool groundGrid) {
        if (m_viewRenderer) m_viewRenderer->setGroundGrid(groundGrid);
    }

    void SceneView::handleScroll(Event& e) {
        // Fly-cam input only while the free camera is the view source.
        if (!m_inputEnabled || isSceneDriven()) return;
        m_cameraController.onEvent(e);
    }

    void SceneView::handleMousePressed(Event& e) {
        auto& me = static_cast<MouseButtonPressedEvent&>(e);
        if (me.getButton() == VE_MOUSE_BUTTON_RIGHT && m_inputEnabled && !isSceneDriven()) {
            m_cameraController.setActive(true);
            RenderCommand::setCursorVisible(false);
        }
    }

    void SceneView::handleMouseReleased(Event& e) {
        auto& me = static_cast<MouseButtonReleasedEvent&>(e);
        // Released unconditionally: losing the view mid-drag must not strand the
        // cursor hidden or the controller stuck active.
        if (me.getButton() == VE_MOUSE_BUTTON_RIGHT) {
            m_cameraController.setActive(false);
            RenderCommand::setCursorVisible(true);
        }
    }

}
