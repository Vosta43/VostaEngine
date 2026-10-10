#pragma once

#include "Core/Core.h"
#include "Renderer/CameraController.h"
#include "Scene/Entity.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ve {

class Scene;
class SceneViewRenderer;
class RenderPipeline;
class Framebuffer;
class Event;

// How a view is spun up. Host-relative asset paths live here so the engine
// never hardcodes a project's directory layout.
struct SceneViewConfig {
    uint32_t    width        = 1280;
    uint32_t    height       = 720;
    std::string pipelinePath;              // JSON pipeline asset; empty = no pipeline
    bool        perspective  = true;
    // Ortho bounds for the CameraController's camera; unused while perspective.
    float       orthoLeft    = -1.6f;
    float       orthoRight   = 1.6f;
    float       orthoBottom  = -0.9f;
    float       orthoTop     = 0.9f;
    std::vector<std::string> sceneShaders; // content shaders, e.g. Model.glsl
};

// One rendered view of a scene: camera + controller, the offscreen color target,
// the pipeline that renders into it, and the input that drives the camera.
//
// Host-driven on purpose -- the view never reads the Window. The host feeds a
// size each frame (SandBox: the window size; the editor: its viewport-panel
// size) and decides how the result is presented (blit to the backbuffer,
// ImGui::Image, ...). SceneView stops at "render + camera + input": gizmos,
// picking and other overlays stay in the host.
class VE_API SceneView {
public:
    explicit SceneView(const Ref<Scene>& scene, const SceneViewConfig& config = {});
    ~SceneView();

    SceneView(const SceneView&) = delete;
    SceneView& operator=(const SceneView&) = delete;

    // One-shot global init + pipeline build + event subscriptions. Call this
    // from the host layer's onAttach(), where the GL context is already live.
    void onAttach();

    // Host policy gate: when false the camera ignores new input and is released.
    void setInputEnabled(bool enabled);
    bool isInputEnabled() const { return m_inputEnabled; }

    // Host-defined target size; recreates the offscreen target when it changes.
    void setSize(uint32_t width, uint32_t height);

    // Per frame: camera update, aspect/viewport, then render. A no-op until a
    // non-zero size has been set.
    void onUpdate(float deltaTime);

    // The camera the view renders from: the free camera in Editor mode, or the
    // scene camera in Scene mode. Hosts reading it for picking/gizmos/overlays
    // get whichever is live.
    Camera&            getCamera() { return isSceneDriven() ? m_sceneCamera : m_cameraController.getCamera(); }
    const Camera&      getCamera() const { return isSceneDriven() ? m_sceneCamera : m_cameraController.getCamera(); }
    // The editor's own free camera. A scene excursion never writes into it, so
    // persisted session state (camera sidecar, move speed) reads from here.
    Camera&            getEditorCamera() { return m_cameraController.getCamera(); }
    const Camera&      getEditorCamera() const { return m_cameraController.getCamera(); }
    CameraController&  getCameraController() { return m_cameraController; }
    SceneViewRenderer& getViewRenderer();
    Ref<Scene>         getScene() const { return m_scene; }

    // Swap the rendered scene (editor rebind). Recreates the view renderer.
    void setScene(const Ref<Scene>& scene);

    // Where the view's camera comes from. Editor = the built-in free camera the
    // host drives with input. Scene = a CameraComponent in the scene, its pose
    // taken from that entity's Transform -- a game/piloted view. The two cameras
    // are distinct objects, so the free camera's state survives a Scene excursion.
    enum class CameraSource { Editor, Scene };
    void setCameraSource(CameraSource source) { m_cameraSource = source; }
    CameraSource getCameraSource() const { return m_cameraSource; }
    bool isSceneDriven() const { return m_cameraSource == CameraSource::Scene; }

    // Which scene camera to pilot. Invalid (id 0xFFFFFFFF) = the scene's primary.
    void setSceneCameraEntity(const Entity& entity) { m_sceneCameraEntity = entity; }
    Entity getSceneCameraEntity() const { return m_sceneCameraEntity; }

    Ref<Framebuffer>    getFramebuffer() const { return m_framebuffer; }
    uint32_t            getColorAttachmentId() const;   // GL id, for ImGui::Image
    Ref<RenderPipeline> getPipeline() const { return m_pipeline; }
    void                setPipeline(const Ref<RenderPipeline>& pipeline) { m_pipeline = pipeline; }

    uint32_t getWidth()  const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    void setWireframe(bool wireframe);
    void setGroundGrid(bool groundGrid);

private:
    void handleScroll(Event& e);
    void handleMousePressed(Event& e);
    void handleMouseReleased(Event& e);

    // Drive the camera from a scene CameraComponent: the entity set by
    // setSceneCameraEntity, or the scene's primary when none was set. No-op when
    // the scene has no camera, so the last pose stays rather than snapping to
    // origin.
    void applySceneCamera(Camera& camera);

    Ref<Scene>             m_scene;
    Ref<SceneViewRenderer> m_viewRenderer;
    Ref<RenderPipeline>    m_pipeline;
    Ref<Framebuffer>       m_framebuffer;
    CameraController       m_cameraController;
    Camera                 m_sceneCamera;        // rendered while Scene-driven
    Entity                 m_sceneCameraEntity;  // invalid = use the primary
    SceneViewConfig        m_config;
    CameraSource           m_cameraSource = CameraSource::Editor;
    uint32_t               m_width  = 0;
    uint32_t               m_height = 0;
    bool                   m_inputEnabled = true;
    bool                   m_attached     = false;
};

}
