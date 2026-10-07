#pragma once

#include "RenderPassBase.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"

namespace ve {

	// Quarter-res volumetric cloud raymarch. Writes RGBA where alpha is cloud
	// opacity (1 = fully transparent). No declared texture inputs: the noise/LUT
	// textures and every cloud uniform are pushed manually, as the ray march is
	// driven by engine state the binder has no vocabulary for.
	class CloudPass : public RenderPassBase {
	public:
		void init() override;
		void execute(RenderContext& ctx) override;

	private:
		Ref<VertexArray> m_fullscreenQuad;
	};

}
