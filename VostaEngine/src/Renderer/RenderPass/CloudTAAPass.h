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
    class CloudTAAPass : public RenderPassBase {
    public:
        void init() override;
        void execute(RenderContext& ctx) override;

        // Ping-pong pair holding the accumulated cloud. CloudPass writes the raw
        // quarter-res frame each frame; that raw buffer is the "current" input
        // and these two swap read/write so the previous average stays available.
        void setHistoryBuffers(const Ref<Framebuffer>& buffer0, const Ref<Framebuffer>& buffer1) {
            m_history[0] = buffer0;
            m_history[1] = buffer1;
        }

    private:
        Ref<Framebuffer> m_history[2];
        Ref<Shader>      m_cloudTaaShader;
        Ref<VertexArray> m_fullscreenQuad;

        int  m_writeIndex = 0;
        bool m_firstFrame = true;
        uint32_t m_lastWidth = 0;
        uint32_t m_lastHeight = 0;
    };

}
