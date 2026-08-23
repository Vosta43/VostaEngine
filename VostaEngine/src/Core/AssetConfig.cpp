#include "vepch.h"
#include "AssetConfig.h"
#include "Log.h"

#include <cassert>

namespace ve {

namespace {
    std::filesystem::path s_assetRoot;
    bool s_initialized = false;
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
    if (p.is_absolute()) {
        return p.lexically_normal().string();
    }
    return (s_assetRoot / p).lexically_normal().string();
}

std::string toRelative(const std::string& path) {
    if (path.empty()) return "";
    std::filesystem::path p(path);
    if (p.is_relative()) {
        // Already relative — assume it's asset-root-relative.
        return p.generic_string();
    }
    p = p.lexically_normal();
    auto rel = p.lexically_relative(s_assetRoot);
    if (!rel.empty() && rel.string().find("..") == 0) {
        // Path is outside the asset root (e.g. a file dialog selection).
        // TODO: implement copy-on-import to bring external files into the asset root.
        VE_CORE_WARN_PRINT("Path outside asset root, stored as-is: %s", path.c_str());
        return p.generic_string();
    }
    return rel.generic_string();
}

} // namespace ve
