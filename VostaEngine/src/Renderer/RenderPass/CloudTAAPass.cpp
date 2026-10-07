#include "vepch.h"
#include "CloudTAAPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"

namespace ve {

    void CloudTAAPass::init() {
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

    void CloudTAAPass::execute(RenderContext& ctx) {
        // When there is nothing to accumulate, hand consumers the raw cloud
        // buffer (CloudPass cleared it to transparent) so "cloudTaa" still
        // resolves to a defined texture instead of leaving the sampled unit stale.
        auto passthrough = [&]() {
            if (ctx.passOutputs) {
                auto it = ctx.passOutputs->find("cloud");
                if (it != ctx.passOutputs->end()) setWritten(it->second);
            }
        };

        if (!hasHistory() || !m_shader) { passthrough(); return; }

        // Mirror CloudPass's disable condition: with no cloud layer the raw
        // cloud buffer stays transparent, so leave it as-is (HDR composites
        // nothing) instead of running a pointless fullscreen pass.
        if (!ctx.hasAtmosphere || !ctx.cloudNoiseTexture) { passthrough(); return; }
        const AtmosphereParams& a = ctx.atmosphere;
        const CloudParams& c = ctx.clouds;
        float inner = a.planetRadius + c.bottomAltitude;
        float outer = a.planetRadius + c.topAltitude;
        if (outer <= inner) { passthrough(); return; }

        Ref<Framebuffer> readBuffer = historyRead(ctx.frameIndex);
        Ref<Framebuffer> writeBuffer = historyWrite(ctx.frameIndex);
        if (!readBuffer || !writeBuffer) { passthrough(); return; }

        // History is invalidated whenever the buffer is recreated at a new size
        // (window resize rebuilds the framebuffers and drops their contents).
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
        // u_CloudColor (the raw frame) and u_History (the read half) come from the
        // declared inputs; the rest is per-frame camera/atmosphere state.
        bindPassInputs(*this, ctx);

        m_shader->setMat4("u_InvViewProj", glm::inverse(ctx.projMatrix * ctx.viewMatrix));
        m_shader->setMat4("u_PrevViewProj", ctx.prevViewProjMatrix);
        m_shader->setFloat3("u_CameraPos", ctx.cameraPosition);
        m_shader->setFloat3("u_PlanetCenter", ctx.planetCenter);
        m_shader->setFloat("u_PlanetRadius", a.planetRadius);
        m_shader->setFloat("u_CloudInnerRadius", inner);
        m_shader->setFloat("u_CloudOuterRadius", outer);

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
