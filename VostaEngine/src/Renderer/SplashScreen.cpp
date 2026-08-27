#include "vepch.h"
#include "SplashScreen.h"

#include "Renderer/Texture.h"
#include "Renderer/Shader.h"
#include "Renderer/Buffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/RenderCommand.h"

namespace ve {

    SplashScreen::SplashScreen(ShaderLibrary& shaderLibrary)
        : m_shaderLibrary(shaderLibrary) {
    }

    SplashScreen::~SplashScreen() = default;

    void SplashScreen::show() {
        createResources();
        m_visible = true;
    }

    void SplashScreen::hide() {
        m_visible = false;
    }

    void SplashScreen::createResources() {
        if (!m_texture) {
            m_texture = Texture2D::create("SandBox/assets/textures/ve.png");
        }
        if (!m_shader) {
            m_shaderLibrary.load("SandBox/assets/shaders/Texture.glsl");
            m_shader = m_shaderLibrary.get("Texture");
        }
        if (!m_vertexArray) {
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
            m_vertexArray = VertexArray::create();
            m_vertexArray->addVertexBuffer(vb);
            m_vertexArray->setIndexBuffer(ib);
        }
    }

    void SplashScreen::render() {
        if (!m_visible || !m_texture || !m_shader || !m_vertexArray) return;

        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        m_shader->bind();
        m_texture->bind(0);
        m_shader->setInt("u_Texture", 0);
        m_shader->setMat4("u_ViewProjection", glm::mat4(1.0f));
        m_shader->setMat4("u_Transform", glm::mat4(1.0f));

        m_vertexArray->bind();
        RenderCommand::drawIndexed(m_vertexArray);
        m_vertexArray->unbind();

        m_shader->unbind();
    }

}
