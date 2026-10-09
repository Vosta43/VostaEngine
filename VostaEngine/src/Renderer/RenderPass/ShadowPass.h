#pragma once

#include "RenderPassBase.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"

namespace ve {

	// Depth-only geometry pass for one shadow cascade. Draws every static mesh
	// into its cascade's shadow map using this frame's light view-projection;
	// the HDR pass samples the result when lighting the direct sun term.
	class ShadowPass : public RenderPassBase {
	public:
		void init() override;
		void execute(RenderContext& ctx) override;

	private:
		// Cascade index, from the pass definition's `index` field.
		int m_cascade = 0;
	};

}
