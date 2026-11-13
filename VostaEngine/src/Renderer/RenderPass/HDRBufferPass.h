#pragma once

#include "RenderPassBase.h"
#include "Renderer/Shader.h"

namespace ve {

	// Deferred lighting pass: reads the GBuffer attachments, the accumulated
	// cloud buffer, the skybox/IBL cubes and the atmosphere LUTs, and writes
	// linear HDR. The GBuffer/cloud/skybox/IBL inputs are declared in the pass
	// description; the atmosphere LUTs and the light SSBO are pushed manually
	// (they are conditional and have no place in the generic binder).
	class HDRBufferPass : public RenderPassBase {
	public:
		void execute(RenderContext& ctx) override;

	private:
		int m_lastLightCount = -1;
	};

}
