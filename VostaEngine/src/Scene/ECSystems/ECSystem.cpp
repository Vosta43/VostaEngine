#include "vepch.h"
#include "ECSystem.h"
#include "Renderer/Camera.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"


namespace ve {

	RenderSystem::RenderSystem(const Ref<Scene>& scene){
		m_scene = scene;
		// TODO:
		m_renderPipeline.init(0,0);
		m_collector = CreateRef<SceneRenderer>(scene);
	}

	void RenderSystem::onUpdate(float deltaTime){

		RenderContext ctx;

		ctx.cameraPosition = m_scene->getCameraPosition();
		ctx.viewMatrix = m_scene->getViewMatrix();
		ctx.projMatrix = m_scene->getProjMatrix();

		ctx.deltaTime = deltaTime;

		// SceneRenderer is the one place scene data becomes draw commands.
		m_collector->collectAllMesh(ctx);
		m_collector->collectAllLight(ctx);
		m_collector->collectAllSprites(ctx);

		m_renderPipeline.render(ctx);
	}

	ECSystemRegistry* ECSystemRegistry::instance = nullptr;

	ECSystemRegistry* ECSystemRegistry::getInstance()
	{
		if (instance == nullptr) {
			instance = new ECSystemRegistry();
		}
		return instance;
	}

}
