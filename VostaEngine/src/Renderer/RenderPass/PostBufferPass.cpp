#include "vepch.h"
#include "PostBufferPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"

namespace ve {

    void PostBufferPass::init()
    {
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
        if (!m_target || !m_shader)
            return;

        m_target->bind();
        RenderCommand::setViewport(ctx.viewPortX, ctx.viewPortY,
                                   ctx.viewPortWidth, ctx.viewPortHeight);
        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        m_shader->bind();
        bindPassInputs(*this, ctx);
        applyPassUniforms(*this);
        m_shader->setFloat("u_Exposure", 1.0f);

        m_fullscreenQuad->bind();
        RenderCommand::drawIndexed(m_fullscreenQuad);
        m_fullscreenQuad->unbind();

        m_shader->unbind();

        m_target->unbind();
        setWritten(m_target);
    }

}
