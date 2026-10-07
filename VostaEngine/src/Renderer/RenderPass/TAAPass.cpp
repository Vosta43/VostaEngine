#include "vepch.h"
#include "TAAPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"

namespace ve {

    void TAAPass::init() {
        float vertices[] = {
            -1.0f, -1.0f,  // bottom left
             1.0f, -1.0f,  // bottom right
             1.0f,  1.0f,  // top right
            -1.0f,  1.0f   // top left
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        m_fullscreenQuad = VertexArray::create();
        auto vb = VertexBuffer::create(vertices, sizeof(vertices));
        BufferLayout layout = {
            { ShaderDataType::Float2, "a_Position" }
        };
        vb->setLayout(layout);
        m_fullscreenQuad->addVertexBuffer(vb);

        auto ib = IndexBuffer::create(indices, 6);
        m_fullscreenQuad->setIndexBuffer(ib);
    }

    void TAAPass::execute(RenderContext& ctx) {
        if (!hasHistory() || !m_shader)
            return;

        Ref<Framebuffer> readBuffer = historyRead(ctx.frameIndex);
        Ref<Framebuffer> writeBuffer = historyWrite(ctx.frameIndex);
        if (!readBuffer || !writeBuffer)
            return;

        // History is invalidated whenever the buffer is recreated at a new
        // size (window resize rebuilds the framebuffers and drops their
        // contents). Detect that by comparing the current buffer size against
        // what we last rendered at.
        if (writeBuffer->getWidth() != m_lastWidth ||
            writeBuffer->getHeight() != m_lastHeight) {
            m_firstFrame = true;
        }
        m_lastWidth = writeBuffer->getWidth();
        m_lastHeight = writeBuffer->getHeight();

        writeBuffer->bind();
        RenderCommand::setViewport(0, 0, writeBuffer->getWidth(), writeBuffer->getHeight());
        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        m_shader->bind();
        // u_HDRColor (@previous), u_DepthMap (gbuffer depth), u_History (read
        // half) and u_CloudTex (cloudTaa) come from the declared inputs.
        bindPassInputs(*this, ctx);

        m_shader->setMat4("u_InvViewProj", glm::inverse(ctx.projMatrix * ctx.viewMatrix));
        m_shader->setMat4("u_PrevViewProj", ctx.prevViewProjMatrix);

        // First frame / after resize: history is empty or stale — write the
        // current frame as-is so we don't blend against garbage.
        m_shader->setFloat("u_BlendAlpha", m_firstFrame ? 1.0f : 0.07f);
        m_shader->setFloat2("u_TexelSize", glm::vec2(
            1.0f / (float)writeBuffer->getWidth(),
            1.0f / (float)writeBuffer->getHeight()));

        m_fullscreenQuad->bind();
        RenderCommand::drawIndexed(m_fullscreenQuad);
        m_fullscreenQuad->unbind();

        m_shader->unbind();
        writeBuffer->unbind();

        setWritten(writeBuffer);
        m_firstFrame = false;
    }

}
