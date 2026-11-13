#include "vepch.h"
#include "GameModuleHost.h"
#include "Application.h"
#include "Log.h"
#include "Network/RepLayout.h"
#include "Network/Rpc.h"
#include "Scene/Scene.h"

#include <unordered_map>

#if defined(VE_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ve {

	namespace {

#if defined(VE_PLATFORM_WINDOWS)

		std::string platformErrorText() {
			const DWORD code = ::GetLastError();
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

		void* openModule(const std::string& path) {
			// LOAD_WITH_ALTERED_SEARCH_PATH makes the module resolve its own
			// dependencies (VostaEngine.dll) relative to its own folder.
			return ::LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		}

		void* findExport(void* handle, const char* name) {
			return reinterpret_cast<void*>(
				::GetProcAddress(static_cast<HMODULE>(handle), name));
		}

		void closeModule(void* handle) {
			::FreeLibrary(static_cast<HMODULE>(handle));
		}

#else

		std::string platformErrorText() {
			const char* error = ::dlerror();
			return error ? std::string(error) : std::string("unknown error");
		}

		void* openModule(const std::string& path) {
			// RTLD_NOW: a module with an unresolved symbol fails here, at load
			// time, rather than at the first call into it.
			return ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
		}

		void* findExport(void* handle, const char* name) {
			return ::dlsym(handle, name);
		}

		void closeModule(void* handle) {
			::dlclose(handle);
		}

#endif

		// Both tables hold pointers into the module's code: layout getters and RPC
		// handlers. Drop them while the DLL is still mapped.
		void clearModuleRegistries() {
			rpc::RpcRegistry::get().reset();
			RepLayoutRegistry::get().reset();
		}

		// A module's replicated properties enter the reflection tables during
		// LoadLibrary or onLoad, so the layout table is rebuilt after a load.
		void rebuildRepLayouts() {
			std::vector<std::string> errors;
			if (RepLayoutRegistry::get().rebuild(errors)) {
				VE_CORE_SUCCESS("Replication layouts rebuilt: "
					<< RepLayoutRegistry::get().layoutCount() << " classes");
			}
			else {
				for (const std::string& error : errors)
					VE_CORE_ERROR_PRINT("RepLayout: %s", error.c_str());
			}

			// Argument ops resolve against the type table rebuilt above.
			rpc::RpcRegistry::get().finalize();
		}

		// Bridges the scene's tick to the module: gameplay runs from
		// Scene::onUpdate, so a host that pauses the scene pauses the game.
		class GameplayTickSystem : public ECSystemBase {
		public:
			GameplayTickSystem(GameModule& module, Scene& scene)
				: m_module(module), m_scene(scene) {}

			void onUpdate(float deltaTime) override {
				m_module.onUpdate(m_scene, deltaTime);
			}

		private:
			GameModule& m_module;
			Scene&      m_scene;
		};

	} // namespace

	// One mapped DLL, shared by every host that loads it. The reflection and RPC
	// tables are process-wide, so they must survive as long as any module
	// instance does: a world that stops must not clear another world's handlers.
	// The registries are dropped in the destructor, i.e. when the last host that
	// loaded this path releases its reference.
	class GameModuleLibrary {
	public:
		~GameModuleLibrary() {
			// Pointers into the module's code are dropped while it is still
			// mapped; the Vellum tree it built goes first for the same reason.
			clearModuleRegistries();
			if (m_app)
				m_app->clearGameUi();
			if (m_osHandle)
				closeModule(m_osHandle);
		}

		GameModuleLibrary(const GameModuleLibrary&) = delete;
		GameModuleLibrary& operator=(const GameModuleLibrary&) = delete;

		static std::shared_ptr<GameModuleLibrary> acquire(Application& app,
			const std::string& dllPath, std::string& error) {
			auto& cache = residency();
			if (auto it = cache.find(dllPath); it != cache.end()) {
				if (std::shared_ptr<GameModuleLibrary> resident = it->second.lock())
					return resident;
			}

			auto library = std::shared_ptr<GameModuleLibrary>(new GameModuleLibrary());
			library->m_path = dllPath;
			library->m_app = &app;

			void* handle = openModule(dllPath);
			if (!handle) {
				error = "failed to open module (" + platformErrorText() + "): " + dllPath;
				return nullptr;
			}

			auto create = reinterpret_cast<VeCreateGameModuleFn>(findExport(handle, "VeCreateGameModule"));
			auto destroy = reinterpret_cast<VeDestroyGameModuleFn>(findExport(handle, "VeDestroyGameModule"));

			if (!create || !destroy) {
				error = "module is missing its VE_GAME_MODULE export: " + dllPath;
				closeModule(handle);
				return nullptr;
			}

			library->m_osHandle = handle;
			library->m_createFn = create;
			library->m_destroyFn = destroy;
			cache[dllPath] = library;
			return library;
		}

		GameModule* createInstance(std::string& error) {
			GameModule* module = nullptr;
			try {
				module = m_createFn();
			}
			catch (...) {
				module = nullptr;
			}

			if (!module) {
				error = "VeCreateGameModule returned null: " + m_path;
				return nullptr;
			}

			if (module->apiVersion() != VE_GAME_MODULE_API_VERSION) {
				error = "module API version mismatch (module=" + std::to_string(module->apiVersion())
					+ ", engine=" + std::to_string(VE_GAME_MODULE_API_VERSION) + "): " + m_path;
				m_destroyFn(module);
				return nullptr;
			}
			return module;
		}

		void destroyInstance(GameModule* module) {
			if (module && m_destroyFn)
				m_destroyFn(module);
		}

		// The layout table cannot be rebuilt until onLoad has registered the
		// module's properties; only the first world loading this DLL does it.
		void ensureLayoutsBuilt() {
			if (m_layoutsBuilt)
				return;
			m_layoutsBuilt = true;
			rebuildRepLayouts();
		}

	private:
		GameModuleLibrary() = default;

		static std::unordered_map<std::string, std::weak_ptr<GameModuleLibrary>>& residency() {
			static std::unordered_map<std::string, std::weak_ptr<GameModuleLibrary>> instance;
			return instance;
		}

		std::string m_path;
		void* m_osHandle = nullptr;                 // HMODULE or dlopen handle
		Application* m_app = nullptr;
		VeCreateGameModuleFn m_createFn = nullptr;
		VeDestroyGameModuleFn m_destroyFn = nullptr;
		bool m_layoutsBuilt = false;
	};

	GameModuleHost::~GameModuleHost() {
		unload();
	}

	bool GameModuleHost::load(Application& app, const std::string& dllPath) {
		if (m_module) {
			m_lastError = "a module is already loaded";
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			return false;
		}

		std::string error;
		m_library = GameModuleLibrary::acquire(app, dllPath, error);
		if (!m_library) {
			m_lastError = error;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			return false;
		}

		m_module = m_library->createInstance(error);
		if (!m_module) {
			m_lastError = error;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			m_library.reset();
			return false;
		}

		// Only commit the host state once onLoad has completed -- a throwing
		// module leaves the host empty rather than half-loaded.
		try {
			m_module->onLoad(app);
		}
		catch (...) {
			m_lastError = "module threw during onLoad: " + dllPath;
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());

			// Let the module undo whatever onLoad managed to do (e.g. pop a layer
			// it pushed) before we drop it -- otherwise that layer lingers in the
			// app pointing into unmapped code. Its cleanup may throw too; swallow
			// that so we still unload.
			try {
				m_module->onUnload(app);
			}
			catch (...) {
				VE_CORE_ERROR_PRINT("%s", "module threw during onUnload after a failed onLoad");
			}

			m_library->destroyInstance(m_module);
			m_module = nullptr;
			m_library.reset();
			return false;
		}

		m_app = &app;
		m_library->ensureLayoutsBuilt();
		return true;
	}

	bool GameModuleHost::start(Scene& scene) {
		if (!m_module) {
			m_lastError = "start() called with no module loaded";
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			return false;
		}
		if (m_tick) {
			m_lastError = "a play session is already running";
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			return false;
		}

		// The tick lives on the scene, so the scene's update drives gameplay.
		m_tick = CreateRef<GameplayTickSystem>(*m_module, scene);
		m_scene = &scene;
		scene.addSystem(m_tick);

		try {
			m_module->onStart(scene);
		}
		catch (...) {
			m_lastError = "module threw during onStart";
			VE_CORE_ERROR_PRINT("%s", m_lastError.c_str());
			scene.removeSystem(m_tick);
			m_tick.reset();
			m_scene = nullptr;
			return false;
		}
		return true;
	}

	void GameModuleHost::stop(Scene& scene) {
		if (!m_tick)
			return;

		if (m_module) {
			try {
				m_module->onEnd(scene);
			}
			catch (...) {
				VE_CORE_ERROR_PRINT("%s", "module threw during onEnd");
			}
		}

		scene.removeSystem(m_tick);
		m_tick.reset();
		m_scene = nullptr;
	}

	void GameModuleHost::unload() {
		// A session left running (stop() never called): drop the tick before the
		// module goes away, so nothing dangles into the freed DLL.
		if (m_tick) {
			if (m_scene)
				m_scene->removeSystem(m_tick);
			m_tick.reset();
			m_scene = nullptr;
		}

		if (m_module) {
			if (m_app) {
				try {
					m_module->onUnload(*m_app);
				}
				catch (...) {
					VE_CORE_ERROR_PRINT("module threw during onUnload");
				}
			}

			// Layout getters and RPC handlers point into the module's code. The
			// shared library drops them only when the last host releases it, so a
			// sibling world that is still running keeps its handlers.
			if (m_library)
				m_library->destroyInstance(m_module);

			m_module = nullptr;
			m_app = nullptr;
		}

		m_library.reset();
	}

} // namespace ve
