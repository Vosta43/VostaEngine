#pragma once

#include "RenderPassBase.h"
#include "Renderer/RenderContext.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"

namespace ve {

    // Full-screen temporal anti-aliasing: reprojects the previous frame's
    // history onto the current camera, clamps it to the 3x3 neighborhood,
    // and blends. Geometry accumulates here; cloud pixels are skipped (their
    // GBuffer depth is the far plane) and CloudTAA accumulates them instead.
    class TAAPass : public RenderPassBase {
    public:
        void init() override;
        void execute(RenderContext& ctx) override;

        // Ping-pong pair. Each frame reads from one and writes the other,
        // so the previous output is always available as history.
        void setHistoryBuffers(const Ref<Framebuffer>& buffer0, const Ref<Framebuffer>& buffer1) {
            m_history[0] = buffer0;
            m_history[1] = buffer1;
        }

    private:
        Ref<Framebuffer> m_history[2];
        Ref<Shader>      m_taaShader;
        Ref<VertexArray> m_fullscreenQuad;

        int  m_writeIndex = 0;
        bool m_firstFrame = true;
        uint32_t m_lastWidth = 0;
        uint32_t m_lastHeight = 0;
    };

}
