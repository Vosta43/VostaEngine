#include "vepch.h"
#include "Application.h"
#include "Window.h"
#include "Log.h"
#include "Events/Event.h"
#include "Events/EventDispatcher.h"
#include "Events/KeyPressedEvent.h"
#include "Events/MouseMovedEvent.h"
#include "Events/WindowResizeEvent.h"
#include "Input.h"
#include "Core/Layers/Layers.h"
#include "KeyCodes.h"
#include "MouseButtonCodes.h"
#include "Deltatime.h"

#include "Gui/Gui.h"
#include "Renderer/Shader.h"
#include "Renderer/SplashScreen.h"

namespace ve {

    Application* Application::s_instance = nullptr;

    Application::Application() {

        s_instance = this;

        m_dispatcher = std::make_unique<EventDispatcher>();
        VE_CORE_SUCCESS("Event dispatcher initialized");

        m_window = std::make_unique<Window>(1500, 980, "VostaEngine 0.2.4 dev", m_dispatcher.get());
        VE_CORE_SUCCESS("Window created");

        m_shaderLibrary = std::make_unique<ShaderLibrary>();
        m_splash = std::make_unique<SplashScreen>(*m_shaderLibrary);

        m_GuiLayer = std::make_unique<GuiLayer>();
        pushOverLay(m_GuiLayer.get());

    }

    void Application::splashScreen(bool show) {
        if (show) {
            m_splash->show();
            m_splash->render();
            m_window->swapBuffers();
        }
        else {
            m_splash->hide();
        }
    }

    Application::~Application() {
        m_layerStack.popOverlay(m_GuiLayer.get());
    }

    void Application::run() {
        VE_CORE_SUCCESS("Application running");

        while (!m_window->shouldClose()) {
            m_window->processEvents();

            if (m_splash->isVisible()) {
                m_splash->render();
                m_window->swapBuffers();
                continue;
            }

            DeltaTime::get().update();
            onUpdate(DeltaTime::get().getDeltaTime());

            for (Layer* layer : m_layerStack) {
                layer->onUpdate();
            }

            m_GuiLayer->begin();
            for (Layer* layer : m_layerStack) {
                layer->onImGuiRender();
            }
            m_GuiLayer->end();

            m_window->swapBuffers();
        }

        VE_CORE_SUCCESS("Application closed");
    }

    Window* Application::getWindow() const {
        return m_window.get();
    }

    bool Application::isViewportHovered() const {
        return m_GuiLayer->isViewportHovered();
    }

    ShaderLibrary& Application::getShaderLibrary() {
        return *m_shaderLibrary;
    }

    GuiLayer* Application::getGuiLayer() {
        return m_GuiLayer.get();
    }

}
