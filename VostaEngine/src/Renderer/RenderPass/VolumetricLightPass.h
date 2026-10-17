#pragma once

#include "RenderPassBase.h"
#include "Renderer/RenderContext.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"

namespace ve {

    // Sun god-rays: a shadow-mapped ray march through a homogeneous participating
    // medium, marched at half resolution and temporally accumulated. Occlusion
    // comes from the scene's cascaded shadow maps, so shafts follow real geometry.
    //
    // Runs every frame and is never gated by `when`: the HDR pass samples this
    // pass's output, and a gate would leave a stale texture bound on that unit
    // (same reason cloud/cloudTaa run unconditionally). When nothing can be
    // marched it clears to black, so the composite adds nothing.
    class VolumetricLightPass : public RenderPassBase {
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
