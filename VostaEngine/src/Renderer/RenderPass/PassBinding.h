#pragma once

#include "Renderer/RenderPass/RenderPassBase.h"
#include "Renderer/RenderContext.h"
#include "Renderer/Material.h"

#include <functional>
#include <unordered_map>

namespace ve {

	class RuntimeVirtualTexture;

	// What a uniform source may read. `model` is the resolved draw model matrix
	// (planet placement included), for mesh and sprite draws alike.
	struct UniformSourceContext {
		const RenderContext*     ctx = nullptr;
		const DrawMeshCommand*   mesh = nullptr;
		const DrawSpriteCommand* sprite = nullptr;
		const glm::mat4*         model = nullptr;
	};

	using UniformSourceFn = std::function<void(const Ref<Shader>&, const std::string& uniform,
	                                           const UniformSourceContext&)>;

	// Maps a "source" string (e.g. @camera.viewProj, @mesh.model) to a shader
	// upload. Builtins are registered once; a pass may resolve one per draw.
	class UniformSourceRegistry {
	public:
		void add(const std::string& source, UniformSourceFn fn);
		bool apply(const Ref<Shader>& shader, const std::string& uniform,
		           const std::string& source, const UniformSourceContext& c) const;

		static const UniformSourceRegistry& builtin();

	private:
		std::unordered_map<std::string, UniformSourceFn> m_sources;
	};

	inline void UniformSourceRegistry::add(const std::string& source, UniformSourceFn fn) {
		m_sources[source] = std::move(fn);
	}

	// Resolves every input a pass declared in its PassDef and binds it to the
	// pass's shader. Sources are either @-sentinel (@previous / @history /
	// @default plus the engine texture vocabulary) or a name matched against the
	// pass-output table and then the FBO registry. An unresolved source warns and
	// is skipped -- it never silently binds garbage.
	void bindPassInputs(RenderPassBase& pass, RenderContext& ctx);
	// Same, for a shader other than the pass's own (a per-draw material shader).
	void bindPassInputs(RenderPassBase& pass, RenderContext& ctx, const Ref<Shader>& shader);

	// Pushes the constant uniforms a pass declared in PassDef::uniforms.
	void applyPassUniforms(RenderPassBase& pass);
	void applyPassUniforms(RenderPassBase& pass, const Ref<Shader>& shader);

	// Draws the frame's cube-sphere tiles with the globe's material. Shared by
	// the mesh draw pass and the globe pass so their shading cannot diverge.
	void drawGlobeTiles(RenderContext& ctx, MaterialBindingSet& scratch);

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
