#include "vepch.h"
#include "TAAPass.h"
#include "Renderer/RenderCommand.h"
#include "Core/Application.h"
#include "Renderer/Buffer.h"

namespace ve {

    void TAAPass::init() {
        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("SandBox/assets/shaders/taa.glsl");
        m_taaShader = shaderLib.get("taa");

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
        if (!m_history[0] || !m_history[1] || !m_taaShader)
            return;

        auto hdrIt = ctx.inputTextures.find("hdrColor");
        auto depthIt = ctx.inputTextures.find("depth");
        if (hdrIt == ctx.inputTextures.end() || depthIt == ctx.inputTextures.end())
            return;

        // History is invalidated whenever the buffer is recreated at a new
        // size (window resize rebuilds the framebuffers and drops their
        // contents). Detect that by comparing the current buffer size against
        // what we last rendered at.
        if (m_history[m_writeIndex]->getWidth() != m_lastWidth ||
            m_history[m_writeIndex]->getHeight() != m_lastHeight) {
            m_firstFrame = true;
        }
        m_lastWidth = m_history[m_writeIndex]->getWidth();
        m_lastHeight = m_history[m_writeIndex]->getHeight();

        Ref<Framebuffer> readBuffer = m_history[m_writeIndex ^ 1];
        Ref<Framebuffer> writeBuffer = m_history[m_writeIndex];

        writeBuffer->bind();
        RenderCommand::setViewport(0, 0, writeBuffer->getWidth(), writeBuffer->getHeight());
        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        m_taaShader->bind();
        m_taaShader->setTexture("u_HDRColor", hdrIt->second, 0);
        m_taaShader->setTexture("u_DepthMap", depthIt->second, 1);
        m_taaShader->setTexture("u_History", readBuffer->getColorTexture(0), 2);

        // Cloud pixels are temporally accumulated by CloudTAAPass, so the screen
        // TAA must not accumulate them (wrong parallax -> ghosting). The clouds
        // key is always set by CloudPass (even when the cloud layer is disabled,
        // it holds a fully-transparent buffer -> cloudT = 1 -> normal accumulate).
        auto cloudIt = ctx.inputTextures.find("clouds");
        if (cloudIt != ctx.inputTextures.end())
            m_taaShader->setTexture("u_CloudTex", cloudIt->second, 3);

        m_taaShader->setMat4("u_InvViewProj", glm::inverse(ctx.projMatrix * ctx.viewMatrix));
        m_taaShader->setMat4("u_PrevViewProj", ctx.prevViewProjMatrix);

        // First frame / after resize: history is empty or stale — write the
        // current frame as-is so we don't blend against garbage.
        m_taaShader->setFloat("u_BlendAlpha", m_firstFrame ? 1.0f : 0.07f);
        m_taaShader->setFloat2("u_TexelSize", glm::vec2(
            1.0f / (float)writeBuffer->getWidth(),
            1.0f / (float)writeBuffer->getHeight()));

        m_fullscreenQuad->bind();
        RenderCommand::drawIndexed(m_fullscreenQuad);
        m_fullscreenQuad->unbind();

        m_taaShader->unbind();
        writeBuffer->unbind();

        // Overwrite the hdrColor key so PostBufferPass picks up the TAA'd HDR.
        ctx.inputTextures["hdrColor"] = writeBuffer->getColorTexture(0);

        m_writeIndex ^= 1;
        m_firstFrame = false;
    }

}
