#pragma once

#include "Core.h"

#include <string>

namespace ve {

// Metadata for one project, persisted as project.json at the project folder
// root. A project folder also holds content/ (scenes + imported assets) and
// Saved/ (session state).
struct VE_API ProjectInfo {
    std::string name;
    std::string defaultScene;   // project-relative, may be empty
};

namespace Project {

    // Absolute path to the folder holding every project: <assetRoot>/Projects.
    VE_API std::string projectsDir();

    // Create <projectsDir>/<name>/ with content/scenes, Saved and project.json.
    // Fails if the name is empty or the folder already exists. Does not change
    // the current project root.
    VE_API bool create(const std::string& name, ProjectInfo& out);

    // Read project.json from a project folder (absolute path). Missing or
    // malformed metadata is tolerated: the name falls back to the folder name.
    VE_API bool open(const std::string& projectDir, ProjectInfo& out);

    // Rewrite project.json inside the current project root.
    VE_API bool save(const ProjectInfo& info);

} // namespace Project

} // namespace ve
