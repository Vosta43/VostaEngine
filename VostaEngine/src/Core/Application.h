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

    private:
        static Application* s_instance;
        std::unique_ptr<EventDispatcher> m_dispatcher;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<GuiLayer> m_GuiLayer;
        std::unique_ptr<vellum::VellumLayer> m_vellumLayer;
        std::unique_ptr<ShaderLibrary> m_shaderLibrary;
        std::unique_ptr<SplashScreen> m_splash;

        LayerStack m_layerStack;

    };

}

#endif
