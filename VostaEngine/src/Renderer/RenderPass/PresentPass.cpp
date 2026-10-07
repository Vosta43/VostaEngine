#include "vepch.h"
#include "PresentPass.h"
#include "PassBinding.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"

namespace ve {

	void PresentPass::init() {
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

	void PresentPass::execute(RenderContext& ctx) {
		Ref<Framebuffer> out = ctx.outputFrameBuffer;
		if (!out || !m_shader) return;

		out->bind();
		RenderCommand::setViewport(0, 0, out->getWidth(), out->getHeight());
		if (ctx.presentDiscardBackground)
			RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		else
			RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		RenderCommand::clear();

		m_shader->bind();
		bindPassInputs(*this, ctx);
		applyPassUniforms(*this);
		m_shader->setInt("u_DiscardBackground", ctx.presentDiscardBackground ? 1 : 0);

		m_fullscreenQuad->bind();
		RenderCommand::drawIndexed(m_fullscreenQuad);
		m_fullscreenQuad->unbind();

		m_shader->unbind();
		out->unbind();

		setWritten(out);
	}

}
