#include "vepch.h"
#include "UIRenderTarget.h"

#include "Renderer/Preprocess/BakeService.h"

#include <algorithm>

namespace ve {

    UIRenderTarget::UIRenderTarget(vellum::VellumLayer& layer, const glm::vec2& pixelSize)
        : m_layer(layer)
        , m_size(pixelSize) {
        m_textureId = m_layer.acquireTextureId();
        m_image = CreateRef<vellum::Image>();
        m_image->setTexture(m_textureId, m_size);
        m_image->setSize(m_size);
        // GL render targets store row 0 at the bottom; Image samples v = 0 as the top.
        m_image->setUV({ 0.0f, 1.0f }, { 1.0f, 0.0f });
    }

    UIRenderTarget::~UIRenderTarget() {
        m_layer.releaseTexture(m_textureId);
    }

    void UIRenderTarget::setSize(const glm::vec2& pixelSize) {
        if (pixelSize == m_size)
            return;
        m_size = pixelSize;
        m_image->setTexture(m_textureId, m_size);
        m_image->setSize(m_size);
    }

    void UIRenderTarget::ensureCreated(uint32_t width, uint32_t height) {
        if (!m_pipeline) {
            m_pipeline = CreateRef<RenderPipeline>();
            m_pipeline->init(width, height);
            m_framebuffer = Framebuffer::create(width, height);
        }
        else if (width != m_width || height != m_height) {
            m_pipeline->resize(width, height);
            m_framebuffer->resize(width, height);
        }
        else {
            return;
        }

        m_width = width;
        m_height = height;
        // resize() rebuilds the attachments, so the stable id must point at the new GL name.
        m_layer.registerTexture(m_textureId, m_framebuffer->getColorTexture(0));
        m_image->setTexture(m_textureId, m_size);
    }

    void UIRenderTarget::render(RenderContext& ctx) {
        const uint32_t width = std::max(1u, static_cast<uint32_t>(m_size.x));
        const uint32_t height = std::max(1u, static_cast<uint32_t>(m_size.y));
        ensureCreated(width, height);

        ctx.viewPortX = 0;
        ctx.viewPortY = 0;
        ctx.viewPortWidth = static_cast<int>(width);
        ctx.viewPortHeight = static_cast<int>(height);
        ctx.outputFrameBuffer = m_framebuffer;
        ctx.presentDiscardBackground = m_transparent;
        ctx.taaEnabled = m_taa;
        // Cloud/volumetric history ping-pongs on this.
        ctx.frameIndex = m_frameIndex++;

        if (m_defaultLighting && !ctx.hasAtmosphere && !ctx.irradianceMap) {
            auto& bake = BakeService::get();
            ctx.irradianceMap = bake.getIrradianceMap();
            ctx.prefilteredEnvMap = bake.getPrefilteredEnvMap();
            ctx.brdfLUT = bake.getBRDFLUT();
        }

        m_pipeline->render(ctx);
    }

}
