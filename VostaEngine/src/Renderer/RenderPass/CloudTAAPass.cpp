#include "vepch.h"
#include "CloudTAAPass.h"
#include "Renderer/RenderCommand.h"
#include "Core/Application.h"
#include "Renderer/Buffer.h"

namespace ve {

    void CloudTAAPass::init() {
        auto& shaderLib = Application::get().getShaderLibrary();
        shaderLib.load("SandBox/assets/shaders/cloud_taa.glsl");
        m_cloudTaaShader = shaderLib.get("cloud_taa");

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
        if (!m_history[0] || !m_history[1] || !m_cloudTaaShader)
            return;

        auto cloudIt = ctx.inputTextures.find("clouds");
        if (cloudIt == ctx.inputTextures.end())
            return;

        // Mirror CloudPass's disable condition: with no cloud layer the raw
        // cloud buffer stays transparent, so leave it as-is (HDR composites
        // nothing) instead of running a pointless fullscreen pass.
        if (!ctx.hasAtmosphere || !ctx.cloudNoiseTexture)
            return;
        const AtmosphereParams& a = ctx.atmosphere;
        const CloudParams& c = ctx.clouds;
        float inner = a.planetRadius + c.bottomAltitude;
        float outer = a.planetRadius + c.topAltitude;
        if (outer <= inner)
            return;

        // History is invalidated whenever the buffer is recreated at a new size
        // (window resize rebuilds the framebuffers and drops their contents).
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

        m_cloudTaaShader->bind();
        m_cloudTaaShader->setTexture("u_CloudColor", cloudIt->second, 0);
        m_cloudTaaShader->setTexture("u_History", readBuffer->getColorTexture(0), 1);

        m_cloudTaaShader->setMat4("u_InvViewProj", glm::inverse(ctx.projMatrix * ctx.viewMatrix));
        m_cloudTaaShader->setMat4("u_PrevViewProj", ctx.prevViewProjMatrix);
        m_cloudTaaShader->setFloat3("u_CameraPos", ctx.cameraPosition);
        m_cloudTaaShader->setFloat3("u_PlanetCenter", ctx.planetCenter);
        m_cloudTaaShader->setFloat("u_PlanetRadius", a.planetRadius);
        m_cloudTaaShader->setFloat("u_CloudInnerRadius", inner);
        m_cloudTaaShader->setFloat("u_CloudOuterRadius", outer);

        // First frame / after resize: history is empty or stale — write the
        // current frame as-is so we don't blend against garbage.
        m_cloudTaaShader->setFloat("u_BlendAlpha", m_firstFrame ? 1.0f : 0.07f);
        m_cloudTaaShader->setFloat2("u_TexelSize", glm::vec2(
            1.0f / (float)writeBuffer->getWidth(),
            1.0f / (float)writeBuffer->getHeight()));

        m_fullscreenQuad->bind();
        RenderCommand::drawIndexed(m_fullscreenQuad);
        m_fullscreenQuad->unbind();

        m_cloudTaaShader->unbind();
        writeBuffer->unbind();

        // Overwrite the clouds key so HDRBufferPass composites the accumulated
        // cloud instead of the raw frame.
        ctx.inputTextures["clouds"] = writeBuffer->getColorTexture(0);

        m_writeIndex ^= 1;
        m_firstFrame = false;
    }

}
