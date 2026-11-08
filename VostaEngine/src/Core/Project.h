#pragma once

#include "Core.h"

#include <string>
#include <vector>

namespace ve {

// Who draws the window backbuffer each frame: the engine's SceneView (Scene), or
// the gameplay module through its own renderer (Module). Only the host decides
// whether to create a SceneView at all; the Scene object exists either way.
enum class Presentation {
    Scene,
    Module,
};

// Metadata for one project, persisted as project.json at the project folder
// root. A project folder also holds content/ (scenes + imported assets) and
// Saved/ (session state).
struct VE_API ProjectInfo {
    std::string name;
    std::string defaultScene;   // project-relative, may be empty
    Presentation presentation = Presentation::Scene;
    std::string version;        // optional; names the package folder and build.json
    // Scene-presentation render pipeline, as an asset path ("content/..." is
    // project-local). Empty means the engine default; a menu/launcher project
    // points it at a leaner pipeline than the full deferred one.
    std::string pipeline;

    // What a packaged build carries, from project.json's "package" block.
    // Entries are project-relative files or folders; empty include means the
    // default packageRoots(). A project that keeps runtime data outside content/
    // (a module with its own layout) declares it here, because the engine cannot
    // derive those paths from the assets alone.
    std::vector<std::string> packageInclude;
    std::vector<std::string> packageExclude;
};

namespace Project {

    // "scene" / "module", the spelling used in project.json.
    VE_API const char* presentationName(Presentation presentation);
    VE_API Presentation parsePresentation(const std::string& name);

    // Absolute path to the folder holding every project: <assetRoot>/Projects.
    VE_API std::string projectsDir();

    // Create <parentDir>/<name>/ with content/scenes, Saved and project.json.
    // parentDir is an absolute folder and need not live inside the engine tree.
    // When withGameModule is set, also scaffold a native gameplay module from the
    // engine's GameProject templates: src/Game.cpp plus <name>.vcxproj/<name>.slnx,
    // a standalone solution that builds <projectRoot>/Binaries/game.dll and
    // reaches the engine through a path computed from the new project's location.
    // Fails if the name or the location is empty, or the folder already exists.
    // Does not change the current project root.
    VE_API bool create(const std::string& parentDir, const std::string& name,
                       bool withGameModule, ProjectInfo& out);

    // Read project.json from a project folder (absolute path). Missing or
    // malformed metadata is tolerated: the name falls back to the folder name.
    VE_API bool open(const std::string& projectDir, ProjectInfo& out);

    // The project-relative paths a package carries: package.include, or { "content" }
    // when the project declares none. A packaged build reproduces each of them at
    // the package root, so a module that resolves assets from the working
    // directory finds them once the player runs from there.
    VE_API std::vector<std::string> packageRoots(const ProjectInfo& project);

    // Rewrite project.json inside the current project root.
    VE_API bool save(const ProjectInfo& info);

} // namespace Project

} // namespace ve
