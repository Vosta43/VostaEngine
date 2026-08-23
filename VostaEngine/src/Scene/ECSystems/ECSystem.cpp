#include "vepch.h"
#include "ECSystem.h"
#include "Renderer/Camera.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"


namespace ve {

	RenderSystem::RenderSystem(const Ref<Scene>& scene){
		m_scene = scene;
		// TODO:
		m_renderPipeline.init(0,0);
	}

	void RenderSystem::onUpdate(float deltaTime){

		RenderContext ctx;

		ctx.cameraPosition = m_scene->getCameraPosition();
		ctx.viewMatrix = m_scene->getViewMatrix();
		ctx.projMatrix = m_scene->getProjMatrix();

		ctx.deltaTime = deltaTime;

		auto meshGroup = m_scene->getRegistry().group<TransformComponent, StaticMeshComponent>();
		for (auto& et : meshGroup) {
			auto& transform = m_scene->getRegistry().get<TransformComponent>(et);
			auto& meshComp = m_scene->getRegistry().get<StaticMeshComponent>(et);

			DrawMeshCommand dmc;
			dmc.meshHandle = meshComp.staticMeshHandle;
			dmc.transform = transform.transform;
			ctx.drawMeshCommands.push_back(dmc);

			//Renderer3D::drawMesh(transform.transform, meshComp.staticMeshHandle);
		}

		auto terrainGroup = m_scene->getRegistry().group<TransformComponent, TerrainComponent>();
		for (auto& et : terrainGroup) {
			auto& transform = m_scene->getRegistry().get<TransformComponent>(et);
			auto& terrainComp = m_scene->getRegistry().get<TerrainComponent>(et);

			if (!terrainComp.generatedMeshHandle.isValid()) continue;

			auto mesh = ResourceManager::get<StaticMesh>(terrainComp.generatedMeshHandle);
			if (!mesh) continue;

			DrawMeshCommand dmc;
			dmc.meshHandle = terrainComp.generatedMeshHandle;
			dmc.materialHandle = terrainComp.terrainMaterialHandle;
			dmc.transform = transform.transform;
			ctx.drawMeshCommands.push_back(dmc);
		}

		auto lightGroup = m_scene->getRegistry().group<TransformComponent,LightComponent>();
		for (auto& et : lightGroup) {
			auto& transformComp = m_scene->getRegistry().get<TransformComponent>(et);
			auto& lightComp = m_scene->getRegistry().get<LightComponent>(et);

			DrawLightCommand dlc;
			dlc.light = lightComp.light;
			dlc.position = glm::vec3(transformComp.transform[3]);
			ctx.drawLightCommands.push_back(dlc);
		}




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
