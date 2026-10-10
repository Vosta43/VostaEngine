#include "vepch.h"
#include "AssetConfig.h"
#include "Log.h"

#include <cassert>

namespace ve {

namespace {
    std::filesystem::path s_assetRoot;
    bool s_initialized = false;
    std::filesystem::path s_projectRoot;
} // namespace

void setAssetRoot(const std::string& absolutePath) {
    (void)absolutePath; // Unused in release (assert is compiled out)
    VE_CORE_ASSERT(!s_initialized, "setAssetRoot() must only be called once");
    s_assetRoot = std::filesystem::path(absolutePath).lexically_normal();
    s_initialized = true;
    VE_CORE_SUCCESS_PRINT("Asset root set to: %s", s_assetRoot.string().c_str());
}

const std::filesystem::path& getAssetRoot() {
    return s_assetRoot;
}

std::string toAbsolute(const std::string& path) {
    if (path.empty()) return "";
    std::filesystem::path p(path);
    if (p.is_absolute())
        return p.lexically_normal().string();
    // "content/..." is the open project's own content; every other key belongs
    // to the engine, whose root is fixed at startup.
    if (hasProjectRoot() && *p.begin() == "content")
        return (s_projectRoot / p).lexically_normal().string();
    return (s_assetRoot / p).lexically_normal().string();
}

std::string toRelative(const std::string& path) {
    if (path.empty()) return "";
    std::filesystem::path p(path);
    if (p.is_relative())
        return p.generic_string();
    p = p.lexically_normal();

    // Project content stays project-root-relative so a project is portable;
    // engine-owned assets stay asset-root-relative.
    if (hasProjectRoot()) {
        auto projRel = p.lexically_relative(s_projectRoot);
        if (!projRel.empty() && *projRel.begin() == "content")
            return projRel.generic_string();
    }

    auto rel = p.lexically_relative(s_assetRoot);
    if (!rel.empty() && rel.string().find("..") == 0) {
        // Path is outside the asset root (e.g. a file dialog selection).
        // TODO: implement copy-on-import to bring external files into the asset root.
        VE_CORE_WARN_PRINT("Path outside asset root, stored as-is: %s", path.c_str());
        return p.generic_string();
    }
    return rel.generic_string();
}

void setProjectRoot(const std::string& absolutePath) {
    s_projectRoot = absolutePath.empty()
        ? std::filesystem::path()
        : std::filesystem::path(absolutePath).lexically_normal();
    VE_CORE_SUCCESS_PRINT("Project root set to: %s",
                          s_projectRoot.empty() ? "(none)" : s_projectRoot.string().c_str());
}

const std::filesystem::path& getProjectRoot() {
    return s_projectRoot;
}

bool hasProjectRoot() {
    return !s_projectRoot.empty();
}

std::string toProjectAbsolute(const std::string& path) {
    if (path.empty()) return "";
    if (!hasProjectRoot()) {
        VE_CORE_WARN_PRINT("toProjectAbsolute: no project open, '%s' unresolved", path.c_str());
        return "";
    }
    std::filesystem::path p(path);
    if (p.is_absolute()) {
        return p.lexically_normal().string();
    }
    return (s_projectRoot / p).lexically_normal().string();
}

std::string toProjectRelative(const std::string& path) {
    if (path.empty()) return "";
    std::filesystem::path p(path);
    if (p.is_relative()) {
        return p.generic_string();
    }
    if (!hasProjectRoot()) {
        return p.lexically_normal().generic_string();
    }
    auto rel = p.lexically_normal().lexically_relative(s_projectRoot);
    if (rel.empty() || rel.string().find("..") == 0) {
        return p.lexically_normal().generic_string();
    }
    return rel.generic_string();
}

} // namespace ve
