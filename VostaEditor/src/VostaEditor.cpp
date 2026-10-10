#include <VostaEngine.h>
#include <Core/AssetConfig.h>
#include <Core/EnginePaths.h>

#include "EditorLayer.h"

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
    ve::setAssetRoot(ve::resolveEngineRoot().string());
    ve::VostaEditor* editor = new ve::VostaEditor();

    editor->run();
    delete editor;
}