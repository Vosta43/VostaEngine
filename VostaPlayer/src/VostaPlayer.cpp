// VostaPlayer: the standalone runtime host. It opens a project, renders its
// default scene and drives the project's gameplay module
// (<projectRoot>/Binaries/game.dll) through the same GameModuleHost the editor's
// Play-in-Editor uses -- same DLL, same code path.

#include <VostaEngine.h>

#include <Asset/BuiltinReousrces.h>
#include <Core/AssetConfig.h>
#include <Core/EnginePaths.h>
#include <Core/GameModuleHost.h>
#include <Core/Layers/Layers.h>
#include <Core/Project.h>
#include <Renderer/SceneView.h>

#include <filesystem>

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

    // The host layer: owns the scene view and the module host. A module is
    // optional -- absent, the scene still loads and renders.
    class PlayerLayer : public ve::Layer {
    public:
        explicit PlayerLayer(std::filesystem::path projectDir)
            : m_projectDir(std::move(projectDir))
            , m_scene(ve::CreateRef<ve::Scene>())
            , m_sceneView(m_scene, playerViewConfig()) {
        }

        void onAttach() override {
            m_sceneView.onAttach();

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

            if (project.defaultScene.empty()) {
                VE_CORE_ERROR_PRINT("%s", "Player: no default scene set in project.json");
            } else {
                ve::SceneSerializer loader(m_scene);
                if (loader.loadFromFile(ve::toProjectAbsolute(project.defaultScene)))
                    VE_CORE_SUCCESS_PRINT("Player: loaded default scene '%s'", project.defaultScene.c_str());
                else
                    VE_CORE_ERROR_PRINT("Player: could not load default scene '%s'", project.defaultScene.c_str());
            }

            m_sceneView.setCameraSource(ve::SceneView::CameraSource::Scene);

            const std::string dllPath = (ve::getProjectRoot() / "Binaries" / "game.dll").string();
            if (m_host.load(ve::Application::get(), dllPath))
                m_host.module()->onPlay(*m_scene);
            else
                VE_CORE_WARN_PRINT("Player: running without gameplay: %s", m_host.lastError().c_str());
        }

        void onDetach() override {
            if (m_host.isLoaded()) {
                m_host.module()->onStop(*m_scene);
                m_host.unload();
            }
        }

        void onUpdate() override {
            auto* win = ve::Application::get().getWindow();
            const uint32_t width  = static_cast<uint32_t>(win->getWidth());
            const uint32_t height = static_cast<uint32_t>(win->getHeight());

            m_sceneView.setSize(width, height);
            m_sceneView.onUpdate(ve::DeltaTime::get().getDeltaTime());

            // The view renders offscreen; present it to the window backbuffer.
            if (auto framebuffer = m_sceneView.getFramebuffer())
                framebuffer->blitToDefault(width, height);
        }

    private:
        std::filesystem::path m_projectDir;
        ve::Ref<ve::Scene>    m_scene;
        ve::SceneView         m_sceneView;
        ve::GameModuleHost    m_host;
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

    // Run the project given on the command line, defaulting to the built-in one.
    const std::filesystem::path projectDir = argc > 1
        ? std::filesystem::absolute(argv[1])
        : (ve::getAssetRoot() / "SandBox");

    VostaPlayerApp* app = new VostaPlayerApp(projectDir);
    ve::Logger::Get().printAllToConsole();
    app->run();
    delete app;
}
