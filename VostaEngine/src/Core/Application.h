#ifndef APPLICATION_H
#define APPLICATION_H

#include "Core.h"
#include "Layers/LayerStack.h"

#include "Gui/Gui.h"
#include "Renderer/Camera.h"
#include "Renderer/CameraController.h"
#include "Renderer/Shader.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/VertexArray.h"


#include <memory>
namespace ve {

    class Window;
    class Renderer;
    class EventDispatcher;

    class VE_API Application {
    public:
        Application();
        void splashScreen(bool show);
        void renderSplash();
        ~Application();

        static Application& get() {return *s_instance;}
        void run();
        virtual void onUpdate(float deltaTime) {};
        Window* getWindow() const;
        EventDispatcher* getDispatcher() const { return m_dispatcher.get(); }
        Camera& getCamera() { return m_camera; }

        void pushLayer(Layer* layer) {
            m_layerStack.pushLayer(layer);
        }

        void pushOverLay(Layer* layer) {
            m_layerStack.pushOverlay(layer);
            //FUCK THIS!!!
            //layer->onAttach();
        }


        bool isViewportHovered() const {
            return m_GuiLayer->isViewportHovered();
        }

        ShaderLibrary& getShaderLibrary() { return m_shaderLibrary; }

        GuiLayer* getGuiLayer() {return m_GuiLayer;}

    private:
        static Application* s_instance;
        std::unique_ptr<EventDispatcher> m_dispatcher;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<Renderer> m_renderer;
        GuiLayer* m_GuiLayer;

        LayerStack m_layerStack;
        ShaderLibrary m_shaderLibrary;

        // Splash
        Ref<Texture2D> m_splashTexture;
        Ref<Shader> m_splashShader;
        Ref<VertexArray> m_splashVA;
        bool m_splashVisible = false;

    protected:
        Camera m_camera;

    };

}

#endif