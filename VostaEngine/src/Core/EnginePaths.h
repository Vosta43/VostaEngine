#pragma once

#include "Core.h"
#include <filesystem>

namespace ve {

// Locates the engine installation root -- the directory that holds the engine's
// own resource tree (VostaEngine/resources/). Called once by a host at startup:
//
//   1. VOSTA_ENGINE_ROOT -- explicit configuration. The launcher states the
//                           root; required in the dev tree, whose build output
//                           layout is not a shipping layout.
//   2. <exeDir>/..        -- a self-contained install (the SDK), where the host
//                           binary sits one level below the root.
//
// The working directory is never consulted. Returns an empty path, after
// logging an error, when neither channel yields a valid root.
VE_API std::filesystem::path resolveEngineRoot();

} // namespace ve
