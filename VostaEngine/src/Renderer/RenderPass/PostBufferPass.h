#pragma once

#include "RenderPassBase.h"
#include "Renderer/RenderContext.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"

namespace ve {

    class PostBufferPass : public RenderPassBase {
    public:
        void init() override;
        void execute(RenderContext& renderContext) override;

        void setFramebuffer(const Ref<Framebuffer>& framebuffer) {
            m_postBuffer = framebuffer;
        }

    private:
        Ref<Framebuffer> m_postBuffer;
        Ref<Shader>      m_postShader;
        Ref<VertexArray> m_fullscreenQuad;
    };

}
