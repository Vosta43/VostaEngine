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
    //
    // History ping-pong halves come from the pass's declared FBO pair; which half
    // is read/written follows the frame parity tracked by RenderPipeline.
    class TAAPass : public RenderPassBase {
    public:
        void init() override;
        void execute(RenderContext& ctx) override;

    private:
        Ref<VertexArray> m_fullscreenQuad;

        bool     m_firstFrame = true;
        uint32_t m_lastWidth = 0;
        uint32_t m_lastHeight = 0;
    };

}
