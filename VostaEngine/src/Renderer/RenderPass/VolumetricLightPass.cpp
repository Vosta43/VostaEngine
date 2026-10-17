#include "vepch.h"
#include "VolumetricLightPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"

#include <string>

namespace ve {

    void VolumetricLightPass::init() {
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

    void VolumetricLightPass::execute(RenderContext& ctx) {
        Ref<Framebuffer> writeBuffer = hasHistory() ? historyWrite(ctx.frameIndex) : m_target;
        if (!writeBuffer) return;

        // History is invalidated whenever the buffer is recreated at a new size
        // (a resize rebuilds the framebuffers and drops their contents).
        if (writeBuffer->getWidth() != m_lastWidth || writeBuffer->getHeight() != m_lastHeight)
            m_firstFrame = true;
        m_lastWidth = writeBuffer->getWidth();
        m_lastHeight = writeBuffer->getHeight();

        writeBuffer->bind();
        RenderCommand::setViewport(0, 0, writeBuffer->getWidth(), writeBuffer->getHeight());
        RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::clear();

        // Needs a sun (the shadow cascades only exist with an atmosphere) and a
        // medium dense enough to scatter. Otherwise the cleared black buffer is
        // what consumers get.
        const bool active = m_shader && ctx.hasVolumetricLight && ctx.hasAtmosphere
            && ctx.shadowCascadeCount > 0 && ctx.volumetric.density > 0.0f;

        if (active) {
            m_shader->bind();
            // u_DepthMap and u_ShadowMap0/1/2 come from the declared inputs.
            bindPassInputs(*this, ctx);
            applyPassUniforms(*this);   // SHADOW_SAMPLES

            m_shader->setMat4("u_InvViewProj", glm::inverse(ctx.projMatrix * ctx.viewMatrix));
            m_shader->setMat4("u_PrevViewProj", ctx.prevViewProjMatrix);
            m_shader->setMat4("u_ViewMatrix", ctx.viewMatrix);
            m_shader->setFloat3("u_CameraPos", toRenderSpaceChart(ctx.cameraPosition, ctx));
            m_shader->setFloat3("u_PlanetCenter", toRenderSpace(ctx.planetCenter, ctx.renderOrigin));

            const AtmosphereParams& a = ctx.atmosphere;
            m_shader->setFloat3("u_SunDirection", glm::normalize(a.sunDirection));
            m_shader->setFloat3("u_SunIntensity", a.sunIntensity * a.resolveSunColor());
            // Sky is not marched; its in-scatter follows from the direction only.
            // The sun color is the physical one, reddened by the atmosphere along
            // the sun ray (the transmittance LUT the HDR pass also samples).
            if (ctx.transmittanceTexture)
                m_shader->setTexture("u_TransmittanceLUT", ctx.transmittanceTexture, 9);

            const VolumetricLightParams& v = ctx.volumetric;
            m_shader->setFloat("u_Density", v.density);
            m_shader->setFloat("u_Scattering", v.scattering);
            m_shader->setFloat("u_Anisotropy", v.anisotropy);
            m_shader->setFloat("u_MaxDistance", v.maxDistance);
            m_shader->setInt("u_StepCount", v.stepCount);
            m_shader->setFloat("u_Intensity", v.intensity);
            m_shader->setFloat("u_HeightFalloff", v.heightFalloff);
            m_shader->setFloat("u_BaseHeight", v.baseHeight);

            // Cascaded shadow data, computed on the CPU this frame (no per-frame UBO).
            m_shader->setInt("u_ShadowCascadeCount", ctx.shadowCascadeCount);
            m_shader->setFloat4("u_ShadowSplitFar", glm::vec4(ctx.shadowSplitFar[0], ctx.shadowSplitFar[1], ctx.shadowSplitFar[2], 0.0f));
            m_shader->setFloat4("u_ShadowTexelWorld", glm::vec4(ctx.shadowTexelWorld[0], ctx.shadowTexelWorld[1], ctx.shadowTexelWorld[2], 0.0f));
            for (int i = 0; i < ctx.shadowCascadeCount; ++i)
                m_shader->setMat4("u_ShadowVP" + std::to_string(i), ctx.shadowLightVP[i]);

            // Temporal accumulation, reprojected from the scene depth in the shader.
            // The first active frame (and after a resize) writes the raw march.
            m_shader->setFloat("u_BlendAlpha", m_firstFrame ? 1.0f : (1.0f - v.temporalBlend));
            m_shader->setFloat2("u_TexelSize", glm::vec2(
                1.0f / (float)writeBuffer->getWidth(),
                1.0f / (float)writeBuffer->getHeight()));
            m_shader->setFloat("u_FrameIndex", (float)ctx.frameIndex);

            m_fullscreenQuad->bind();
            RenderCommand::drawIndexed(m_fullscreenQuad);
            m_fullscreenQuad->unbind();
            m_shader->unbind();
        }

        writeBuffer->unbind();
        setWritten(writeBuffer);
        // Toggling the feature (or a resize) must restart accumulation from the
        // raw march rather than blending against a stale/parked history.
        m_firstFrame = !active;
    }

}
