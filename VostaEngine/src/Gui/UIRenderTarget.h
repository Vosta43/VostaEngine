#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/VellumLayer.h"
#include "Gui/Vellum/Widgets/Image.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderPipeline.h"

#include <glm.hpp>

namespace ve {

    // An offscreen pipeline target displayed by a Vellum Image. The host fills a
    // RenderContext and calls render() from onUpdate, before the UI phase.
    // Needs a live GL context and RenderCommand::init (any SceneView host has both).
    class VE_API UIRenderTarget {
    public:
        UIRenderTarget(vellum::VellumLayer& layer, const glm::vec2& pixelSize);
        ~UIRenderTarget();

        const Ref<vellum::Image>& image() const { return m_image; }

        void setSize(const glm::vec2& pixelSize);
        const glm::vec2& size() const { return m_size; }

        // Supplies default IBL maps when the context has neither an atmosphere nor its own.
        void setDefaultLighting(bool enabled) { m_defaultLighting = enabled; }
        // Clears to transparent and discards sky, so the widget can sit on a panel.
        void setTransparentBackground(bool enabled) { m_transparent = enabled; }
        // Off by default: a rotating object has no motion vectors, so TAA smears it.
        void setTAAEnabled(bool enabled) { m_taa = enabled; }

        // Camera, draw commands, lights and deltaTime/totalTime are the caller's.
        void render(RenderContext& ctx);

        const Ref<Framebuffer>& framebuffer() const { return m_framebuffer; }
        const Ref<RenderPipeline>& pipeline() const { return m_pipeline; }

    private:
        void ensureCreated(uint32_t width, uint32_t height);

        vellum::VellumLayer& m_layer;
        vellum::TextureId     m_textureId = 0;
        Ref<vellum::Image>    m_image;
        Ref<Framebuffer>      m_framebuffer;
        Ref<RenderPipeline>   m_pipeline;
        glm::vec2 m_size{ 1.0f };
        uint32_t  m_width = 0;
        uint32_t  m_height = 0;
        uint32_t  m_frameIndex = 0;
        bool      m_defaultLighting = true;
        bool      m_transparent = true;
        bool      m_taa = false;
    };

}
