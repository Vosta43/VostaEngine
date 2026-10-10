#include "vepch.h"
#include "Project.h"
#include "AssetConfig.h"
#include "Json.h"
#include "Log.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>

namespace ve {

namespace {
    constexpr const char* kMetaFileName   = "project.json";
    // Engine payload, as assembled by the VostaEngineSdk project. A new project
    // copies from here; see reference_shader_dir / project_content_root.
    constexpr const char* kSdkSubdir       = "SDK";
    constexpr const char* kTemplatesSubdir = "templates/GameProject";
    // The folders that make up a project's <project>/engine/ directory.
    constexpr const char* kPayloadDirs[] = { "include", "lib", "bin", "buildsystem", "VostaEngine" };

    std::filesystem::path metaPath(const std::filesystem::path& dir) {
        return dir / kMetaFileName;
    }

    bool readTextFile(const std::filesystem::path& path, std::string& out) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return true;
    }

    bool writeTextFile(const std::filesystem::path& path, const std::string& text) {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(out);
    }

    void replaceAll(std::string& s, const std::string& from, const std::string& to) {
        if (from.empty()) return;
        for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size())
            s.replace(pos, from.size(), to);
    }

    // A random v4 GUID in lowercase 8-4-4-4-12 form. VS accepts the bare form for
    // .slnx project ids and the braced form for ProjectGuid.
    std::string randomGuid4() {
        static std::mt19937_64 rng(std::random_device{}());
        unsigned char b[16];
        for (int i = 0; i < 16; i += 8) {
            const uint64_t r = rng();
            for (int j = 0; j < 8; ++j) b[i + j] = static_cast<unsigned char>(r >> (j * 8));
        }
        b[6] = static_cast<unsigned char>((b[6] & 0x0F) | 0x40);   // version 4
        b[8] = static_cast<unsigned char>((b[8] & 0x3F) | 0x80);   // variant 1

        static const char* hex = "0123456789abcdef";
        std::string s;
        s.reserve(36);
        for (int i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) s.push_back('-');
            s.push_back(hex[b[i] >> 4]);
            s.push_back(hex[b[i] & 0x0F]);
        }
        return s;
    }

    // A project name becomes a folder name, so reject anything that cannot be
    // one outright instead of failing halfway through writing files.
    bool isValidProjectName(const std::string& name) {
        if (name.empty() || name == "." || name == "..") return false;
        for (const unsigned char c : name) {
            if (c < 0x20) return false;
            if (std::string("\\/:*?\"<>|").find(static_cast<char>(c)) != std::string::npos)
                return false;
        }
        return name.back() != '.' && name.back() != ' ';
    }

    // The artifacts a new project cannot run without. SDK/ is assembled by a build
    // target, so an assembly that failed partway (or was never run for this
    // configuration) leaves a directory tree that looks plausible but is unusable.
    // Checking the files, not just the folders, is what stops that from becoming a
    // project whose F5 cannot launch.
    bool sdkPayloadReady(const std::filesystem::path& sdkRoot, std::string& missing) {
        static const char* kRequired[] = {
            "templates/GameProject/Game.vcxproj",
            "buildsystem/Vosta.GameModule.props",
            "lib/VostaEngine.lib",
            "bin/VostaEngine.dll",
            "bin/VostaPlayer.exe",
        };
        std::error_code ec;
        for (const char* rel : kRequired) {
            if (!std::filesystem::exists(sdkRoot / rel, ec)) {
                missing = rel;
                return false;
            }
        }
        return true;
    }

    // Copies the scaffold and the engine payload into a new project folder:
    //   <SDK>/templates/GameProject/**                   -> <dir>/         (Game.* renamed)
    //   <SDK>/{include,lib,bin,buildsystem,VostaEngine}  -> <dir>/engine/
    // All scaffold files are text, so placeholders are substituted during the
    // copy; the engine payload is copied verbatim.
    bool materializeProject(const std::filesystem::path& dir, const std::string& name) {
        const std::filesystem::path sdkRoot = getAssetRoot() / kSdkSubdir;
        const std::filesystem::path tmplDir = sdkRoot / kTemplatesSubdir;
        const std::string guid = randomGuid4();

        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(
            tmplDir, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code fileEc;
            if (!it->is_regular_file(fileEc)) continue;

            std::filesystem::path rel = std::filesystem::relative(it->path(), tmplDir, ec);
            if (ec) {
                VE_CORE_ERROR_PRINT("Project::create: could not resolve '%s'", it->path().string().c_str());
                return false;
            }
            if (rel.filename() == "Game.vcxproj")      rel.replace_filename(name + ".vcxproj");
            else if (rel.filename() == "Game.slnx")    rel.replace_filename(name + ".slnx");

            std::string text;
            if (!readTextFile(it->path(), text)) {
                VE_CORE_ERROR_PRINT("Project::create: could not read '%s'", it->path().string().c_str());
                return false;
            }
            // The only content reference to the renamed project file is the
            // solution's <Project Path>, which uses {{PROJECT_NAME}}.vcxproj.
            replaceAll(text, "{{PROJECT_NAME}}", name);
            replaceAll(text, "{{PROJECT_GUID_BRACED}}", "{" + guid + "}");
            replaceAll(text, "{{PROJECT_GUID}}", guid);

            const std::filesystem::path outPath = dir / rel;
            std::error_code mkEc;
            std::filesystem::create_directories(outPath.parent_path(), mkEc);
            if (!writeTextFile(outPath, text)) {
                VE_CORE_ERROR_PRINT("Project::create: could not write '%s'", outPath.string().c_str());
                return false;
            }
        }
        if (ec) {
            VE_CORE_ERROR_PRINT("Project::create: could not read template '%s' (%s)",
                                tmplDir.string().c_str(), ec.message().c_str());
            return false;
        }

        const std::filesystem::path engineDir = dir / "engine";
        // copy() creates each destination directory but not its parent, so the
        // engine folder itself must exist before the first payload lands in it.
        std::error_code engineMkEc;
        std::filesystem::create_directories(engineDir, engineMkEc);
        if (engineMkEc) {
            VE_CORE_ERROR_PRINT("Project::create: could not create '%s' (%s)",
                                engineDir.string().c_str(), engineMkEc.message().c_str());
            return false;
        }
        for (const char* sub : kPayloadDirs) {
            const std::filesystem::path from = sdkRoot / sub;
            std::error_code subEc;
            if (!std::filesystem::is_directory(from, subEc)) {
                VE_CORE_ERROR_PRINT("Project::create: engine payload '%s' missing under '%s'",
                                    sub, sdkRoot.string().c_str());
                return false;
            }
            std::filesystem::copy(from, engineDir / sub,
                                  std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing, subEc);
            if (subEc) {
                VE_CORE_ERROR_PRINT("Project::create: could not copy engine '%s' (%s)",
                                    sub, subEc.message().c_str());
                return false;
            }
        }
        return true;
    }
}

