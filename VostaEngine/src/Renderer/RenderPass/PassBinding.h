#pragma once

#include "Renderer/RenderPass/RenderPassBase.h"
#include "Renderer/RenderContext.h"
#include "Renderer/Material.h"

namespace ve {

	class RuntimeVirtualTexture;

	// Resolves every input a pass declared in its PassDef and binds it to the
	// pass's shader. Sources are either @-sentinel (@previous / @history /
	// @default plus the engine texture vocabulary) or a name matched against the
	// pass-output table and then the FBO registry. An unresolved source warns and
	// is skipped -- it never silently binds garbage.
	void bindPassInputs(RenderPassBase& pass, RenderContext& ctx);

	// Pushes the constant uniforms a pass declared in PassDef::uniforms.
	void applyPassUniforms(RenderPassBase& pass);

	// Applies a material's filled binding set to `shader`: resolves every texture
	// handle through the resource manager, then pushes the scalar/vector uniforms.
	// Shared by the geometry pass and the RVT bake pass so both replay a material
	// identically. Does not bind the shader.
	void applyMaterialBindings(const Ref<Shader>& shader, const MaterialBindingSet& bindings);

	// Binds the frame's RVT: page table, physical arrays, and the grid uniforms.
	// Its units overlap the layered material's content samplers, so call this after
	// applyMaterialBindings -- last write wins.
	void bindRuntimeVirtualTexture(const Ref<Shader>& shader, const RuntimeVirtualTexture& rvt);

}
