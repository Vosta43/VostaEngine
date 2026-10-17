#include "EditorCameraStore.h"

#include "Core/Json.h"

namespace ve {

namespace {

// "terrain1.veworld" -> "terrain1.veworld.camera.json".
std::filesystem::path editorCameraFile(const std::filesystem::path& scenePath) {
    return std::filesystem::path(scenePath.string() + ".camera.json");
}

bool readEditorCamera(const std::filesystem::path& scenePath, Camera& cam, float& moveSpeed) {
    if (scenePath.empty())
        return false;
    const std::filesystem::path file = editorCameraFile(scenePath);
    // Absent is the normal case for a scene the editor has never shown, so do
    // not let JsonReader log it as an error.
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
        return false;

    JsonReader r;
    if (!JsonReader::load(file.string(), r) || !r.valid())
        return false;
    // Read the position at authoritative precision: the writer stores it as
    // doubles so a planet-scale editor view survives a round-trip.
    cam.setPosition(ve::Vec3(r.getVec3d("position", glm::dvec3(cam.getPosition()))));
    cam.setYawPitch(r.getFloat("yaw", cam.getYaw()), r.getFloat("pitch", cam.getPitch()));
    moveSpeed = r.getFloat("moveSpeed", moveSpeed);
    return true;
}

void writeEditorCamera(const std::filesystem::path& scenePath, const Camera& cam, float moveSpeed) {
    if (scenePath.empty())
        return;
    // JsonWriter's root is already an object; beginObject() with no key appends
    // to an enclosing array and throws here.
    JsonWriter w;
    w.set("position", cam.getPosition());
    w.set("yaw", cam.getYaw());
    w.set("pitch", cam.getPitch());
    w.set("moveSpeed", moveSpeed);
    w.writeToFile(editorCameraFile(scenePath).string());
}

}

void EditorCameraStore::restore(SceneView& view, const std::filesystem::path& scenePath) {
    auto& cam = view.getEditorCamera();
    float speed = view.getCameraController().getMoveSpeed();
    readEditorCamera(scenePath, cam, speed);
    view.getCameraController().setMoveSpeed(speed);

    // Remember what is on disk so the idle save does not immediately rewrite an
    // unchanged camera.
    m_savedCamPos = cam.getPosition();
    m_savedCamYaw = cam.getYaw();
    m_savedCamPitch = cam.getPitch();
}

bool EditorCameraStore::save(SceneView& view, const std::filesystem::path& scenePath, bool force) {
    auto& cam = view.getEditorCamera();
    if (!force && cam.getPosition() == m_savedCamPos
        && cam.getYaw() == m_savedCamYaw && cam.getPitch() == m_savedCamPitch)
        return false;

    writeEditorCamera(scenePath, cam, view.getCameraController().getMoveSpeed());
    m_savedCamPos = cam.getPosition();
    m_savedCamYaw = cam.getYaw();
    m_savedCamPitch = cam.getPitch();
    return true;
}

bool EditorCameraStore::maybeAutosave(SceneView& view, const std::filesystem::path& scenePath,
                                      double nowSeconds, bool anyMouseDown) {
    // Gated on a bound scene: with none open the viewport is a scratch pad, and
    // its camera must not be written over some other scene's saved view.
    // Throttled, and only while the mouse is idle so an in-progress fly-around or
    // drag does not hammer the disk.
    if (scenePath.empty() || anyMouseDown || nowSeconds - m_lastCameraSaveTime <= 0.5)
        return false;
    m_lastCameraSaveTime = nowSeconds;
    return save(view, scenePath);
}

}
