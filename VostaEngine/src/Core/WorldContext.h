#pragma once

#include "Core.h"
#include "GameModuleHost.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ve {

	class Application;
	class Scene;
	class NetSession;

	// One running world: a scene copy, an optional gameplay-module instance and an
	// optional network session. A host that owns several (the editor's multi-world
	// PIE) decides the order they tick in; each tick brackets itself so module
	// code resolves the network, RPC session and input to this world only.
	class VE_API WorldContext {
	public:
		enum class Mode { Offline, ListenServer, Client };

		struct Config {
			Mode mode = Mode::Offline;
			std::string modulePath;                 // empty = run with no gameplay
			std::string address = "127.0.0.1";
			uint16_t port = 7777;
			int localPlayerIndex = 0;               // server/offline = 0, clients = 1..
		};

		WorldContext();
		~WorldContext();

		WorldContext(const WorldContext&) = delete;
		WorldContext& operator=(const WorldContext&) = delete;

		// Loads the module first, then opens the network: the module's replicated
		// properties must be registered before the session snapshots a layout.
		// Returns false with *error set; the world is left Idle.
		bool start(Application& app, Ref<Scene> scene, const Config& config, std::string* error);
		void stop();

		void tick(float deltaTime);

		void setPaused(bool paused) { m_paused = paused; }
		bool paused() const { return m_paused; }
		// One fixed step on the next tick, then back to paused.
		void requestStep() { m_stepOnce = true; }

		void setFocused(bool focused) { m_focused = focused; }
		bool isFocused() const { return m_focused; }

		bool isLive() const { return m_state == State::Live; }
		bool hasModule() const { return m_host.isLoaded(); }
		NetSession* network() const { return m_net.get(); }
		const Ref<Scene>& sceneRef() const { return m_scene; }
		Scene& scene() { return *m_scene; }
		const Config& config() const { return m_config; }
		int localPlayerIndex() const { return m_config.localPlayerIndex; }

	private:
		enum class State { Idle, Live };

		void tickInner(float deltaTime);
		void rollbackStart();

		State m_state = State::Idle;
		Config m_config;
		Ref<Scene> m_scene;
		GameModuleHost m_host;
		std::unique_ptr<NetSession> m_net;
		bool m_paused = false;
		bool m_stepOnce = false;
		bool m_focused = true;
	};

}
