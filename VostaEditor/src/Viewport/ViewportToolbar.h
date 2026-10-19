#pragma once

#include <VostaEngine.h>

#include <functional>

namespace ve {

class PlaySession;

// The viewport toolbar strip: FPS readout, an editor-camera popup (world
// position, fly speed, view distance), wireframe / ground-grid toggles, the
// Play-in-Editor transport, the view-camera picker and VSync. Owns its own
// widgets and icons.
//
// Play and Stop cross back into the layer through callbacks because they touch
// editor-wide state (selection, the runtime scene swap, panel locking). Pause,
// Resume and Step act on the session directly.
class ViewportToolbar {
public:
    struct Callbacks {
        std::function<void()> onPlay;
        std::function<void()> onStop;
    };
    void setCallbacks(Callbacks callbacks) { m_callbacks = std::move(callbacks); }

    // Loads the toolbar icons. Call from the layer's onAttach, where a GL context
    // is live.
    void attach();

    // Draws the strip inline, into whatever window is current. `scene` is the
    // authored scene the view-camera picker lists cameras from.
    void onGuiRender(SceneView& view, const Ref<Scene>& scene, PlaySession& play);

    // Camera fly-speed boost. The multiplier is applied by the layer for the one
    // frame the camera updates in -- CameraController itself stays boost-agnostic,
    // and the controller keeps holding the un-multiplied base speed, so that is
    // also what the camera store persists.
    static constexpr float kCameraBoostMultiplier = 100.0f;
    bool isCameraBoostEnabled() const { return m_cameraBoost; }

private:
    Callbacks m_callbacks;

    // Viewport toolbar toggle icons.
    Ref<Texture2D> m_wireframeIcon;
    Ref<Texture2D> m_groundGridIcon;

    // Play-in-Editor transport icons.
    Ref<Texture2D> m_playIcon;
    Ref<Texture2D> m_pauseIcon;
    Ref<Texture2D> m_stepIcon;
    Ref<Texture2D> m_stopIcon;

    // Widget state. m_farPlane is seeded from the editor camera on the first
    // frame, so the slider starts where the view already is.
    float m_fpsTimer  = 0.0f;
    int   m_fpsFrames = 0;
    float m_fpsValue  = 0.0f;
    float m_farPlane = 0.0f;
    bool  m_farPlaneSeeded = false;
    bool  m_wireframe = false;
    bool  m_groundGrid = false;
    bool  m_cameraBoost = false;
    bool  m_taa = true;
    bool  m_showGlobe = false;
    bool  m_vsync = false;
    int   m_rvtDebug = 0;
    // Resource residency budget in MB (0 = eviction off).
    int   m_resourceBudgetMB = 256;
};

}
