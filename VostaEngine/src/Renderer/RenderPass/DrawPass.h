#pragma once

#include "RenderPassBase.h"
#include "Renderer/Material.h"

namespace ve {

	// Declarative geometry pass. Registered under one type name per geometry
	// source; the PassDef's "draw" block selects which commands are eligible.
	class DrawPass : public RenderPassBase {
	public:
		enum class Kind { Fullscreen, Meshes, Sprites, Globe, Skybox, Particles };

		explicit DrawPass(Kind kind) : m_kind(kind) {}

		void execute(RenderContext& ctx) override;

	private:
		void drawFullscreenPass(RenderContext& ctx);
		void drawMeshes(RenderContext& ctx);
		void drawSprites(RenderContext& ctx);
		void drawGlobe(RenderContext& ctx);
		void drawSkybox(RenderContext& ctx);
		void drawParticles(RenderContext& ctx);

		Kind m_kind;
		MaterialBindingSet m_bindings;
	};

}
