#include "vepch.h"
#include "Project.h"
#include "AssetConfig.h"
#include "Json.h"
#include "Log.h"

#include <filesystem>

namespace ve {

namespace {
    constexpr const char* kMetaFileName = "project.json";

    std::filesystem::path metaPath(const std::filesystem::path& dir) {
        return dir / kMetaFileName;
    }
}

namespace Project {

std::string projectsDir() {
    return (getAssetRoot() / "Projects").string();
}

bool create(const std::string& name, ProjectInfo& out) {
    if (name.empty()) {
        VE_CORE_ERROR_PRINT("Project::create: name must not be empty");
        return false;
    }

    const std::filesystem::path dir = std::filesystem::path(projectsDir()) / name;
    std::error_code ec;
    if (std::filesystem::exists(dir, ec)) {
        VE_CORE_ERROR_PRINT("Project::create: '%s' already exists", dir.string().c_str());
        return false;
    }

    std::filesystem::create_directories(dir / "content" / "scenes", ec);
    std::filesystem::create_directories(dir / "Saved", ec);
    if (ec) {
        VE_CORE_ERROR_PRINT("Project::create: could not create '%s' (%s)",
                            dir.string().c_str(), ec.message().c_str());
        return false;
    }

    out = ProjectInfo{};
    out.name = name;

    JsonWriter w;
    w.set("name", out.name);
    w.set("defaultScene", out.defaultScene);
    if (!w.writeToFile(metaPath(dir).string())) {
        VE_CORE_ERROR_PRINT("Project::create: could not write '%s'", metaPath(dir).string().c_str());
        return false;
    }

    VE_CORE_SUCCESS_PRINT("Project created at: %s", dir.string().c_str());
    return true;
}

bool open(const std::string& projectDir, ProjectInfo& out) {
    const std::filesystem::path dir = std::filesystem::path(projectDir).lexically_normal();
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        VE_CORE_ERROR_PRINT("Project::open: '%s' is not a directory", dir.string().c_str());
        return false;
    }

    JsonReader r;
    const bool haveMeta = JsonReader::load(metaPath(dir).string(), r) && r.valid();

    out = ProjectInfo{};
    out.name         = haveMeta ? r.getString("name", dir.filename().string())
                                : dir.filename().string();
    out.defaultScene = haveMeta ? r.getString("defaultScene", "") : "";
    return true;
}

bool save(const ProjectInfo& info) {
    if (!hasProjectRoot()) {
        VE_CORE_ERROR_PRINT("Project::save: no project open");
        return false;
    }
    JsonWriter w;
    w.set("name", info.name);
    w.set("defaultScene", info.defaultScene);
    if (!w.writeToFile(metaPath(getProjectRoot()).string())) {
        VE_CORE_ERROR_PRINT("Project::save: could not write project.json");
        return false;
    }
    return true;
}

} // namespace Project

} // namespace ve
