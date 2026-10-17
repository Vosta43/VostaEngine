#pragma once

#include <VostaEngine.h>

#include <filesystem>
#include <glm.hpp>

namespace ve {

// Editor-view camera persistence: a "<scene>.camera.json" sidecar next to the
// scene file, so reopening a scene resumes where the user left off. Session
// state, not document state -- deliberately kept out of the .veworld, but tied to
// the scene path rather than a name-keyed bucket in Saved/, so it follows the
// scene through renames, moves and copies. Only the control state is stored
// (position + yaw/pitch), not the derived matrices, so it round-trips through
// setYawPitch exactly.
//
// Only the read/write mechanics live here. The ORDER of saves around a scene
// switch -- save under the old path, flip the name, restore the new path -- is
// the caller's business, and this class never decides it.
class EditorCameraStore {
public:
    // Load the sidecar for scenePath and adopt it as the current view. Absent is
    // the normal case for a scene the editor has never shown.
    void restore(SceneView& view, const std::filesystem::path& scenePath);

    // Write the current view under scenePath. Returns true when anything was
    // written. force skips the "unchanged" short-circuit, needed on Save As where
    // the name changed but the camera did not.
    bool save(SceneView& view, const std::filesystem::path& scenePath, bool force = false);

    // Throttled idle save: at most once per 0.5s, and only while the mouse is up.
    // nowSeconds is the caller's ImGui::GetTime(), so this stays free of ImGui.
    bool maybeAutosave(SceneView& view, const std::filesystem::path& scenePath,
                       double nowSeconds, bool anyMouseDown);

private:
    // Last camera state written to disk, so save() can skip no-op writes and
    // maybeAutosave can throttle the idle save.
    ve::Vec3 m_savedCamPos{ 0.0 };
    float m_savedCamYaw = 0.0f;
    float m_savedCamPitch = 0.0f;
    double m_lastCameraSaveTime = 0.0;
};

}
