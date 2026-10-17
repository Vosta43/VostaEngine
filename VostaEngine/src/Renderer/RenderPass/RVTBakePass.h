#pragma once

#include "RenderPassBase.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Material.h"
#include "Core/AssetHandle.h"

#include <vector>

namespace ve {

	// Fills the RVT's physical pages. Declares no screen target: it binds its own bake
	// framebuffer, so it must not disturb the ping-pong chain. Per requested page it
	// re-points the bake target at that page's slot and re-runs the terrain's layered
	// material under a top-down ortho projection over the page's world rect.
	class RVTBakePass : public RenderPassBase {
	public:
		void init() override;
		void execute(RenderContext& ctx) override;

	private:
		// Scratch bindings, reused across draws to keep capacity.
		MaterialBindingSet m_bindings;
		// Meshes already drawn for the page in flight, so one tile's LOD chunks do not
		// each redraw the whole mesh.
		std::vector<AssetHandle> m_drawnMeshes;
	};

}
