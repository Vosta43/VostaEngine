#pragma once

#include "Renderer/RenderPass/RenderPassBase.h"
#include "Renderer/RenderContext.h"

namespace ve {

	// Resolves every input a pass declared in its PassDef and binds it to the
	// pass's shader. Sources are either @-sentinels (@previous / @history /
	// @default plus the engine texture vocabulary) or a name matched against the
	// pass-output table and then the FBO registry. An unresolved source warns and
	// is skipped -- it never silently binds garbage.
	void bindPassInputs(RenderPassBase& pass, RenderContext& ctx);

	// Pushes the constant uniforms a pass declared in PassDef::uniforms.
	void applyPassUniforms(RenderPassBase& pass);

}
