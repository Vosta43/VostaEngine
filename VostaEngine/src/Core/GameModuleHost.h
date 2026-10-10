#pragma once

#include "Core.h"
#include "GameModule.h"

#include <string>

namespace ve {

	class Application;

	// Loads one game-module DLL and drives its GameModule lifecycle. One module
	// per host. Windows-only for now (LoadLibraryEx). Every failure path logs and
	// returns false -- a bad module must never crash the host.
	class VE_API GameModuleHost {
	public:
		GameModuleHost() = default;
		~GameModuleHost();

		GameModuleHost(const GameModuleHost&) = delete;
		GameModuleHost& operator=(const GameModuleHost&) = delete;

		// Loads dllPath, resolves the factory, version-checks it and calls
		// GameModule::onLoad(app). Remembers &app for the matching unload().
		bool load(Application& app, const std::string& dllPath);

		// Calls GameModule::onUnload, destroys the module and frees the DLL.
		// No-op when nothing is loaded.
		void unload();

		bool isLoaded() const { return m_module != nullptr; }
		GameModule* module() const { return m_module; }
		const std::string& lastError() const { return m_lastError; }

	private:
		void* m_osHandle = nullptr;          // HMODULE
		GameModule* m_module = nullptr;
		Application* m_app = nullptr;
		VeDestroyGameModuleFn m_destroyFn = nullptr;
		std::string m_lastError;
	};

} // namespace ve
