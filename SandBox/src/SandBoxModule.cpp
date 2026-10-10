// SandBox is the built-in project's gameplay module. A host -- the editor's Play
// session or VostaPlayer -- loads this DLL from <projectRoot>/Binaries/game.dll.
// SandBox no longer owns a window, loads scenes or renders; those are the host's
// job. Only gameplay lives here.

#include <VostaEngine.h>

#include <Core/Layers/Layers.h>
#include <Core/Log.h>
#include <Scene/Scene.h>

#include <glm.hpp>

namespace {

	// Log-only stand-in for real gameplay. Its presence proves the host drives a
	// layer pushed by a module; replace the body with systems over the scene.
	class SandBoxGameplayLayer : public ve::Layer {
	public:
		void onAttach() override {
			VE_CORE_INFO_PRINT("%s", "[SandBox] gameplay layer attached");

			// Login-panel demo: one white translucent panel, centred by the anchor
			// so it stays centred whatever the window size.
			auto root = ve::CreateRef<ve::vellum::Stack>();
			root->add(ve::CreateRef<ve::vellum::ColoredBox>(glm::vec4(1.0f, 1.0f, 1.0f, 0.60f), glm::vec2(360.0f, 440.0f)),
				glm::vec2(0.0f), glm::vec2(0.5f));

			// Multi-size demo: the same face at 48px gets its own atlas, filled on
			// demand the first time these glyphs are drawn.
			root->add(ve::CreateRef<ve::vellum::TextBlock>(
				ve::Application::get().getVellumLayer()->font(48.0f), "Vellum",
				glm::vec4(0.13f, 0.15f, 0.18f, 1.0f)),
				glm::vec2(0.0f, -140.0f), glm::vec2(0.5f));

			// Interactive demo: hover / press / click. Releasing off the rect cancels.
			// Anchored centre, nudged down so it sits near the panel's bottom.
			auto button = ve::CreateRef<ve::vellum::Button>(glm::vec2(200.0f, 52.0f), glm::vec4(0.16f, 0.42f, 0.72f, 1.0f));
			button->setOnClick([] { VE_CORE_INFO_PRINT("%s", "[SandBox] Vellum button clicked"); });
			button->addChild(ve::CreateRef<ve::vellum::TextBlock>(
				ve::Application::get().getVellumLayer()->defaultFont(), "Sign In"));
			root->add(button, glm::vec2(0.0f, 150.0f), glm::vec2(0.5f));

			ve::Application::get().getVellumLayer()->setRoot(root);
		}

		void onDetach() override {
			// The host's Vellum layer is shared; drop our tree so it does not
			// linger after the module unloads.
			ve::Application::get().getVellumLayer()->setRoot(nullptr);
			VE_CORE_INFO_PRINT("%s", "[SandBox] gameplay layer detached");
		}

		void onUpdate() override {
			// Log roughly once a second so the console stays readable.
			//if ((++m_frames % 60) == 1)
			//	VE_CORE_INFO_PRINT("[SandBox] frame %d", m_frames);
		}

	private:
		int m_frames = 0;
	};

	class SandBoxModule : public ve::GameModule {
	public:
		void onLoad(ve::Application& app) override {
			VE_CORE_INFO_PRINT("%s", "[SandBox] onLoad");
			m_layer = new SandBoxGameplayLayer();
			app.pushLayer(m_layer);
		}

		void onUnload(ve::Application& app) override {
			VE_CORE_INFO_PRINT("%s", "[SandBox] onUnload");
			if (m_layer) {
				app.popLayer(m_layer);
				delete m_layer;   // Layer's destructor is virtual
				m_layer = nullptr;
			}
		}

		void onPlay(ve::Scene& scene) override {
			VE_CORE_INFO_PRINT("[SandBox] onPlay, entities=%zu",
				scene.getRegistry().each().size());
		}

		void onStop(ve::Scene& scene) override {
			VE_CORE_INFO_PRINT("[SandBox] onStop, entities=%zu",
				scene.getRegistry().each().size());
		}

	private:
		SandBoxGameplayLayer* m_layer = nullptr;
	};

} // namespace

VE_GAME_MODULE(SandBoxModule)
