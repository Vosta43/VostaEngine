#include "PlaySession.h"

#include <Core/Application.h>
#include <Core/Log.h>
#include <Scene/Scene.h>
#include <Scene/SceneClone.h>

namespace ve {

PlaySession::~PlaySession() {
    stop();
}

bool PlaySession::play(Application& app, const Ref<Scene>& authored, const std::string& dllPath) {
    if (m_state != State::Edit) {
        VE_CORE_WARN_PRINT("%s", "PlaySession: a session is already running");
        return false;
    }
    if (!authored) {
        VE_CORE_ERROR_PRINT("%s", "PlaySession: no scene to play");
        return false;
    }

    m_runtimeScene = cloneScene(authored);
    if (!m_runtimeScene) {
        VE_CORE_ERROR_PRINT("%s", "PlaySession: scene clone failed");
        return false;
    }

    m_stepOnce = false;

    if (!dllPath.empty()) {
        if (m_host.load(app, dllPath))
            m_host.module()->onPlay(*m_runtimeScene);
        else
            VE_CORE_WARN_PRINT("PlaySession: module load failed, running without gameplay: %s",
                               m_host.lastError().c_str());
    }

    m_state = State::Play;
    return true;
}

void PlaySession::tick(float deltaTime) {
    if (!isLive() || !m_runtimeScene)
        return;
    if (m_state == State::Pause && !m_stepOnce)
        return;

    m_runtimeScene->onUpdate(deltaTime);

    if (m_stepOnce) {
        m_stepOnce = false;
        m_state = State::Pause;
    }
}

void PlaySession::pause() {
    if (m_state == State::Play)
        m_state = State::Pause;
}

void PlaySession::resume() {
    if (m_state == State::Pause) {
        m_stepOnce = false;
        m_state = State::Play;
    }
}

void PlaySession::stepOnce() {
    if (m_state == State::Pause)
        m_stepOnce = true;
}

void PlaySession::stop() {
    if (m_host.isLoaded()) {
        if (m_runtimeScene)
            m_host.module()->onStop(*m_runtimeScene);
        m_host.unload();
    }

    m_runtimeScene.reset();
    m_stepOnce = false;
    m_state = State::Edit;
}

} // namespace ve
