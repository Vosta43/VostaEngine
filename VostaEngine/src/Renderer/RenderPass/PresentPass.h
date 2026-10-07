#pragma once

#include "RenderPassBase.h"
#include "Renderer/VertexArray.h"

namespace ve {

	// Final blit to the caller's output framebuffer ("@default"). Replaces the
	// screen-blit block that used to live in SceneViewRenderer: resolves its
	// declared inputs (@previous colour, gbuffer depth) through the binder, then
	// draws one fullscreen quad. Clears the output first, so downstream content
	// (skybox, sprites) draws on top of a complete image.
	class PresentPass : public RenderPassBase {
	public:
		void init() override;
		void execute(RenderContext& ctx) override;

	private:
		Ref<VertexArray> m_fullscreenQuad;
	};

}
