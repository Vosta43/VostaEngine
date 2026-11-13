#include "vepch.h"
#include "PresentPass.h"
#include "PassBinding.h"

#include "Renderer/RenderCommand.h"

namespace ve {

	void PresentPass::execute(RenderContext& ctx) {
		Ref<Framebuffer> out = ctx.outputFrameBuffer;
		if (!out || !m_shader) return;
		if (!beginPass(ctx)) return;

		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, ctx.presentDiscardBackground ? 0.0f : 1.0f);
		RenderCommand::clear();

		m_shader->bind();
		bindPassInputs(*this, ctx);
		applyPassUniforms(*this);
		m_shader->setInt("u_DiscardBackground", ctx.presentDiscardBackground ? 1 : 0);

		// y=0 ground grid (editor view helper). The non-jittered VP keeps thin
		// lines from shimmering with the TAA jitter.
		if (ctx.groundGrid) {
			glm::mat4 vp = ctx.projMatrixNoJitter * ctx.viewMatrix;
			m_shader->setMat4("u_ViewProj", vp);
			m_shader->setMat4("u_InvViewProj", glm::inverse(vp));
			m_shader->setFloat3("u_CameraPos", toRenderSpaceChart(ctx.cameraPosition, ctx));
			m_shader->setFloat3("u_WorldOrigin", groundGridOrigin(ctx));
		}
		m_shader->setInt("u_GroundGrid", ctx.groundGrid ? 1 : 0);

		drawFullscreenQuad();

		m_shader->unbind();
		out->unbind();

		setWritten(out);
	}

}
