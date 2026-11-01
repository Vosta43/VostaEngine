#pragma once

#include "Core/Core.h"

#include <string>
#include <vector>

namespace ve {

// A finished child process.
struct ProcessResult {
    bool        started  = false;   // false when the process could not be launched
    int         exitCode = -1;
    std::string output;             // child stdout + stderr, combined
};

// Runs argv to completion, blocking the calling thread. argv[0] is the program
// (searched on PATH when it carries no directory), the rest are arguments, each
// quoted for the platform. The child's stdout and stderr are captured into the
// result rather than inherited, so callers surface output without owning a
// console. processDir sets the child's working directory ("" inherits the
// caller's).
//
// argv rather than one command-line string: the platform takes arguments as an
// argv when it spawns, so the string form would only push the quoting problem
// onto every caller.
//
// With wait false the child is created and left running: the call returns once
// the process exists, exitCode stays -1 and output is empty. That is the mode
// for launching something the user keeps using (a packaged game), as opposed to
// running a tool to completion (MSBuild).
//
// Implementations: Platform/Windows/ProcessWindows.cpp (CreateProcess) and
// Platform/Posix/ProcessPosix.cpp (fork/execvp). On POSIX a program that cannot
// be launched reports started and exits with code 127, carrying the reason in
// output.
VE_API ProcessResult runProcess(const std::vector<std::string>& argv,
                                const std::string& processDir = std::string(),
                                bool wait = true);

} // namespace ve
