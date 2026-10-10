#include "vepch.h"
#include "GameModuleHost.h"
#include "Application.h"
#include "Log.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace ve {

#ifdef _WIN32

	namespace {

		std::string winErrorText(DWORD code) {
			char* buffer = nullptr;
			const DWORD len = ::FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				reinterpret_cast<LPSTR>(&buffer), 0, nullptr);

			std::string text = buffer ? std::string(buffer, len) : std::string("unknown error");
			if (buffer)
				::LocalFree(buffer);

			// Trim the trailing CR/LF that FormatMessage appends.
			while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' '))
				text.pop_back();
			return text;
		}

	} // namespace

	GameModuleHost::~GameModuleHost() {
		unload();
	}

	bool GameModuleHost::load(Application& app, const std::string& dllPath) {
		if (m_module) {
			m_lastError = "a module is already loaded";
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			return false;
		}

		// LOAD_WITH_ALTERED_SEARCH_PATH makes the module resolve its own
		// dependencies (VostaEngine.dll) relative to its own folder.
		HMODULE handle = ::LoadLibraryExA(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (!handle) {
			m_lastError = "LoadLibraryEx failed (" + winErrorText(::GetLastError()) + "): " + dllPath;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			return false;
		}

		auto create = reinterpret_cast<VeCreateGameModuleFn>(
			reinterpret_cast<void*>(::GetProcAddress(handle, "VeCreateGameModule")));
		auto destroy = reinterpret_cast<VeDestroyGameModuleFn>(
			reinterpret_cast<void*>(::GetProcAddress(handle, "VeDestroyGameModule")));

		if (!create || !destroy) {
			m_lastError = "module is missing its VE_GAME_MODULE export: " + dllPath;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			::FreeLibrary(handle);
			return false;
		}

		GameModule* module = nullptr;
		try {
			module = create();
		}
		catch (...) {
			module = nullptr;
		}

		if (!module) {
			m_lastError = "VeCreateGameModule returned null: " + dllPath;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			::FreeLibrary(handle);
			return false;
		}

		if (module->apiVersion() != VE_GAME_MODULE_API_VERSION) {
			m_lastError = "module API version mismatch (module=" + std::to_string(module->apiVersion())
				+ ", engine=" + std::to_string(VE_GAME_MODULE_API_VERSION) + "): " + dllPath;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			destroy(module);
			::FreeLibrary(handle);
			return false;
		}

		// Only commit the host state once onLoad has completed -- a throwing
		// module leaves the host empty rather than half-loaded.
		try {
			module->onLoad(app);
		}
		catch (...) {
			m_lastError = "module threw during onLoad: " + dllPath;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			destroy(module);
			::FreeLibrary(handle);
			return false;
		}

		m_osHandle = handle;
		m_module = module;
		m_destroyFn = destroy;
		m_app = &app;
		return true;
	}

	void GameModuleHost::unload() {
		if (m_module) {
			if (m_app) {
				try {
					m_module->onUnload(*m_app);
				}
				catch (...) {
					VE_CORE_ERROR_PRINT("module threw during onUnload");
				}
			}

			if (m_destroyFn)
				m_destroyFn(m_module);

			m_module = nullptr;
			m_destroyFn = nullptr;
			m_app = nullptr;
		}

		if (m_osHandle) {
			::FreeLibrary(static_cast<HMODULE>(m_osHandle));
			m_osHandle = nullptr;
		}
	}

#else // !_WIN32

	GameModuleHost::~GameModuleHost() {}

	bool GameModuleHost::load(Application&, const std::string&) {
		m_lastError = "GameModuleHost is not implemented on this platform yet";
		VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
		return false;
	}

	void GameModuleHost::unload() {}

#endif

} // namespace ve
