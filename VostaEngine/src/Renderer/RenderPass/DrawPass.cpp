#include "vepch.h"
#include "DrawPass.h"
#include "PassBinding.h"

#include "Core/ResourceManager.h"
#include "Particle/ParticleBillboard.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RuntimeVirtualTexture.h"

namespace ve {

	void DrawPass::execute(RenderContext& ctx) {
		if (!beginPass(ctx)) return;

		switch (m_kind) {
			case Kind::Fullscreen: drawFullscreenPass(ctx); break;
			case Kind::Meshes:     drawMeshes(ctx); break;
			case Kind::Sprites:    drawSprites(ctx); break;
			case Kind::Globe:      drawGlobe(ctx); break;
			case Kind::Skybox:     drawSkybox(ctx); break;
			case Kind::Particles:  drawParticles(ctx); break;
		}

		m_boundTarget->unbind();
		setWritten(m_boundTarget);
	}

	void DrawPass::drawFullscreenPass(RenderContext& ctx) {
		if (!m_shader || !m_shader->isValid()) return;

		m_shader->bind();
		bindPassInputs(*this, ctx);
		applyPassUniforms(*this);
		drawFullscreenQuad();
		m_shader->unbind();
	}

	void DrawPass::drawMeshes(RenderContext& ctx) {
		const UniformSourceRegistry& sources = UniformSourceRegistry::builtin();

		RenderCommand::setWireframe(ctx.wireframe);

		for (const auto& cmd : ctx.drawMeshCommands) {
			if ((cmd.renderLayer & m_def.draw.layerMask) == 0) continue;

			Ref<Material> material;
			Ref<Shader> shader;
			bool rvtShaderUsed = false;

			if (m_def.draw.materialShader) {
				material = ResourceManager::get<Material>(cmd.materialHandle);
				if (!material) continue;

				Ref<Shader> rvtShader = (m_def.draw.rvt && ctx.rvt && ctx.rvt->isInitialized())
					? material->getRVTShader() : nullptr;
				rvtShaderUsed = rvtShader && rvtShader->isValid();
				shader = rvtShaderUsed ? rvtShader : material->getShader();
				if (!shader || !shader->isValid()) shader = m_shader;
			}
			else {
				shader = m_shader;
			}
			if (!shader || !shader->isValid()) continue;

			shader->bind();
			bindPassInputs(*this, ctx, shader);
			applyPassUniforms(*this, shader);

			const glm::mat4 model = renderModelMatrix(cmd, ctx);
			const UniformSourceContext src{ &ctx, &cmd, nullptr, &model };
			for (const auto& du : m_def.draw.uniforms)
				sources.apply(shader, du.uniform, du.source, src);

			if (material) {
				material->fillBindings(m_bindings);
				applyMaterialBindings(shader, m_bindings);
			}

			// Last: the RVT units overlap the ones the material just claimed.
			if (rvtShaderUsed) bindRuntimeVirtualTexture(shader, *ctx.rvt);

			auto mesh = cmd.transientMesh;
			if (!mesh) mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
			if (mesh) {
				mesh->bind();
				mesh->draw(cmd.startIndex, cmd.indexCount);
				mesh->unbind();
			}

			shader->unbind();
		}

		RenderCommand::setWireframe(false);
	}

	void DrawPass::drawSprites(RenderContext& ctx) {
		if (ctx.drawSpriteCommands.empty()) return;

		Renderer2D::beginScene(ctx.projMatrix * ctx.viewMatrix);
		for (const auto& cmd : ctx.drawSpriteCommands) {
			if ((cmd.renderLayer & m_def.draw.layerMask) == 0) continue;
			Renderer2D::drawQuad(renderModelMatrix(cmd.transform, cmd.worldPosition, ctx),
			                     cmd.size, cmd.textureHandle);
		}
		Renderer2D::endScene();
	}

	void DrawPass::drawGlobe(RenderContext& ctx) {
		if (ctx.globeTiles.empty()) return;

		RenderCommand::setWireframe(ctx.wireframe);
		drawGlobeTiles(ctx, m_bindings);
		RenderCommand::setWireframe(false);
	}

	void DrawPass::drawSkybox(RenderContext& ctx) {
		if (!ctx.skyboxTexture) return;

		Renderer3D::setSkyboxTexture(ctx.skyboxTexture);
		Renderer3D::drawSkybox(ctx.viewMatrix, ctx.projMatrix);
	}

	void DrawPass::drawParticles(RenderContext& ctx) {
		if (ctx.drawParticleCommands.empty()) return;

		const glm::mat3 assetToChart = ParticleBillboard::spaceToWorld();
		for (const auto& cmd : ctx.drawParticleCommands) {
			if ((cmd.renderLayer & m_def.draw.layerMask) == 0) continue;
			if (!cmd.asset || !cmd.data) continue;

			const ve::Vec3 origin = cmd.origin;
			auto toRenderSpace = [&ctx, &assetToChart, origin](const glm::vec3& local) {
				return toRenderSpaceChart(origin + ve::Vec3(assetToChart * local), ctx);
			};
			ParticleBillboard::draw(*cmd.asset, *cmd.data, toRenderSpace, cmd.tint, cmd.size);
		}

		ParticleBillboard::flush(ctx.viewMatrix, ctx.projMatrix);
	}

}
