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
#include "Core/AssetLoadQueue.h"
#include "Core/ResourceManager.h"

#include "Gui/Gui.h"
#include "Gui/Vellum/VellumLayer.h"
#include "Renderer/Shader.h"
#include "Renderer/SplashScreen.h"
#include "Asset/BuiltinReousrces.h"
#include "Audio/AudioDevice.h"
#include "Particle/ParticleSelfTest.h"

namespace ve {

    Application* Application::s_instance = nullptr;

    Application::Application() {

        s_instance = this;

        // Register engine-owned assets (default material, ...) before any scene
        // or layer can ask for them.
        BuiltinResources::init();

        // Opens the output device (and the decoder's resource manager). A
        // machine without speakers still runs; every audio call then no-ops.
        AudioDevice::get().init();

#ifdef _DEBUG
        ParticleSelfTest::runAll();
#endif

        m_dispatcher = std::make_unique<EventDispatcher>();
        VE_CORE_SUCCESS("Event dispatcher initialized");

        m_window = std::make_unique<Window>(1500, 980, "VostaEngine 0.2.11 dev", m_dispatcher.get());
        VE_CORE_SUCCESS("Window created");

        m_shaderLibrary = std::make_unique<ShaderLibrary>();
        m_splash = std::make_unique<SplashScreen>(*m_shaderLibrary);

        m_GuiLayer = std::make_unique<GuiLayer>();
        pushOverLay(m_GuiLayer.get());

        m_vellumLayer = std::make_unique<vellum::VellumLayer>();
        pushOverLay(m_vellumLayer.get());

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
        AudioDevice::get().shutdown();
        m_layerStack.popOverlay(m_vellumLayer.get());
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

            // Workers finished some loads; GPU upload and storage writes must
            // happen on this thread.
            AssetLoadQueue::get().pump();
            // Age LRU entries, trim over-budget resources, and refill slots
            // whose demand outlived an unload.
            ResourceManager::tick();

            // Reap voices that reached their end and re-anchor spatial voices
            // after the listener moved.
            AudioDevice::get().update();

            onUpdate(DeltaTime::get().getDeltaTime());

            for (Layer* layer : m_layerStack) {
                layer->onUpdate();
            }

            m_GuiLayer->begin();
            for (Layer* layer : m_layerStack) {
                layer->onUIRender();
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

    vellum::VellumLayer* Application::getVellumLayer() {
        return m_vellumLayer.get();
    }

}
