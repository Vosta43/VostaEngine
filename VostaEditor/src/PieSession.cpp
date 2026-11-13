#include "PieSession.h"

#include <algorithm>

#include <Audio/AudioDevice.h>
#include <Core/Application.h>
#include <Core/Log.h>
#include <Core/WorldContext.h>
#include <Network/NetSession.h>
#include <Scene/Scene.h>
#include <Scene/SceneClone.h>

namespace ve {

namespace {

    constexpr int kMaxPlayers = 4;
    // The default port may still be held by the previous Play; walking forward a
    // few ports keeps re-entering Play a one-click operation.
    constexpr int kServerPortAttempts = 10;

}

PieSession::~PieSession() {
    stop();
}

bool PieSession::start(Application& app, const Ref<Scene>& authored, const std::string& modulePath,
                       const PieConfig& config, const SceneViewConfig& viewConfig,
                       const Camera* startCamera) {
    if (isLive()) {
        VE_CORE_WARN_PRINT("%s", "PieSession: a session is already running");
        return false;
    }
    if (!authored) {
        VE_CORE_ERROR_PRINT("%s", "PieSession: no scene to play");
        return false;
    }

    const int players = std::clamp(config.players, 1, kMaxPlayers);
    const bool networked = config.listenServer && players > 1;

    uint16_t port = config.port;
    m_focusedWorld = 0;
    m_stepOnce = false;

    for (int i = 0; i < players; ++i) {
        const bool isServer = networked && i == 0;
        const bool isClient = networked && i > 0;

        std::string lastError;
        bool started = false;

        // The server owns the port and retries it; clients dial the port the
        // server actually bound.
        const int attempts = isServer ? kServerPortAttempts : 1;
        for (int attempt = 0; attempt < attempts && !started; ++attempt) {
            const uint16_t candidate = static_cast<uint16_t>(port + attempt);

            Ref<Scene> runtime = cloneScene(authored);
            if (!runtime) {
                lastError = "scene clone failed";
                break;
            }

            auto ctx = std::make_unique<WorldContext>();

            WorldContext::Config worldConfig;
            worldConfig.mode = !networked
                ? WorldContext::Mode::Offline
                : (isServer ? WorldContext::Mode::ListenServer : WorldContext::Mode::Client);
            worldConfig.modulePath = modulePath;
            worldConfig.address = config.address;
            worldConfig.port = candidate;
            worldConfig.localPlayerIndex = i;

            // Move the reference in: the world must be the only holder, or a
            // failed start leaves the scene alive past the module unload.
            if (!ctx->start(app, std::move(runtime), worldConfig, &lastError))
                continue;

            World world;
            world.ctx = std::move(ctx);
            world.label = !networked
                ? "Offline (Player 0)"
                : (isServer ? "Server (Player 0)" : "Client " + std::to_string(i));

            world.view = CreateRef<SceneView>(world.ctx->sceneRef(), viewConfig);
            if (startCamera)
                world.view->getEditorCamera() = *startCamera;
            world.view->onAttach();
            // A real target before the first tile is laid out, so the first
            // frame renders instead of showing an empty texture.
            world.view->setSize(viewConfig.width, viewConfig.height);
            world.view->setCameraSource(SceneView::CameraSource::Scene);
            world.view->setInputEnabled(false);

            if (isServer)
                port = candidate;

            m_worlds.push_back(std::move(world));
            started = true;
        }

        if (!started) {
            VE_CORE_ERROR_PRINT("PIE: %s failed to start (%s)",
                isServer ? "server" : (isClient ? "client" : "world"), lastError.c_str());
            stop();
            return false;
        }
    }

    m_state = State::Play;
    VE_CORE_SUCCESS_PRINT("PIE: %zu world(s) started on %s:%u", m_worlds.size(),
        networked ? config.address.c_str() : "offline", static_cast<unsigned>(port));
    return true;
}

void PieSession::update(float deltaTime) {
    if (!isLive())
        return;

    const bool paused = m_state == State::Pause;
    const bool step = m_stepOnce;

    // Index 0 is the server when networked, so this order is server-then-clients.
    for (size_t i = 0; i < m_worlds.size(); ++i) {
        World& world = m_worlds[i];
        world.ctx->setFocused(i == m_focusedWorld);
        world.ctx->setPaused(paused);
        if (paused && step)
            world.ctx->requestStep();
        world.ctx->tick(deltaTime);

        // Each tile renders into its own framebuffer; the editor's own view is
        // parked while PIE is live, so nothing else would draw these.
        if (world.view)
            world.view->onUpdate(deltaTime);
    }

    m_stepOnce = false;
}

void PieSession::pause() {
    if (m_state != State::Play)
        return;
    AudioDevice::get().setPaused(true);
    m_state = State::Pause;
}

void PieSession::resume() {
    if (m_state != State::Pause)
        return;
    AudioDevice::get().setPaused(false);
    m_stepOnce = false;
    m_state = State::Play;
}

void PieSession::stepOnce() {
    if (m_state == State::Pause)
        m_stepOnce = true;
}

void PieSession::stop() {
    for (World& world : m_worlds) {
        if (world.view) {
            world.view->setInputEnabled(false);
            // Release the view first: it holds a scene reference, and the world
            // can only destroy its scene (before unloading the module DLL) once
            // it is the last holder.
            world.view.reset();
        }
        if (world.ctx)
            world.ctx->stop();
    }
    m_worlds.clear();

    AudioDevice::get().stopAllVoices();
    AudioDevice::get().setPaused(false);
    m_state = State::Edit;
    m_focusedWorld = 0;
    m_stepOnce = false;
}

void PieSession::setFocusedWorld(size_t index) {
    if (!m_worlds.empty())
        m_focusedWorld = std::min(index, m_worlds.size() - 1);
}

}
