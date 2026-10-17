#pragma once

#include "RenderPassBase.h"
#include "Renderer/VertexArray.h"

namespace ve {

	// Draws the RVT top-down over the whole virtual grid, for inspecting the bake
	// independent of the scene camera. Gated on the "rvtDebug" condition; writes to
	// @default so it replaces the presented image while active.
	//
	// It samples the page table exactly as a real consumer does, so it doubles as a
	// dry run of the sampling path.
	class RVTDebugPass : public RenderPassBase {
	public:
		void init() override;
		void execute(RenderContext& ctx) override;

	private:
		Ref<VertexArray> m_fullscreenQuad;
	};

}
