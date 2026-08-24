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

#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"

namespace ve {

    Application* Application::s_instance = nullptr;
    
    Application::Application()
        : m_camera(-1.0f,1.0f,-1.0f,1.0f) {
        
        s_instance = this;

        m_dispatcher = std::make_unique<EventDispatcher>();
        VE_CORE_SUCCESS("Event dispatcher initialized");

        m_window = std::make_unique<Window>(1500, 980, "VostaEngine 0.2.2 dev", m_dispatcher.get());
        VE_CORE_SUCCESS("Window created");

        m_camera.setAspectRatio((float)1500 / (float)980);
        //m_renderer = std::make_unique<Renderer>();
        //m_renderer->init(m_window.get());
        //VE_CORE_SUCCESS("Renderer initialized");

        m_GuiLayer = new GuiLayer;
        pushOverLay(m_GuiLayer);

    }

    void Application::splashScreen(bool show) {
        if (show) {
            if (!m_splashTexture) {
                m_splashTexture = Texture2D::create("SandBox/assets/textures/ve.png");
            }
            if (!m_splashShader) {
                auto& shaderLib = getShaderLibrary();
                shaderLib.load("SandBox/assets/shaders/Texture.glsl");
                m_splashShader = shaderLib.get("Texture");
            }
            if (!m_splashVA) {

                float vertices[] = {
                    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                     1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
                     1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                };
                uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

                auto vb = VertexBuffer::create(vertices, sizeof(vertices));
                vb->setLayout({
                    { ShaderDataType::Float3, "a_Position" },
                    { ShaderDataType::Float2, "a_TexCoord" },
                    });
                auto ib = IndexBuffer::create(indices, 6);
                m_splashVA = VertexArray::create();
                m_splashVA->addVertexBuffer(vb);
                m_splashVA->setIndexBuffer(ib);
            }
            m_splashVisible = true;

            renderSplash();
            m_window->swapBuffers();
        }
        else {
            m_splashVisible = false;
        }
    }

    void Application::renderSplash() {
        if (!m_splashVisible || !m_splashTexture || !m_splashShader) return;

        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        m_splashShader->bind();
        m_splashTexture->bind(0);
        m_splashShader->setInt("u_Texture", 0);
        m_splashShader->setMat4("u_ViewProjection", glm::mat4(1.0f));
        m_splashShader->setMat4("u_Transform", glm::mat4(1.0f));

        m_splashVA->bind();
        RenderCommand::drawIndexed(m_splashVA);
        m_splashVA->unbind();

        //m_splashTexture->unbind();
        m_splashShader->unbind();
    }

    Application::~Application() = default;

    void Application::run() {
        VE_CORE_SUCCESS("Application running");

        while (!m_window->shouldClose()) {
            m_window->processEvents();

            if (m_splashVisible) {
                renderSplash();
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

}