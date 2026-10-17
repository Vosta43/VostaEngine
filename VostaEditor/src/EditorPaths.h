#pragma once

#include "Core/AssetConfig.h"

#include <filesystem>

namespace ve {

// Where the open project keeps its scenes. Shared by the modules that list, load
// and save .veworld files.
inline std::filesystem::path scenesDir() {
    return std::filesystem::path(toProjectAbsolute("content/scenes"));
}

}
