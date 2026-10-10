#pragma once

#include "Core.h"

// Bump when the GameModule virtual interface changes in a way that is not
// append-only. A host refuses to load a module built against a different value.
#define VE_GAME_MODULE_API_VERSION 1

namespace ve {

	class Application;
	class Scene;

	// Lifecycle contract between a host (editor / player) and a game-module DLL.
	// Append-only: never reorder or change the existing virtuals -- the vtable
	// order IS the ABI (see VE_GAME_MODULE_API_VERSION).
	class VE_API GameModule {
	public:
		virtual ~GameModule() = default;

		// Reports the version this module was built against. The default reads
		// the header the module was compiled with, which is exactly what the
		// host needs to compare.
		virtual int apiVersion() const { return VE_GAME_MODULE_API_VERSION; }

		// Called once after the host's Application is fully constructed (GL
		// context live). Register components/systems and push gameplay layers.
		virtual void onLoad(Application& app) = 0;

		// Called before the module is destroyed and its DLL is freed. Pop and
		// delete every layer pushed in onLoad -- the layer stack does not own them.
		virtual void onUnload(Application& app) = 0;

		// Called when a play session starts, after onLoad. Register gameplay
		// systems on the runtime scene (and add any gameplay layers). The scene is
		// a throwaway duplicate of what the editor authored -- mutate it freely.
		virtual void onPlay(Scene& runtimeScene) {}

		// Called when the session stops, before onUnload. Detach anything bound to
		// runtimeScene in onPlay.
		virtual void onStop(Scene& runtimeScene) {}
	};

	typedef GameModule* (*VeCreateGameModuleFn)();
	typedef void (*VeDestroyGameModuleFn)(GameModule*);

} // namespace ve

// Export a module's C ABI entry points. extern "C" keeps the names undecorated
// so GetProcAddress can find them; returning a class *pointer* is C-compatible
// (a pointer is a POD, so no C4190). new/delete stay inside the module's heap.
#define VE_GAME_MODULE(ModuleClass)                                                     \
	extern "C" __declspec(dllexport) ve::GameModule* VeCreateGameModule() {             \
		return new ModuleClass();                                                       \
	}                                                                                   \
	extern "C" __declspec(dllexport) void VeDestroyGameModule(ve::GameModule* module) { \
		delete module;                                                                  \
	}
