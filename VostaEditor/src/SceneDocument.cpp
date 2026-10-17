#include "SceneDocument.h"

#include "AssetSaveRegistry.h"
#include "EditorPaths.h"

#include "Asset/Utils.h"
#include "Core/AssetConfig.h"
#include "Scene/Terrain/Terrain.h"
#include "Scene/Terrain/TerrainDataResource.h"

#include <fstream>

namespace ve {

namespace {

// Session state, not document content: which scene the editor had open. Lives
// under Saved/ so it can be gitignored.
std::filesystem::path engineSavedDir() {
    return getAssetRoot() / "Saved";
}

std::filesystem::path projectSavedDir() {
    return getProjectRoot() / "Saved";
}

std::string readLine(const std::filesystem::path& file) {
    std::ifstream in(file);
    std::string line;
    std::getline(in, line);
    return line;
}

void writeLine(const std::filesystem::path& file, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::trunc);
    if (out.is_open())
        out << text << "\n";
}

std::string readLastSceneName() {
    return hasProjectRoot() ? readLine(projectSavedDir() / "last_scene.txt") : "";
}

void writeLastSceneName(const std::string& name) {
    if (hasProjectRoot())
        writeLine(projectSavedDir() / "last_scene.txt", name);
}

std::string readLastProjectDir() {
    return readLine(engineSavedDir() / "last_project.txt");
}

void writeLastProjectDir(const std::string& dir) {
    writeLine(engineSavedDir() / "last_project.txt", dir);
}

}

void SceneDocument::bootstrap() {
    // Reopen the last project; fall back to the built-in SandBox project.
    const std::string last = readLastProjectDir();
    if (!last.empty() && Project::open(last, m_project)) {
        setProjectRoot(last);
    } else {
        const std::string sandbox = (getAssetRoot() / "SandBox").string();
        Project::open(sandbox, m_project);
        setProjectRoot(sandbox);
        writeLastProjectDir(sandbox);
    }

    // Prefills the save/load dialogs only; no scene is bound until one is opened,
    // so nothing camera-related keys off this.
    m_currentSceneName = readLastSceneName();
}

bool SceneDocument::apply(const std::string& projectDir) {
    if (!Project::open(projectDir, m_project))
        return false;

    setProjectRoot(projectDir);
    writeLastProjectDir(projectDir);
    return true;
}

void SceneDocument::setCurrent(std::string name, std::filesystem::path path) {
    m_currentSceneName = std::move(name);
    m_currentScenePath = std::move(path);
    writeLastSceneName(m_currentSceneName);
}

std::string SceneDocument::terrainDataPath(const std::filesystem::path& scenePath) {
    return (scenePath.parent_path() / (scenePath.stem().string() + "_terrain.veasset")).string();
}

bool SceneDocument::save(const Ref<Scene>& scene) {
    if (m_saveFileName.empty()) {
        VE_CORE_ERROR("No filename specified for saving!");
        return false;
    }

    if (m_saveFileName.find(".veworld") == std::string::npos) {
        m_saveFileName += ".veworld";
    }

    std::filesystem::path absolutePath = scenesDir() / m_saveFileName;
    std::filesystem::create_directories(absolutePath.parent_path());

    // Before the scene is written, so its JSON carries the terrain-data link.
    writeTerrainData(scene, absolutePath);

    SceneSerializer serializer(scene);
    if (!serializer.saveToFile(absolutePath.string())) {
        VE_CORE_ERROR("Failed to save scene to: %s", absolutePath.string().c_str());
        return false;
    }

    setCurrent(m_saveFileName, absolutePath);

    VE_CORE_SUCCESS_PRINT("Scene saved to: %s", absolutePath.string().c_str());
    return true;
}

void SceneDocument::writeTerrainData(const Ref<Scene>& scene, const std::filesystem::path& scenePath) {
    if (!scene)
        return;
    TerrainSubsystem terrain(*scene);
    TerrainSystemComponent* sys = terrain.system();
    if (!sys)
        return;

    Ref<TerrainDataResource> data = terrain.ensureData();
    if (!data)
        return;

    // A terrain that was never painted leaves no file and no reference.
    const std::string path = terrainDataPath(scenePath);
    if (!data->anyPainted() && !sys->terrainDataHandle.isValid())
        return;

    auto& storage = ResourceManager::getStorage<TerrainDataResource>();
    const std::string rel = toRelative(path);
    if (sys->terrainDataHandle.isValid()
        && ResourceManager::getPath<TerrainDataResource>(sys->terrainDataHandle) != rel) {
        // Save As renamed the scene; drop the stale registration.
        ResourceManager::remove<TerrainDataResource>(sys->terrainDataHandle);
    }
    sys->terrainDataHandle = storage.store(rel, data);

    data->write(path);
    data->bDirty = false;
    AssetSaveRegistry::get().markClean(path);
}

void SceneDocument::syncTerrainDataDirty(const Ref<Scene>& scene) {
    if (!scene)
        return;
    TerrainSubsystem terrain(*scene);
    TerrainSystemComponent* sys = terrain.system();
    if (!sys || !sys->terrainDataHandle.isValid())
        return;

    Ref<TerrainDataResource> data = terrain.data();
    if (!data || !data->bDirty)
        return;

    const std::string path = ResourceManager::getPath<TerrainDataResource>(sys->terrainDataHandle);
    AssetSaveRegistry::get().markDirty(path, [path, data] {
        data->write(toAbsolute(path));
        data->bDirty = false;
    });
}

}
