#pragma once

#include "RenderPassBase.h"
#include "Renderer/RenderContext.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"

namespace ve {

    // Temporal accumulation for the quarter-res volumetric cloud buffer. The
    // screen TAA cannot accumulate clouds: their GBuffer depth is the far plane
    // (sky), so the far-plane guard in taa.glsl excludes them and the quarter-res
    // undersampling shows up as shake/blur. Clouds get their own history here,
    // reprojected at the cloud's ENTRY distance (recomputed from the view ray +
    // shell in the shader), which is the correct parallax for a cloud shell.
    //
    // History ping-pong halves come from the pass's declared FBO pair; which half
    // is read/written follows the frame parity tracked by RenderPipeline.
    class CloudTAAPass : public RenderPassBase {
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
