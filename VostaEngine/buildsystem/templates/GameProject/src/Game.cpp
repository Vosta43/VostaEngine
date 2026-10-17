// Gameplay module; the host loads this DLL from <projectRoot>/Binaries/game.dll.

#include <VostaEngine.h>

// onStart/onUpdate/onEnd bracket a play session; the scene ticks onUpdate.
class Game : public ve::GameModule {
public:
	void onStart(ve::Scene& scene) override {
		// Build your world here -- scene is the throwaway runtime world.
	}

	void onUpdate(ve::Scene& scene, float deltaTime) override {
		// Per-frame gameplay goes here.
	}

	void onEnd(ve::Scene& scene) override {}
};

VE_GAME_MODULE(Game)
