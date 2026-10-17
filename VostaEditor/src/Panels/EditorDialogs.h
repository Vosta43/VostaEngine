#pragma once

#include <VostaEngine.h>

#include <Build/Packager.h>
#include <Core/Project.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ve {

// The editor's modal dialogs: Save Scene, Load Scene, New Project, Open Project
// and Package Project. Owns their fields and visibility; the scene and project
// work they trigger crosses back into the layer through callbacks, the same way
// the panels signal. Packaging needs none -- it drives the engine's Packager
// service directly.
class EditorDialogs {
public:
    struct Callbacks {
        // Save the current scene under `name`. False when nothing was written.
        std::function<bool(const std::string& name)> onSaveScene;
        // Load the scene file `name` from the project's scenes folder.
        std::function<void(const std::string& name)> onLoadScene;
        // Switch the open project to the absolute folder `dir`.
        std::function<void(const std::string& dir)> onApplyProject;
    };
    void setCallbacks(Callbacks callbacks) { m_callbacks = std::move(callbacks); }

    // Refresh what the dialogs prefill: the scene they name by default and the
    // project label the Package dialog shows. Call it whenever the layer's
    // document changes.
    void setContext(const std::string& currentSceneName, const ProjectInfo& project);

    void openSave();
    void openLoad();
    void openNewProject();
    void openOpenProject();
    void openPackage();

    // Draws whichever modal is flagged open.
    void onGuiRender();

private:
    // One row in the Load dialog: a .veworld file found in the scenes folder.
    struct SceneFile {
        std::string name;
        uintmax_t size = 0;
    };

    void refreshSceneFileList();
    void refreshProjectList();

    Callbacks m_callbacks;

    // Save / Load.
    bool m_showSavePopup = false;
    bool m_showLoadPopup = false;
    std::string m_currentSceneName;
    std::string m_loadFileName;
    char m_saveFileNameBuffer[256] = "scene.veworld";
    char m_loadFileNameBuffer[256] = "";
    std::vector<SceneFile> m_sceneFiles;

    // New / Open Project.
    bool m_showNewProjectPopup = false;
    bool m_showOpenProjectPopup = false;
    char m_newProjectNameBuffer[256] = "NewProject";
    // Folder the new project is created in. Seeded with <assetRoot>/Projects the
    // first time the dialog opens, then editable to any absolute path.
    char m_newProjectLocationBuffer[512] = "";
    bool m_newProjectWithModule = true;
    std::vector<std::string> m_projectDirs;
    std::string m_openProjectDir;
    std::string m_projectName;

    // Package.
    bool m_showPackagePopup = false;
    bool m_packageRelease = true;
    std::string m_packageError;
    char m_packageOutputBuffer[512] = "";
    PackageState m_packageState;
};

}
