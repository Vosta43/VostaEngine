#pragma once

#include <VostaEngine.h>

#include <Core/Project.h>

#include <filesystem>
#include <string>

namespace ve {

// The open scene document and the project that contains it: which .veworld is
// bound, where it lives, the name a Save would write to, the project metadata,
// and the terrain-data file that travels beside the scene. Persistence and
// identity only -- no UI, no view, no camera.
//
// It never touches the editor camera: the order of camera saves around a scene
// switch (old path first, then flip, then the new path) is EditorLayer's
// business, not this class's.
class SceneDocument {
public:
    // Reopen the last project (or fall back to the built-in SandBox) and seed the
    // current scene name from that project's last-session record. No scene is
    // bound until one is opened, so nothing camera-related keys off the name.
    void bootstrap();

    // Project::open + setProjectRoot + remember as last. False when the folder is
    // not a project. Does NOT rebind the scene/view -- the caller does that.
    bool apply(const std::string& projectDir);

    const ProjectInfo& project() const { return m_project; }
    ProjectInfo& project() { return m_project; }

    const std::string& currentName() const { return m_currentSceneName; }
    const std::filesystem::path& currentPath() const { return m_currentScenePath; }
    // Bind a new document and record it as the last scene for the next session.
    void setCurrent(std::string name, std::filesystem::path path);

    const std::string& saveName() const { return m_saveFileName; }
    void setSaveName(std::string name) { m_saveFileName = std::move(name); }

    // Write the scene to <scenesDir>/<saveName>, normalizing the extension. Writes
    // the terrain-data file first so the scene JSON carries its link, then
    // serializes, then rebinds the name/path and the last-scene record.
    bool save(const Ref<Scene>& scene);

    // Mark the terrain-data file dirty for the File Browser's Save (N) button.
    void syncTerrainDataDirty(const Ref<Scene>& scene);

    // "<sceneStem>_terrain.veasset" next to the scene -- the one file a terrain's
    // painted weights are written to.
    static std::string terrainDataPath(const std::filesystem::path& scenePath);

private:
    // Write the painted weights and point the system's handle at the file.
    void writeTerrainData(const Ref<Scene>& scene, const std::filesystem::path& scenePath);

    std::string m_currentSceneName;
    std::filesystem::path m_currentScenePath;
    std::string m_saveFileName = "scene.veworld";
    ProjectInfo m_project;
};

}
