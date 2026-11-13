#include "vepch.h"
#include "RVTDebugPass.h"
#include "PassBinding.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/RuntimeVirtualTexture.h"

namespace ve {

	void RVTDebugPass::execute(RenderContext& ctx) {
		RuntimeVirtualTexture* rvt = ctx.rvt.get();
		if (!rvt || !rvt->isInitialized() || ctx.rvtDebugView == RuntimeVirtualTexture::kDebugNone)
			return;

		Ref<Framebuffer> out = ctx.outputFrameBuffer;
		if (!out || !m_shader || !m_shader->isValid()) return;
		if (!beginPass(ctx)) return;

		RenderCommand::setDepthTesting(false);
		RenderCommand::setBlend(false);
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		RenderCommand::clear();

		m_shader->bind();
		m_shader->setInt("u_DebugView", ctx.rvtDebugView);
		// The same binds the geometry pass uses, so the views cannot disagree.
		bindRuntimeVirtualTexture(m_shader, *rvt);

		drawFullscreenQuad();

		m_shader->unbind();
		out->unbind();

		// Restore the process-global default for the next frame's passes.
		RenderCommand::setDepthTesting(true);

		// No setWritten(): this is an overlay into the target, not a pipeline stage,
		// so @previous must keep pointing at the presentation.
	}

}
