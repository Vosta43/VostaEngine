#pragma once

#include "Core/Core.h"

#include <string>
#include <vector>

namespace ve {

// One packaging job: assemble the open project into a self-contained folder that
// runs by double-clicking <name>.exe -- the player, the engine runtime and its
// resources, the project's content, and, when the project has one, a Release
// gameplay module.
// How the payload is laid out in the output folder. Encryption is the remaining
// unimplemented mode, refused by name rather than silently writing plain data.
enum class PackMode {
    Loose,          // files copied as they are
    Pak,            // archived into a single package file
    PakEncrypted,   // archived and encrypted
};

struct PackageOptions {
    std::string projectRoot;    // absolute project folder
    std::string outputDir;      // absolute parent; <outputDir>/<projectName>[-<version>]/ is created
    // Run MSBuild on the project's .slnx first. Off by default: a user who just
    // built the project should not pay for the same build twice, so the existing
    // Binaries/game.dll ships as-is.
    bool        rebuildModule = false;
    bool        release = true; // build the shipping module (Release) or a debug one
    std::string version;        // optional; appended to the output folder name
    PackMode    mode = PackMode::Loose;
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

    // Console transcript: one line per stage, copy, warning and failure, oldest
    // first. The job appends under its own mutex, so a snapshot is consistent.
    std::vector<std::string> log;

    // References in the copied assets that the package does not carry (a path
    // outside the declared roots, an absolute path). The job still completes;
    // these are the failures the user would otherwise only see at runtime.
    std::vector<std::string> warnings;
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
