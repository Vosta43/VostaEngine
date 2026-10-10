// Gameplay module. The host -- the editor's Play session or VostaPlayer -- loads
// this DLL from <projectRoot>/Binaries/game.dll.

#include <VostaEngine.h>

#include <Core/Layers/Layers.h>
#include <Core/Log.h>
#include <Scene/Scene.h>

namespace {

	class GameLayer : public ve::Layer {
	public:
		void onAttach() override {
			VE_CORE_INFO_PRINT("%s", "[{{PROJECT_NAME}}] gameplay layer attached");
		}
	};

	class GameModule : public ve::GameModule {
	public:
		void onLoad(ve::Application& app) override {
			m_layer = new GameLayer();
			app.pushLayer(m_layer);
		}

		void onUnload(ve::Application& app) override {
			if (m_layer) {
				app.popLayer(m_layer);
				delete m_layer;   // Layer's destructor is virtual
				m_layer = nullptr;
			}
		}

		void onPlay(ve::Scene& scene) override {
			VE_CORE_INFO_PRINT("[{{PROJECT_NAME}}] onPlay, entities=%zu",
				scene.getRegistry().each().size());
		}

		void onStop(ve::Scene& scene) override {}

	private:
		GameLayer* m_layer = nullptr;
	};

} // namespace

VE_GAME_MODULE(GameModule)
