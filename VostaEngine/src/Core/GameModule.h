#pragma once

#include "Core.h"

// Bump when the GameModule virtual interface changes in a way that is not
// append-only. A host refuses to load a module built against a different value.
#define VE_GAME_MODULE_API_VERSION 2

namespace ve {

	class Application;
	class Scene;

	// A game module IS the game: one class, no layer to push. The host drives it.
	// onStart/onUpdate/onEnd bracket a play session, and onUpdate runs from the
	// runtime scene's own tick -- so a host that pauses the scene (the editor's
	// Play session) pauses the game with it.
	//
	// Append-only: never reorder or change the existing virtuals -- the vtable
	// order IS the ABI (see VE_GAME_MODULE_API_VERSION).
	class VE_API GameModule {
	public:
		virtual ~GameModule() = default;

		// Reports the version this module was built against. The default reads
		// the header the module was compiled with, which is exactly what the
		// host needs to compare.
		virtual int apiVersion() const { return VE_GAME_MODULE_API_VERSION; }

		// Optional module-wide setup, called once after the host's Application is
		// constructed (GL context live) and again before the DLL is freed.
		// Register component types or other global state here.
		virtual void onLoad(Application&) {}
		virtual void onUnload(Application&) {}

		// Play session. onStart runs once when a session begins -- the scene is a
		// throwaway duplicate of what the editor authored, so mutate it freely.
		// onUpdate runs every frame while the session is live. onEnd runs once when
		// it stops, before the scene goes away.
		virtual void onStart(Scene&) {}
		virtual void onUpdate(Scene&, float deltaTime) {}
		virtual void onEnd(Scene&) {}
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
