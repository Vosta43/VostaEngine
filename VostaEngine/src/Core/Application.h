#ifndef APPLICATION_H
#define APPLICATION_H

#include "Core.h"
#include "Layers/LayerStack.h"

#include <memory>
namespace ve {

    class Window;
    class EventDispatcher;
    class GuiLayer;
    class ShaderLibrary;
    class SplashScreen;
    class NetSession;
    class WorldContext;

    namespace vellum { class VellumLayer; }


    class VE_API Application {
    public:
        Application();
        void splashScreen(bool show);
        ~Application();

        static Application& get() {return *s_instance;}
        void run();
        virtual void onUpdate(float deltaTime) {};
        Window* getWindow() const;
        EventDispatcher* getDispatcher() const { return m_dispatcher.get(); }

        void pushLayer(Layer* layer) {
            m_layerStack.pushLayer(layer);
        }

        void pushOverLay(Layer* layer) {
            m_layerStack.pushOverlay(layer);
        }

        void popLayer(Layer* layer) {
            m_layerStack.popLayer(layer);
        }

        void popOverLay(Layer* layer) {
            m_layerStack.popOverlay(layer);
        }

        bool isViewportHovered() const;
        ShaderLibrary& getShaderLibrary();
        GuiLayer* getGuiLayer();
        vellum::VellumLayer* getVellumLayer();

        // The network a module should use. While a world is ticking this is that
        // world's session; otherwise it is the process-wide one run() pumps
        // (VostaPlayer and other single-world hosts).
        NetSession* getNetwork() const;

        // The world currently ticking, if any. A host with several worlds (PIE)
        // scopes this around each tick, so module code sees its own world.
        WorldContext* getActiveWorld() const { return m_activeWorld; }
        void setActiveWorld(WorldContext* world) { m_activeWorld = world; }
        // Which player this world controls: 0 for offline/listen-server worlds,
        // 1..N-1 for the clients of a multi-world session.
        int getLocalPlayerIndex() const;

        // Drops the Vellum tree a gameplay module installed. Hosts call it before
        // unloading the module: widget vtables and callbacks may live in its DLL.
        void clearGameUi();

    private:
        void pollProfilerHotkey();

        // True while the capture hotkey is held, so one press is one toggle.
        bool m_captureKeyHeld = false;

        static Application* s_instance;
        std::unique_ptr<EventDispatcher> m_dispatcher;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<GuiLayer> m_GuiLayer;
        std::unique_ptr<vellum::VellumLayer> m_vellumLayer;
        std::unique_ptr<ShaderLibrary> m_shaderLibrary;
        std::unique_ptr<SplashScreen> m_splash;
        std::unique_ptr<NetSession> m_network;

        LayerStack m_layerStack;

        // True on Windows when this constructor initialised the thread as an STA.
        bool m_comInitialized = false;

        // Appended last so the layout modules were compiled against does not move;
        // getNetwork()/getLocalPlayerIndex() are out-of-line for the same reason.
        WorldContext* m_activeWorld = nullptr;
    };

}

#endif
