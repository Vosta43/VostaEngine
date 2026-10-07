#pragma once

#include "RenderPassBase.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Shader.h"

namespace ve {

	// Deferred geometry pass. Writes albedo/normal/material into the "gbuffer"
	// target's colour attachments and depth into its depth attachment; downstream
	// passes read it by name.
	class GBufferPass : public RenderPassBase {
	public:
		void init() override;
		void execute(RenderContext& ctx) override;
	};

}
