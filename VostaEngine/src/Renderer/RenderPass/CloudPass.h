#pragma once

#include "RenderPassBase.h"
#include "Renderer/Shader.h"

namespace ve {

	class CloudPass : public RenderPassBase {
	public:
		void init() override;

		void execute(RenderContext& ctx) override;

		void setFramebuffer(const Ref<Framebuffer>& framebuffer) {
			m_cloudBuffer = framebuffer;
		}

	private:

		Ref<Framebuffer> m_cloudBuffer;
		Ref<Shader> m_cloudShader;
		Ref<VertexArray> m_fullscreenQuad;
	};


}
