#include "vepch.h"
#include "PostBufferPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"

namespace ve {

    void PostBufferPass::execute(RenderContext& ctx)
    {
        if (!m_shader)
            return;
        if (!beginPass(ctx))
            return;

        m_shader->bind();
        bindPassInputs(*this, ctx);
        applyPassUniforms(*this);
        m_shader->setFloat("u_Exposure", 1.0f);

        drawFullscreenQuad();

        m_shader->unbind();

        m_target->unbind();
        setWritten(m_target);
    }

}
