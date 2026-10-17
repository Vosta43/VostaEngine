#include "vepch.h"
#include "GBufferPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/RuntimeVirtualTexture.h"
#include "Renderer/LayeredShaderBuilder.h"
#include "Renderer/Texture.h"
#include "Asset/BuiltinReousrces.h"

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

		// Opaque pass: every attachment must be written unconditionally. Some
		// material shaders emit alpha 0 on the normal/material targets, so an
		// inherited blend state would drop those writes and leave the cleared black.
		RenderCommand::setBlend(false);

		// Debug wireframe: rasterize triangles as outlines. Restored right after
		// the loop so the deferred full-screen passes and skybox stay filled.
		RenderCommand::setWireframe(ctx.wireframe);

		for (const auto& cmd : ctx.drawMeshCommands) {
			auto material = ResourceManager::get<Material>(cmd.materialHandle);
			if (!material) continue;

			// A live RVT already holds this surface's blended result, so a material with
			// an RVT path shades from the page table instead of re-evaluating its layers.
			// Otherwise it keeps its own shader; the pass default is the last fallback.
			const bool frameHasRVT = ctx.rvt && ctx.rvt->isInitialized();
			Ref<Shader> rvtShader = frameHasRVT ? material->getRVTShader() : nullptr;
			const bool useRvtShader = rvtShader && rvtShader->isValid();

			Ref<Shader> activeShader = useRvtShader ? rvtShader : material->getShader();
			if (!activeShader || !activeShader->isValid()) activeShader = m_shader;
			if (!activeShader || !activeShader->isValid()) continue;

			activeShader->bind();
			activeShader->setMat4("u_ViewProj", ctx.projMatrix * ctx.viewMatrix);
			activeShader->setMat4("u_Model", renderModelMatrix(cmd, ctx));
			activeShader->setFloat("u_Time", ctx.totalTime);
			// World-anchored material shaders (terrain layers, the RVT consumer) add
			// this back where they need absolute world XZ. Others don't declare it.
			activeShader->setFloat3("u_WorldOrigin", glm::vec3(ctx.renderOrigin));

			// The material decides what to bind; this pass only applies it. The
			// set is a member so its vectors keep their capacity across draws.
			material->fillBindings(m_bindings);
			applyMaterialBindings(activeShader, m_bindings);

			// Chart coordinate mode. A layered shader switches to globe mode only for
			// the planet tiles below; other materials don't declare this (no-op).
			activeShader->setInt("u_CoordMode", 0);

			// Last, because the RVT units overlap the ones the layer stack just
			// claimed: a consumer shader reads pages, not the material's textures.
			if (useRvtShader) bindRuntimeVirtualTexture(activeShader, *ctx.rvt);

			auto mesh = cmd.transientMesh;
			if (!mesh) mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
			if (mesh) {
				mesh->bind();
				mesh->draw(cmd.startIndex, cmd.indexCount);
				mesh->unbind();
			}

			activeShader->unbind();
		}

		// Prototype planet body: the LOD-selected cube-sphere tiles. Each tile's
		// vertices are tile-local and its centre is a planet-local sphere point, so
		// placement is a pure translate of (planetCenter + centerLocal) into render
		// space -- the absolute path, since a chart column at the sphere centre would
		// degenerate. Drawn inside the wireframe window on purpose, so the LOD set
		// (unequal-sized quads, cracks at boundaries) can be inspected directly.
		if (!ctx.globeTiles.empty()) {
			// Sphere mode needs both the material and its baked atlas; the terrain's
			// layered material gives the globe the same surface rules as the terrain.
			// Without both, the pass's default material still shapes a readable planet.
			const bool sphereMode = ctx.globeMaterial != nullptr && ctx.globeWeights != nullptr;
			Ref<Material> material = sphereMode
				? ctx.globeMaterial
				: ResourceManager::get<Material>(BuiltinResources::getDefaultMaterial());
			if (material) {
				Ref<Shader> globeShader = material->getShader();
				if (globeShader && globeShader->isValid()) {
					globeShader->bind();
					globeShader->setMat4("u_ViewProj", ctx.projMatrix * ctx.viewMatrix);
					globeShader->setFloat("u_Time", ctx.totalTime);
					globeShader->setFloat3("u_WorldOrigin", glm::vec3(ctx.renderOrigin));
					globeShader->setInt("u_CoordMode", sphereMode ? 1 : 0);

					material->fillBindings(m_bindings);
					applyMaterialBindings(globeShader, m_bindings);

					// Replace the material's own weight array with the direction-domain
					// atlas: same unit the bindings just used, so the rebind wins.
					if (sphereMode) {
						ctx.globeWeights->bind(LayeredShaderBuilder::kWeightsTextureUnit);
						globeShader->setInt("u_Weights", (int)LayeredShaderBuilder::kWeightsTextureUnit);
					}

					for (const auto& tile : ctx.globeTiles) {
						if (!tile.mesh) continue;
						globeShader->setMat4("u_Model", glm::translate(glm::mat4(1.0f),
							toRenderSpace(ctx.planetCenter + tile.centerLocal, ctx.renderOrigin)));
						tile.mesh->bind();
						tile.mesh->draw(0, 0);
						tile.mesh->unbind();
					}

					globeShader->unbind();
				}
			}
		}

		RenderCommand::setWireframe(false);

		m_target->unbind();

		setWritten(m_target);
	}

}
