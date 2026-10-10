#include "vepch.h"
#include "EnginePaths.h"
#include "Log.h"

#include <cstdlib>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#endif

namespace ve {

namespace {

std::filesystem::path executablePath() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) return {};
    return std::filesystem::path(buffer);
#else
    char buffer[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) return {};
    buffer[length] = '\0';
    return std::filesystem::path(buffer);
#endif
}

// The engine's resource tree is what separates a real root from any other
// directory, so the test is positive rather than a bare existence check.
bool isEngineRoot(const std::filesystem::path& dir) {
    return !dir.empty() && std::filesystem::exists(dir / "VostaEngine" / "resources");
}

// getenv is flagged as unsafe by the MSVC CRT; _dupenv_s is its sanctioned
// replacement, which keeps the engine build warning-clean.
std::string envVar(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return {};
    std::string result(value);
    std::free(value);
    return result;
#else
    if (const char* value = std::getenv(name)) return value;
    return {};
#endif
}

} // namespace

std::filesystem::path resolveEngineRoot() {
    const std::string configured = envVar("VOSTA_ENGINE_ROOT");
    if (!configured.empty()) {
        const std::filesystem::path fromEnv(configured);
        if (isEngineRoot(fromEnv)) {
            VE_CORE_SUCCESS_PRINT("Engine root from VOSTA_ENGINE_ROOT: %s", fromEnv.string().c_str());
            return fromEnv;
        }
        VE_CORE_WARN_PRINT("VOSTA_ENGINE_ROOT '%s' has no VostaEngine/resources; ignoring it", configured.c_str());
    }

    const std::filesystem::path exePath = executablePath();
    if (!exePath.empty()) {
        const std::filesystem::path candidate = exePath.parent_path().parent_path();
        if (isEngineRoot(candidate)) {
            VE_CORE_SUCCESS_PRINT("Engine root from executable location: %s", candidate.string().c_str());
            return candidate;
        }
    }

    VE_CORE_ERROR_PRINT("Engine root not found. Set VOSTA_ENGINE_ROOT to the directory "
                        "containing VostaEngine/resources.");
    return {};
}

} // namespace ve
