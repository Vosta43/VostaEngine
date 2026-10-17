#include "vepch.h"
#include "RVTDebugPass.h"
#include "PassBinding.h"

#include "Renderer/Buffer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RuntimeVirtualTexture.h"

namespace ve {

	void RVTDebugPass::init() {
		float vertices[] = {
			-1.0f, -1.0f,
			 1.0f, -1.0f,
			 1.0f,  1.0f,
			-1.0f,  1.0f
		};
		uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

		m_fullscreenQuad = VertexArray::create();
		auto vb = VertexBuffer::create(vertices, sizeof(vertices));
		BufferLayout layout = { { ShaderDataType::Float2, "a_Position" } };
		vb->setLayout(layout);
		m_fullscreenQuad->addVertexBuffer(vb);

		auto ib = IndexBuffer::create(indices, 6);
		m_fullscreenQuad->setIndexBuffer(ib);
	}

	void RVTDebugPass::execute(RenderContext& ctx) {
		RuntimeVirtualTexture* rvt = ctx.rvt.get();
		if (!rvt || !rvt->isInitialized() || ctx.rvtDebugView == RuntimeVirtualTexture::kDebugNone)
			return;

		Ref<Framebuffer> out = ctx.outputFrameBuffer;
		if (!out || !m_shader || !m_shader->isValid()) return;

		out->bind();
		RenderCommand::setViewport(0, 0, out->getWidth(), out->getHeight());
		RenderCommand::setDepthTesting(false);
		RenderCommand::setBlend(false);
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		RenderCommand::clear();

		m_shader->bind();
		m_shader->setInt("u_DebugView", ctx.rvtDebugView);
		// The same binds the geometry pass uses, so the views cannot disagree.
		bindRuntimeVirtualTexture(m_shader, *rvt);

		m_fullscreenQuad->bind();
		RenderCommand::drawIndexed(m_fullscreenQuad);
		m_fullscreenQuad->unbind();

		m_shader->unbind();
		out->unbind();

		// Restore the process-global default: the host draws the skybox and sprites
		// into the same target right after the pipeline.
		RenderCommand::setDepthTesting(true);

		// No setWritten(): this is an overlay into the target, not a pipeline stage,
		// so @previous must keep pointing at the presentation.
	}

}
