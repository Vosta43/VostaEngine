// VostaPlayer: the standalone runtime host. It opens a project, drives its
// gameplay module (<projectRoot>/Binaries/game.dll) through the same
// GameModuleHost the editor's Play-in-Editor uses -- same DLL, same code path --
// and either renders the default scene itself or hands the window to the module,
// as project.json's "presentation" asks.

#include <VostaEngine.h>

#include <Asset/BuiltinReousrces.h>
#include <Core/AssetConfig.h>
#include <Core/EnginePaths.h>
#include <Core/GameModuleHost.h>
#include <Core/Layers/Layers.h>
#include <Core/Project.h>
#include <Core/Vfs.h>
#include <Renderer/SceneView.h>

#include <filesystem>

// Hybrid laptops send this process to the discrete GPU. OpenGL ignores the
// Windows per-app graphics preference under some driver combinations -- the
// context silently lands on the integrated adapter -- and both vendors honour
// these exports instead.
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {

    // Same scene scaffolding the editor and the old SandBox host used. The asset
    // paths are engine-root-relative (resolved through the asset root).
    ve::SceneViewConfig playerViewConfig() {
        ve::SceneViewConfig cfg;
        cfg.width        = 1280;
        cfg.height       = 720;
        cfg.pipelinePath = "VostaEngine/resources/pipelines/default.json";
        cfg.perspective  = true;
        cfg.sceneShaders = { "VostaEngine/resources/shaders/Model.glsl",
                             "VostaEngine/resources/shaders/Texture.glsl" };
        return cfg;
    }

    // The host layer: owns the scene, the optional scene view and the module
    // host. The view is built after Project::open, since presentation decides
    // whether there is one at all.
    class PlayerLayer : public ve::Layer {
    public:
        explicit PlayerLayer(std::filesystem::path projectDir)
            : m_projectDir(std::move(projectDir))
            , m_scene(ve::CreateRef<ve::Scene>()) {
        }

        void onAttach() override {
            ve::ProjectInfo project;
            const std::string dir = m_projectDir.string();
            if (!ve::Project::open(dir, project))
                VE_CORE_ERROR_PRINT("Player: not a project directory: %s", dir.c_str());
            ve::setProjectRoot(dir);

            // A saved scene references built-ins by key; register them before
            // loading so those keys resolve (a mesh/texture needs the GL context,
            // live by onAttach).
            ve::BuiltinResources::getBuiltinSphere();
            ve::BuiltinResources::getDefaultWhiteTexture();

            // A module that presents itself owns the window: no view, no scene
            // file, and the depth convention is the module's to pick. The Scene
            // object still exists, because the module API is expressed in terms
            // of it.
            if (project.presentation == ve::Presentation::Scene) {
                m_sceneView = ve::CreateRef<ve::SceneView>(m_scene, playerViewConfig());
                m_sceneView->onAttach();

                if (project.defaultScene.empty()) {
                    VE_CORE_ERROR_PRINT("%s", "Player: no default scene set in project.json");
                } else {
                    ve::SceneSerializer loader(m_scene);
                    if (loader.loadFromFile(ve::toProjectAbsolute(project.defaultScene)))
                        VE_CORE_SUCCESS_PRINT("Player: loaded default scene '%s'", project.defaultScene.c_str());
                    else
                        VE_CORE_ERROR_PRINT("Player: could not load default scene '%s'", project.defaultScene.c_str());
                }

                m_sceneView->setCameraSource(ve::SceneView::CameraSource::Scene);
            }

            const std::string dllPath = (ve::getProjectRoot() / "Binaries" / "game.dll").string();
            if (m_host.load(ve::Application::get(), dllPath)) {
                if (!m_host.start(*m_scene))
                    VE_CORE_WARN_PRINT("Player: running without gameplay: %s", m_host.lastError().c_str());
            }
            else
                VE_CORE_WARN_PRINT("Player: running without gameplay: %s", m_host.lastError().c_str());
        }

        void onDetach() override {
            if (m_host.isLoaded()) {
                m_host.stop(*m_scene);
                m_host.unload();
            }
            m_sceneView = nullptr;
        }

        void onUpdate() override {
            // Tick gameplay before rendering so this frame shows its result.
            m_scene->onUpdate(ve::DeltaTime::get().getDeltaTime());

            if (!m_sceneView) return;

            auto* win = ve::Application::get().getWindow();
            const uint32_t width  = static_cast<uint32_t>(win->getWidth());
            const uint32_t height = static_cast<uint32_t>(win->getHeight());

            m_sceneView->setSize(width, height);
            m_sceneView->onUpdate(ve::DeltaTime::get().getDeltaTime());

            // The view renders offscreen; present it to the window backbuffer.
            if (auto framebuffer = m_sceneView->getFramebuffer())
                framebuffer->blitToDefault(width, height);
        }

    private:
        std::filesystem::path     m_projectDir;
        ve::Ref<ve::Scene>        m_scene;
        ve::Ref<ve::SceneView>    m_sceneView;
        ve::GameModuleHost        m_host;
    };

    class VostaPlayerApp : public ve::Application {
    public:
        explicit VostaPlayerApp(const std::filesystem::path& projectDir) {
            pushLayer(new PlayerLayer(projectDir));
        }
    };

} // namespace

int main(int argc, char** argv) {
    ve::setAssetRoot(ve::resolveEngineRoot().string());

    // A packaged build is its own project root: project.json sits beside
    // VostaEngine/resources, so a double-click launches it with no arguments.
    // Otherwise run the project named on the command line, defaulting to the
    // built-in SandBox.
    const std::filesystem::path projectDir = argc > 1
        ? std::filesystem::absolute(argv[1])
        : (std::filesystem::exists(ve::getAssetRoot() / "project.json")
               ? ve::getAssetRoot()
               : ve::getAssetRoot() / "SandBox");

    // A module-owned layout resolves its data from the working directory, so the
    // project folder must be the cwd rather than wherever the player happened to
    // be launched from. A packaged build gets this for free (the exe sits at the
    // package root); this makes the assumption explicit for every other launch.
    std::error_code cwdEc;
    std::filesystem::current_path(projectDir, cwdEc);
    if (cwdEc)
        VE_CORE_WARN_PRINT("Player: could not switch to the project folder (%s)",
                           cwdEc.message().c_str());

    // A PAK build ships its content as <project name>.pak beside the exe. Mount
    // before any layer runs: once a scene or asset loads, lookups check the
    // archive first. A loose build simply has no archive and stays untouched.
    {
        ve::ProjectInfo project;
        if (ve::Project::open(projectDir.string(), project)) {
            const std::filesystem::path archive =
                projectDir / (project.name + ".pak");
            std::error_code pakEc;
            if (std::filesystem::exists(archive, pakEc)) {
                std::string error;
                if (ve::vfs::mountPak(archive.string(), &error))
                    VE_CORE_SUCCESS_PRINT("Player: mounted %s",
                                          archive.filename().string().c_str());
                else
                    VE_CORE_ERROR_PRINT("Player: cannot mount %s (%s)",
                                        archive.filename().string().c_str(),
                                        error.c_str());
            }
        }
    }

    VostaPlayerApp* app = new VostaPlayerApp(projectDir);
    ve::Logger::Get().printAllToConsole();
    app->run();
    delete app;
}
