#pragma once

#include "Core/Core.h"
#include "RenderPassBase.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"

namespace ve {
	
	class GBufferPass : public RenderPassBase {
	public:
		void init() override;
		
		void execute(RenderContext& ctx) override;
		
		void setFramebuffer(const Ref<Framebuffer>& framebuffer) {
			m_GBuffer = framebuffer;
		}

	private:
		Ref<Framebuffer> m_GBuffer;
		Ref<Shader> m_GBufferShader;
	};


}