#include "vepch.h"
#include "PostBufferPass.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"
#include "Core/Application.h"

namespace ve {

    void PostBufferPass::init()
    {
        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("SandBox/assets/shaders/postprocess.glsl");
        m_postShader = shaderLib.get("postprocess");

        float vertices[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f,
            -1.0f,  1.0f
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        m_fullscreenQuad = VertexArray::create();
        auto vb = VertexBuffer::create(vertices, sizeof(vertices));
        BufferLayout layout = { { ShaderDataType::Float2, "a_Position" } };
        vb->setLayout(layout);
        m_fullscreenQuad->addVertexBuffer(vb);

        auto ib = IndexBuffer::create(indices, 6);
        m_fullscreenQuad->setIndexBuffer(ib);
    }

    void PostBufferPass::execute(RenderContext& ctx)
    {
        if (!m_postBuffer || !m_postShader)
            return;

        m_postBuffer->bind();
        RenderCommand::setViewport(ctx.viewPortX, ctx.viewPortY,
                                   ctx.viewPortWidth, ctx.viewPortHeight);
        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        auto hdrIt = ctx.inputTextures.find("hdrColor");
        if (hdrIt != ctx.inputTextures.end()) {
            m_postShader->bind();
            m_postShader->setTexture("u_HDRColor", hdrIt->second, 0);
            m_postShader->setFloat("u_Exposure", 1.0f);

            m_fullscreenQuad->bind();
            RenderCommand::drawIndexed(m_fullscreenQuad);
            m_fullscreenQuad->unbind();

            m_postShader->unbind();
        }

        m_postBuffer->unbind();

        // Overwrite so downstream consumers (screen pass / thumbnail renderer)
        // pick up the tone-mapped LDR result instead of raw HDR.
        ctx.inputTextures["hdrColor"] = m_postBuffer->getColorTexture(0);
    }

}
