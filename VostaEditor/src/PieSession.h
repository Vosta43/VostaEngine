#pragma once

#include <VostaEngine.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ve {

// What one Play press asks for: one listen server plus (players - 1) clients,
// all inside this process. players == 1 (or listenServer == false) is offline.
struct PieConfig {
    bool        listenServer = true;
    int         players      = 2;            // 1..4, includes the server
    uint16_t    port         = 7777;
    std::string address      = "127.0.0.1";
};

// A multi-world play session. Each world is a scene clone with its own module
// instance, its own network session and its own offscreen view; the host draws
// them as tiles and sends input only to the focused one. Stop discards every
// world, so the authored scene is never touched.
class PieSession {
public:
    enum class State { Edit, Play, Pause };

    struct World {
        std::unique_ptr<WorldContext> ctx;
        Ref<SceneView> view;
        std::string label;
    };

    PieSession() = default;
    ~PieSession();

    PieSession(const PieSession&) = delete;
    PieSession& operator=(const PieSession&) = delete;

    // Clones `authored` once per player, starts the module and the session on
    // each, and builds one SceneView per world. A failure rolls the whole PIE
    // back: a client without gameplay would only look like a working test.
    // `startCamera` seeds every world's fallback camera with the editor's pose,
    // so Play starts where the user was looking (a scene without a camera
    // component would otherwise land on the view's untouched default).
    bool start(Application& app, const Ref<Scene>& authored, const std::string& modulePath,
               const PieConfig& config, const SceneViewConfig& viewConfig,
               const Camera* startCamera = nullptr);

    // Server first, then clients, so a snapshot a client applies this frame
    // describes this frame's server state.
    void update(float deltaTime);

    void pause();
    void resume();
    void stepOnce();
    void stop();

    State state() const { return m_state; }
    bool isLive() const { return m_state != State::Edit; }

    size_t worldCount() const { return m_worlds.size(); }
    World& world(size_t index) { return m_worlds[index]; }
    const World& world(size_t index) const { return m_worlds[index]; }

    // Input and audio belong to the focused world only.
    void setFocusedWorld(size_t index);
    size_t focusedWorld() const { return m_focusedWorld; }

private:
    std::vector<World> m_worlds;
    State  m_state = State::Edit;
    size_t m_focusedWorld = 0;
    bool   m_stepOnce = false;
};

}
