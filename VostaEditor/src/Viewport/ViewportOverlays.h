#pragma once

#include <VostaEngine.h>

#include <glm.hpp>

#include "imgui.h"

namespace ve {

class TerrainEditor;

// Everything the editor draws into or atop the viewport image that is not the
// scene render itself: the transform gizmo, camera-frustum wireframes, the
// terrain brush cursor, light/camera billboard icons, and the picking pass.
// Editor overlay only -- the engine draws none of it.
//
// Selection is shared with the layer's outliner and passes through by reference,
// the same way EditorViewController::onGuiRender takes it.
class ViewportOverlays {
public:
    // Loads the picking shader and the billboard icons. Call from onAttach, where
    // a GL context is live.
    void attach();

    // Queue a pick at an FBO-space position (y-down). Consumed by performPick().
    void requestPick(const glm::vec2& fboPos) {
        m_pickPos = fboPos;
        m_needsPicking = true;
    }
    bool needsPick() const { return m_needsPicking; }
    void cancelPick() { m_needsPicking = false; }

    // Size the picking target to the viewport, recreating it when it changes.
    void ensurePickTarget(uint32_t width, uint32_t height);

    // The overlays that sit on top of the viewport image: selection gizmo, camera
    // frustums and the terrain brush cursor. `scene` is the authored scene;
    // mouseInViewport is the caller's geometric hit test, reused so the ring and
    // the paint stroke agree on where the cursor is.
    void drawOverlays(SceneView& view, const Ref<Scene>& scene, uint32_t& selectedEntity,
                      const ImVec2& viewportPos, const ImVec2& viewportSize,
                      bool mouseInViewport, bool sceneLive, bool showTerrainBrush,
                      TerrainEditor& terrainEditor, const glm::vec2& mouseViewportPos);

    // Billboard icons (skipped while a play session is live) and the pending pick.
    void renderBillboards(SceneView& view, const Ref<Scene>& scene, bool sceneLive);
    // Run the picking pass at the queued point and return the entity under it, or
    // -1 when nothing was hit. Clears the pending request.
    int32_t performPick(SceneView& view, const Ref<Scene>& scene);

private:
    static glm::mat4 makeBillboard(const glm::vec3& position, const glm::mat4& viewMatrix);
    void renderLightBillboards(SceneView& view, const Ref<Scene>& scene, const glm::vec3& cameraPos);
    void renderCameraBillboards(SceneView& view, const Ref<Scene>& scene, const glm::vec3& cameraPos);

    Ref<Shader> m_pickingShader;

    // Light icon billboard.
    AssetHandle m_pointLightIcon;
    float m_lightIconSize = 0.5f;

    // Camera icon billboard, so a CameraComponent has a visible origin.
    AssetHandle m_cameraIcon;
    float m_cameraIconSize = 0.5f;

    Ref<Framebuffer> m_pickingFramebuffer;
    bool      m_needsPicking = false;
    glm::vec2 m_pickPos = { 0.0f, 0.0f };
};

}
