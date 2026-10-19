#pragma once

#include "Core/Core.h"

#include <string>

namespace ve {

// One packaging job: assemble the open project into a self-contained folder that
// runs by double-clicking <name>.exe -- the player, the engine runtime and its
// resources, the project's content, and, when the project has one, a Release
// gameplay module.
struct PackageOptions {
    std::string projectRoot;    // absolute project folder
    std::string outputDir;      // absolute parent; <outputDir>/<projectName>/ is created
    bool        release = true; // build the shipping module (Release) or a debug one
};

enum class PackageStage {
    Idle, BuildModule, CollectContent, AssembleRuntime, WriteMeta, Done, Failed
};

struct PackageState {
    PackageStage stage     = PackageStage::Idle;
    int          step      = 0;     // 1-based index of the stage in progress
    int          stepCount = 4;
    std::string  message;           // one line for the progress panel
    std::string  detail;            // result folder on Done, reason on Failed
    bool         running   = false;
};

namespace Packager {

// Starts a job on a worker thread; the caller polls snapshot() for progress.
// Returns false, with the reason in error, when a job is already running or the
// options are unusable.
VE_API bool start(const PackageOptions& options, std::string& error);

// Point-in-time copy of the job state. Safe to call every frame while a job runs.
VE_API PackageState snapshot();

} // namespace Packager

} // namespace ve
