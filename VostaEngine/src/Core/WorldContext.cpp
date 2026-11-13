#include "vepch.h"
#include "Core/WorldContext.h"

#include "Core/Application.h"
#include "Core/InputGate.h"
#include "Network/NetSession.h"
#include "Network/Rpc.h"
#include "Scene/Scene.h"

namespace ve {

	namespace {

		// While a world ticks, Application resolves getNetwork() and the local
		// player index to it -- game modules read both every frame.
		class ScopedActiveWorld {
		public:
			explicit ScopedActiveWorld(WorldContext* world)
				: m_previous(Application::get().getActiveWorld()) {
				Application::get().setActiveWorld(world);
			}

			~ScopedActiveWorld() {
				Application::get().setActiveWorld(m_previous);
			}

			ScopedActiveWorld(const ScopedActiveWorld&) = delete;
			ScopedActiveWorld& operator=(const ScopedActiveWorld&) = delete;

		private:
			WorldContext* m_previous;
		};

	}

	WorldContext::WorldContext() = default;

	WorldContext::~WorldContext() {
		stop();
	}

	bool WorldContext::start(Application& app, Ref<Scene> scene, const Config& config, std::string* error) {
		auto fail = [error](const std::string& message) {
			if (error)
				*error = message;
			return false;
		};

		if (m_state == State::Live)
			return fail("world is already running");
		if (!scene)
			return fail("no scene to run");

		m_config = config;
		m_scene = std::move(scene);
		m_paused = false;
		m_stepOnce = false;

		// Inert until start*(); it exists now so module start-up already resolves
		// this world's session rather than a sibling world's.
		if (m_config.mode != Mode::Offline)
			m_net = std::make_unique<NetSession>();

		{
			ScopedActiveWorld active(this);
			rpc::ScopedRpcSession rpcScope(m_net.get());

			if (!m_config.modulePath.empty()) {
				if (!m_host.load(app, m_config.modulePath)) {
					const std::string message = m_host.lastError();
					rollbackStart();
					return fail(message);
				}
				if (!m_host.start(*m_scene)) {
					const std::string message = m_host.lastError();
					rollbackStart();
					return fail(message);
				}
			}
		}

		// After the module: its replicated properties must exist before the
		// session builds a layout from the registry.
		if (m_net) {
			const bool started = m_config.mode == Mode::ListenServer
				? m_net->startServer(*m_scene, m_config.port)
				: m_net->startClient(*m_scene, m_config.address, m_config.port);

			if (!started) {
				const std::string message = m_config.mode == Mode::ListenServer
					? "cannot listen on port " + std::to_string(m_config.port)
					: "cannot connect to " + m_config.address + ":" + std::to_string(m_config.port);
				rollbackStart();
				return fail(message);
			}
		}

		m_state = State::Live;
		return true;
	}

	void WorldContext::rollbackStart() {
		if (m_scene && m_host.isLoaded()) {
			rpc::ScopedRpcSession rpcScope(m_net.get());
			m_host.stop(*m_scene);
		}
		if (m_net) {
			m_net->stop();
			m_net.reset();
		}
		if (m_scene) {
			m_scene->stopAudio();
			// Destroy the scene before the module unloads: components it
			// registered hold type-erased callbacks compiled into the module DLL.
			m_scene.reset();
		}
		m_host.unload();
		m_state = State::Idle;
	}

	void WorldContext::stop() {
		if (m_state == State::Idle && !m_host.isLoaded() && !m_net)
			return;

		if (m_host.isLoaded()) {
			rpc::ScopedRpcSession rpcScope(m_net.get());
			if (m_scene)
				m_host.stop(*m_scene);
		}

		if (m_net) {
			m_net->stop();
			m_net.reset();
		}

		if (m_scene) {
			m_scene->stopAudio();
			// The scene must die while the module DLL is still mapped: component
			// storages hold type-erased lambdas whose code lives in that DLL, and
			// unloading first turns their destructors into a jump into freed code.
			m_scene.reset();
		}

		m_host.unload();

		m_paused = false;
		m_stepOnce = false;
		m_state = State::Idle;
	}

	void WorldContext::tick(float deltaTime) {
		if (m_state != State::Live || !m_scene)
			return;

		ScopedActiveWorld active(this);
		rpc::ScopedRpcSession rpcScope(m_net.get());

		// One keyboard, many worlds: an unfocused world must read nothing, or
		// every client would react to the same WASD press.
		if (m_focused) {
			tickInner(deltaTime);
		}
		else {
			ScopedInputBlock block;
			tickInner(deltaTime);
		}
	}

	void WorldContext::tickInner(float deltaTime) {
		// The mixer is process-wide; only the focused world drives its listener
		// and voices, so sounds do not stack N times.
		m_scene->setAudioEnabled(m_focused);

		if (!m_paused) {
			m_scene->onUpdate(deltaTime);
		}
		else if (m_stepOnce) {
			m_scene->stepFixed();
			m_stepOnce = false;
		}

		// Pumped even while paused: ENet times out without service, and a paused
		// PIE must not drop its peers.
		if (m_net)
			m_net->update(deltaTime);
	}

}
