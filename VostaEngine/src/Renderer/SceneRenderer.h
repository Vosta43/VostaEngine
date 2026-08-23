#pragma once

#include "Core/Core.h"
#include "Scene/Scene.h"
#include "RenderContext.h"

// Translates ECS scene data into GPU-ready draw commands.
// Traverses the Scene's entity registry to collect visible meshes, lights,
// and their material bindings into a RenderContext consumed by RenderPipeline.
//
// Responsibilities:
// - Frustum / distance culling (future)
// - LOD selection (future)
// - Material override resolution per submesh
// - Light collection and attenuation setup
//
// Does NOT own scene data, submit commands to the GPU, or manage
// render passes. Scene is read-only during traversal.

namespace ve {
	
	class VE_API SceneRenderer {
	public:
		SceneRenderer(const Ref<Scene>& scene);

			void collectAllMesh(RenderContext& ctx);
		void collectAllLight(RenderContext& ctx);
		void collectAllSprites(RenderContext& ctx);

	private:
		Ref<Scene> m_scene;		
	};

}