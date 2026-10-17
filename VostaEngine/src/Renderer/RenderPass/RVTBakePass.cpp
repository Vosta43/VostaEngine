#include "vepch.h"
#include "RVTBakePass.h"
#include "PassBinding.h"

#include "Core/ResourceManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RuntimeVirtualTexture.h"
#include "Renderer/StaticMesh.h"

#include <algorithm>

namespace ve {

	void RVTBakePass::init() {
	}

	void RVTBakePass::execute(RenderContext& ctx) {
		RuntimeVirtualTexture* rvt = ctx.rvt.get();
		if (!rvt || !rvt->isInitialized()) return;

		const auto& reqs = rvt->bakeRequests();
		if (reqs.empty()) return;

		Ref<Framebuffer> fbo = rvt->bakeFramebuffer();
		if (!fbo) return;

		const int physSize = rvt->physicalPageSize();

		// The bake is a pure XZ projection: no depth, no blending, no wireframe.
		RenderCommand::setDepthTesting(false);
		RenderCommand::setBlend(false);
		RenderCommand::setWireframe(false);
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		for (const auto& req : reqs) {
			// World XZ -> clip, mapping the page's bake rect onto [0,1]^2. gl_Position.z
			// stays 0; nothing tests it.
			const float pageSizeWorld = rvt->bakedWorldSizeAt(req.mip);
			const glm::vec2 o = rvt->bakedWorldOrigin(req.x, req.y, req.mip);
			glm::mat4 vp(0.0f);
			vp[0][0]   =  2.0f / pageSizeWorld;
			vp[3][0]   = -2.0f * o.x / pageSizeWorld - 1.0f;
			vp[2][1]   =  2.0f / pageSizeWorld;
			vp[3][1]   = -2.0f * o.y / pageSizeWorld - 1.0f;
			vp[3][3]   =  1.0f;

			fbo->bind();
			fbo->attachColorTextureLayer(0, (uint32_t)req.slot, rvt->physical(0));
			fbo->attachColorTextureLayer(1, (uint32_t)req.slot, rvt->physical(1));
			fbo->attachColorTextureLayer(2, (uint32_t)req.slot, rvt->physical(2));
			fbo->attachColorTextureLayer(3, (uint32_t)req.slot, rvt->physical(3));
			RenderCommand::setViewport(0, 0, (uint32_t)physSize, (uint32_t)physSize);
			// Clear first: a page hanging past the terrain bounds gets no geometry.
			RenderCommand::clear();

			const glm::vec2 pageMax = o + glm::vec2(pageSizeWorld);

			m_drawnMeshes.clear();
			for (const auto& cmd : ctx.drawMeshCommands) {
				if (!cmd.isTerrain) continue;

				// Skip tiles that miss this page; a tile with no known footprint is kept.
				if (cmd.terrainSizeXZ.x > 0.0f && cmd.terrainSizeXZ.y > 0.0f) {
					const glm::vec2 tMax = cmd.terrainMinXZ + cmd.terrainSizeXZ;
					if (tMax.x <= o.x || cmd.terrainMinXZ.x >= pageMax.x ||
					    tMax.y <= o.y || cmd.terrainMinXZ.y >= pageMax.y)
						continue;
				}

				// One draw per mesh: the bake wants the whole tile, not the camera's LOD.
				if (std::find(m_drawnMeshes.begin(), m_drawnMeshes.end(), cmd.meshHandle) != m_drawnMeshes.end())
					continue;

				auto material = ResourceManager::get<Material>(cmd.materialHandle);
				if (!material) continue;

				Ref<Shader> shader = material->getShader();
				if (!shader || !shader->isValid()) continue;

				auto mesh = cmd.transientMesh;
				if (!mesh) mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
				if (!mesh) continue;

				m_drawnMeshes.push_back(cmd.meshHandle);

				shader->bind();
				// The clip rect is built from absolute world XZ, so the model needs no
				// render origin. This program is shared with the GBuffer pass, so pin
				// the uniform rather than inherit what that pass left set.
				shader->setMat4("u_ViewProj", vp);
				glm::mat4 model = cmd.transform;
				model[3] = glm::vec4(glm::vec3(cmd.worldPosition), 1.0f);
				shader->setMat4("u_Model", model);
				shader->setFloat3("u_WorldOrigin", glm::vec3(0.0f));
				shader->setFloat("u_Time", ctx.totalTime);

				material->fillBindings(m_bindings);
				applyMaterialBindings(shader, m_bindings);

				mesh->bind();
				mesh->draw();
				mesh->unbind();

				shader->unbind();
			}

			fbo->unbind();
		}

		// Restore the process-global default the later passes rely on.
		RenderCommand::setDepthTesting(true);
		rvt->commitBakes();
	}

}
