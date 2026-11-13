#pragma once

#include "RenderPassBase.h"
#include "Renderer/RenderContext.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"

namespace ve {

    // Tonemap: reads the previous pass's linear HDR (@previous) and writes LDR.
    class PostBufferPass : public RenderPassBase {
    public:
        void execute(RenderContext& renderContext) override;
    };

}
