#include "vepch.h"
#include "Scene.h"
#include "Components.h"

#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Texture.h"
#include "Core/Application.h"
#include "Renderer/CameraController.h"
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>


namespace ve {
	
	Scene::Scene(){
	
		


		
	}
	Scene::~Scene() {}

	void Scene::onComponentAssigned(std::type_index type, Entity entity) {
		if (const ComponentTypeInfo* info = findComponentType(type))
			if (info->initialize)
				info->initialize(m_entityRegistry, entity);
	}

	void Scene::onUpdate(float deltaTime){

		m_ecsystemGraph.onUpdate(deltaTime);
	}

	//void Scene::onUpdate(float deltaTime) {
	//	auto group = m_entityRegistry.group<TransformComponent,SpriteRendererComponent>();
	//	for (auto& et : group) {
	//		auto& transform = m_entityRegistry.get<TransformComponent>(et);
	//		auto& sprite = m_entityRegistry.get<SpriteRendererComponent>(et);

	//		transform.transform = glm::rotate(transform.transform, deltaTime * 1.0f, glm::vec3(0, 1, 0));

	//		Renderer2D::drawQuad(transform.transform, sprite.size, sprite.textureHandle, glm::vec4(1.0f));
	//	}
	//
	//	auto meshGroup = m_entityRegistry.group<TransformComponent, StaticMeshComponent>();
	//	for (auto& et : meshGroup) {
	//		auto& transform = m_entityRegistry.get<TransformComponent>(et);
	//		auto& meshComp = m_entityRegistry.get<StaticMeshComponent>(et);
	//		Renderer3D::drawMesh(transform.transform, meshComp.staticMeshHandle);
	//	}

	//	auto terrainGroup = m_entityRegistry.group<TransformComponent, TerrainComponent>();
	//	for (auto& et : terrainGroup) {
	//		auto& transform = m_entityRegistry.get<TransformComponent>(et);
	//		auto& terrainComp = m_entityRegistry.get<TerrainComponent>(et);
	//		if (terrainComp.generatedMeshHandle.isValid()) {
	//			Renderer3D::drawMesh(transform.transform, terrainComp.generatedMeshHandle);
	//		}
	//	}

	//	auto skyView = m_entityRegistry.view<SkyBoxComponent>();
	//	if (!skyView.empty()) {
	//		auto entity = *skyView.begin();
	//		auto& skyComp = m_entityRegistry.get<SkyBoxComponent>(entity);

	//		Ref<TextureCubeMap> cubemap = ResourceManager::get<TextureCubeMap>(skyComp.textureCubeMapHandle);
	//		if (cubemap) {
	//			// Retrieve current camera matrices
	//			Renderer3D::setSkyboxTexture(cubemap);
	//			Renderer3D::drawSkybox(m_viewMatrix, m_projMatrix);
	//		}
	//	}


	//}

	void Scene::setCameraMatrices(const glm::mat4& view, const glm::mat4& proj) {
		m_viewMatrix = view;
		m_projMatrix = proj;
	}

	void Scene::setCameraPosition(const glm::vec3& pos)
	{	
		m_cameraPosition = pos;
	}

	
	void Scene::onPickingRender() {

		auto view = m_entityRegistry.group<TransformComponent, SpriteRendererComponent>();
		for (auto entity : view) {
			auto& transform = m_entityRegistry.get<TransformComponent>(entity);
			auto& sprite = m_entityRegistry.get<SpriteRendererComponent>(entity);

			glm::mat4 tf = transform.transform;
			glm::vec2 size = sprite.size;
			uint32_t entityID = (uint32_t)entity; 

			Renderer2D::drawPickingQuad(tf, size, entityID);
		}
		
		auto meshGroup = m_entityRegistry.group<TransformComponent, StaticMeshComponent>();
		for (auto& et : meshGroup) {
			auto& transform = m_entityRegistry.get<TransformComponent>(et);
			auto& meshComp = m_entityRegistry.get<StaticMeshComponent>(et);
			uint32_t entityID = (uint32_t)et;
			Renderer3D::drawPickingMesh(transform.transform, meshComp.staticMeshHandle, entityID);
		}

		auto terrainGroup = m_entityRegistry.group<TransformComponent, TerrainComponent>();
		for (auto& et : terrainGroup) {
			auto& transform = m_entityRegistry.get<TransformComponent>(et);
			auto& terrainComp = m_entityRegistry.get<TerrainComponent>(et);
			if (terrainComp.generatedMeshHandle.isValid()) {
				uint32_t entityID = (uint32_t)et;
				Renderer3D::drawPickingMesh(transform.transform, terrainComp.generatedMeshHandle, entityID);
			}
		}

	}

	Entity Scene::createEntity() {
		return m_entityRegistry.create();
	}

	Entity Scene::getPrimaryCameraEntity() {
		Entity fallback;   // invalid by default
		for (uint32_t id : m_entityRegistry.view<CameraComponent>()) {
			if (m_entityRegistry.get<CameraComponent>(id).primary)
				return m_entityRegistry.getEntity(id);
			if (fallback.getId() == 0xFFFFFFFFu)
				fallback = m_entityRegistry.getEntity(id);
		}
		return fallback;
	}
}