#include "vepch.h"
#include "GBufferPass.h"
#include "Renderer/RenderCommand.h"
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

			// A material may bring its own shader; otherwise the pass default.
			Ref<Shader> activeShader = material->getShader();
			if (!activeShader) activeShader = m_shader;

			activeShader->bind();
			activeShader->setMat4("u_ViewProj", ctx.projMatrix * ctx.viewMatrix);
			activeShader->setMat4("u_Model", cmd.transform);

			// The material decides what to bind; this pass only applies it. The
			// set is a member so its vectors keep their capacity across draws.
			material->fillBindings(m_bindings);

			for (const auto& t : m_bindings.textures) {
				auto tex = ResourceManager::get<Texture2D>(t.textureHandle);
				if (tex) activeShader->setTexture(t.uniformName, tex, t.unit);
			}
			for (const auto& b : m_bindings.ints)   activeShader->setInt(b.name, b.value);
			for (const auto& b : m_bindings.floats) activeShader->setFloat(b.name, b.value);
			for (const auto& b : m_bindings.vec2s)  activeShader->setFloat2(b.name, b.value);
			for (const auto& b : m_bindings.vec3s)  activeShader->setFloat3(b.name, b.value);

			auto mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
			if (mesh) {
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
