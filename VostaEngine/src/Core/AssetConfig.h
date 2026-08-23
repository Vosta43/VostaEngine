#pragma once

#include "Core.h"
#include <string>
#include <filesystem>

namespace ve {

// Call once at startup with the absolute project root directory (the
// directory containing SandBox/, VostaEngine/, and VostaEditor/).
// Asserts if called more than once.
VE_API void setAssetRoot(const std::string& absolutePath);

// Returns the absolute project root path.
VE_API const std::filesystem::path& getAssetRoot();

// Resolves a project-relative path against the asset root to produce an
// absolute filesystem path. If the input is already absolute, returns it
// unchanged (lexically normalized).
VE_API std::string toAbsolute(const std::string& path);

// Strips the asset-root prefix from an absolute path to produce a
// project-relative path. If the path is not under the asset root, logs a
// warning and returns the path unchanged.
VE_API std::string toRelative(const std::string& path);

} // namespace ve
