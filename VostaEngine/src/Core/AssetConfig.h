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

// ── Project root ────────────────────────────────────────────────────────
// The folder of the currently open project. Unlike the asset root this can be
// reassigned whenever another project is opened, so it is deliberately not
// guarded by the once-only assert. Empty when no project is open.

// Set the current project folder (absolute). Pass "" to close the project.
VE_API void setProjectRoot(const std::string& absolutePath);
VE_API const std::filesystem::path& getProjectRoot();
VE_API bool hasProjectRoot();

// Resolve a project-relative path against the project root. Returns "" when no
// project is open.
VE_API std::string toProjectAbsolute(const std::string& path);
// Strip the project-root prefix from an absolute path.
VE_API std::string toProjectRelative(const std::string& path);

} // namespace ve
