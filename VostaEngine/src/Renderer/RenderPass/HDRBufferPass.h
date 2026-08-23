#pragma once

#include "RenderPassBase.h"
#include "Renderer/Shader.h"

namespace ve {

	class HDRBufferPass : public RenderPassBase {
	public:
		void init() override;

		void execute(RenderContext& ctx) override;

		void setFramebuffer(const Ref<Framebuffer>& framebuffer) {
			m_HDRBuffer = framebuffer;
		}

	private:

		Ref<Framebuffer> m_HDRBuffer;
		Ref<Shader> m_HDRBufferShader;
		Ref<VertexArray> m_fullscreenQuad;
	};


}