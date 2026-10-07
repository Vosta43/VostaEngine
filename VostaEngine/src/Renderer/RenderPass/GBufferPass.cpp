#include "vepch.h"
#include "GBufferPass.h"
#include "Renderer/RenderCommand.h"
#include "Core/Application.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/Texture.h"

namespace ve {
	void GBufferPass::init() {
	}

	void GBufferPass::execute(RenderContext& ctx){

		if (!m_target) {
			return;
		}

		m_target->bind();
		RenderCommand::setViewport(ctx.viewPortX,ctx.viewPortY,ctx.viewPortWidth,ctx.viewPortHeight);
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		RenderCommand::clear();

		if (!m_shader) {
			return;
		}

		// Debug wireframe: rasterize triangles as outlines. Restored right after
		// the loop so the deferred full-screen passes and skybox stay filled.
		RenderCommand::setWireframe(ctx.wireframe);

		for (const auto& cmd : ctx.drawMeshCommands) {
			auto material = ResourceManager::get<Material>(cmd.materialHandle);
			if (!material) continue;

			// Use material-graph compiled shader when available, otherwise default gbuffer shader.
			Ref<Shader> activeShader = material->customShader ? material->customShader : m_shader;
			activeShader->bind();
			activeShader->setMat4("u_ViewProj", ctx.projMatrix * ctx.viewMatrix);
			activeShader->setMat4("u_Model", cmd.transform);

			if (material->albedoMapHandle.isValid())
			{
				auto albedoMap = ResourceManager::get<Texture2D>(material->albedoMapHandle);
				if (albedoMap)
				{
					activeShader->setTexture("u_AlbedoMap", albedoMap,0);
					activeShader->setInt("u_UseAlbedoMap", 1);
				}
			}
			else
			{
				activeShader->setFloat3("u_AlbedoColor", material->albedoColor);
				activeShader->setInt("u_UseAlbedoMap", 0);
			}
			if (material->normalMapHandle.isValid())
			{
				auto normalMap = ResourceManager::get<Texture2D>(material->normalMapHandle);
				if (normalMap)
				{
					activeShader->setTexture("u_NormalMap", normalMap,1);
					activeShader->setInt("u_UseNormalMap", 1);
				}
			}
			else
			{
				activeShader->setInt("u_UseNormalMap", 0);
			}
			if (material->metallicMapHandle.isValid())
			{
				auto metallicMap = ResourceManager::get<Texture2D>(material->metallicMapHandle);
				if (metallicMap)
				{
					activeShader->setTexture("u_MetallicMap", metallicMap,2);
					activeShader->setInt("u_UseMetallicMap", 1);
				}
			}
			else
			{
				activeShader->setFloat("u_Metallic", material->metallic);
				activeShader->setInt("u_UseMetallicMap", 0);
			}
			if (material->roughnessMapHandle.isValid())
			{
				auto roughnessMap = ResourceManager::get<Texture2D>(material->roughnessMapHandle);
				if (roughnessMap)
				{
					activeShader->setTexture("u_RoughnessMap", roughnessMap,3);
					activeShader->setInt("u_UseRoughnessMap", 1);
				}
			}
			else
			{
				activeShader->setFloat("u_Roughness", material->roughness);
				activeShader->setInt("u_UseRoughnessMap", 0);
			}

			// AO map
			if (material->aoMapHandle.isValid())
			{
				auto aoMap = ResourceManager::get<Texture2D>(material->aoMapHandle);
				if (aoMap)
				{
					activeShader->setTexture("u_AOMap", aoMap,4);
					activeShader->setInt("u_UseAOMap", 1);
				}
			}
			else
			{
				activeShader->setFloat("u_AO", material->ao);
				activeShader->setInt("u_UseAOMap", 0);
			}

			// Bind graph textures (from MaterialAsset compilation)
			uint32_t graphTexSlot = 5;
			for (const auto& binding : material->graphTextures) {
				auto tex = ResourceManager::get<Texture2D>(binding.textureHandle);
				if (tex) {
					activeShader->setTexture(binding.uniformName, tex, graphTexSlot);
				}
				++graphTexSlot;
			}

			// Draw mesh
			auto mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
			if (mesh)
			{
				mesh->bind();
				mesh->draw(cmd.startIndex, cmd.indexCount);
				mesh->unbind();
			}

			activeShader->unbind();
		}

		RenderCommand::setWireframe(false);

		m_target->unbind();

		setWritten(m_target);
	}

}