namespace Project {

std::string projectsDir() {
    return (getAssetRoot() / "Projects").string();
}

bool create(const std::string& parentDir, const std::string& name,
            bool withGameModule, ProjectInfo& out) {
    if (!isValidProjectName(name)) {
        VE_CORE_ERROR_PRINT("Project::create: '%s' is not a usable project name", name.c_str());
        return false;
    }
    if (parentDir.empty()) {
        VE_CORE_ERROR_PRINT("Project::create: location must not be empty");
        return false;
    }

    std::error_code ec;
    // Check the engine payload before touching the disk, so an incomplete SDK fails
    // without leaving a half-written project folder.
    if (withGameModule) {
        const std::filesystem::path sdkRoot = getAssetRoot() / kSdkSubdir;
        std::string missing;
        if (!sdkPayloadReady(sdkRoot, missing)) {
            VE_CORE_ERROR_PRINT("Project::create: engine SDK payload is incomplete, '%s' is missing under '%s'. "
                                "Build the VostaEngineSdk project, then try again.",
                                missing.c_str(), sdkRoot.string().c_str());
            return false;
        }
    }

    const std::filesystem::path dir =
        std::filesystem::path(parentDir).lexically_normal() / name;
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

    if (withGameModule && !materializeProject(dir, name)) {
        // The folder was created moments ago and is ours alone, so discard it
        // rather than leaving a project that cannot build.
        std::error_code rmEc;
        std::filesystem::remove_all(dir, rmEc);
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
