#include <VostaEngine.h>
#include <Core/AssetConfig.h>

#include "EditorLayer.h"

#include <filesystem>

// Walk up from current_path() until we find a directory that contains both
// VostaEngine/ and SandBox/ — that is the project root.
static std::filesystem::path findProjectRoot() {
    std::filesystem::path dir = std::filesystem::current_path();
    while (!dir.empty() && dir != dir.root_path()) {
        if (std::filesystem::exists(dir / "VostaEngine") &&
            std::filesystem::exists(dir / "SandBox")) {
            return dir;
        }
        dir = dir.parent_path();
    }
    return std::filesystem::current_path();
}

namespace ve {
    class VostaEditor : public Application {
    public:
        VostaEditor() {
            splashScreen(true);
            pushLayer(new EditorLayer());
            splashScreen(false);
        }
    };
}
int main() {
    ve::setAssetRoot(findProjectRoot().string());
    ve::VostaEditor* editor = new ve::VostaEditor();

    editor->run();
    delete editor;
}