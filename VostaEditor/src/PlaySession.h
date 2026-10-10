#pragma once

#include <VostaEngine.h>

#include <string>

namespace ve {

// Owns one play session: a throwaway duplicate of the authored scene, driven by
// a game-module DLL. Play duplicates the scene and loads the module; Stop
// discards both. The authored scene is never touched, so Stop needs no reversion
// pass -- there is nothing to undo.
class PlaySession {
public:
    enum class State { Edit, Play, Pause };

    ~PlaySession();

    PlaySession() = default;
    PlaySession(const PlaySession&) = delete;
    PlaySession& operator=(const PlaySession&) = delete;

    // Duplicates `authored`, loads the module at `dllPath` (empty = run with no
    // gameplay module) and calls its onPlay on the copy. False when a session is
    // already live, the scene is null, or the clone fails. A module that fails to
    // load is logged, not fatal: the session still runs, just without gameplay.
    bool play(Application& app, const Ref<Scene>& authored, const std::string& dllPath);

    // Drives the runtime scene. A no-op while paused, except that a pending single
    // step runs one frame and returns to paused.
    void tick(float deltaTime);

    void pause();
    void resume();
    void stepOnce();
    void stop();

    State state() const { return m_state; }
    bool isLive() const { return m_state != State::Edit; }
    const Ref<Scene>& runtimeScene() const { return m_runtimeScene; }

private:
    GameModuleHost m_host;
    Ref<Scene>     m_runtimeScene;
    State          m_state = State::Edit;
    bool           m_stepOnce = false;
};

} // namespace ve
