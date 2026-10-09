#include "vepch.h"
#include "ShadowPass.h"
#include "Renderer/RenderCommand.h"
#include "Core/ResourceManager.h"
#include "Renderer/StaticMesh.h"

namespace ve {

	void ShadowPass::init() {
	}

	void ShadowPass::execute(RenderContext& ctx) {
		// The cascade matrices are only fit when a sun exists; without them the
		// map is meaningless and downstream lighting ignores it anyway.
		if (ctx.shadowCascadeCount <= 0 || !m_target) {
			return;
		}
		m_cascade = m_def.index;
		if (m_cascade < 0 || m_cascade >= ctx.shadowCascadeCount) {
			return;
		}

		m_target->bind();
		RenderCommand::setViewport(0, 0, m_target->getWidth(), m_target->getHeight());
		// Depth mask must be on before the clear: glClear respects it.
		RenderCommand::setDepthTesting(true);
		RenderCommand::setDepthMask(true);
		RenderCommand::clear();

		if (!m_shader) {
			m_target->unbind();
			return;
		}

		m_shader->bind();
		m_shader->setMat4("u_ShadowVP", ctx.shadowLightVP[m_cascade]);

		for (const auto& cmd : ctx.drawMeshCommands) {
			auto mesh = ResourceManager::get<StaticMesh>(cmd.meshHandle);
			if (!mesh) continue;

			m_shader->setMat4("u_Model", cmd.transform);
			mesh->bind();
			mesh->draw(cmd.startIndex, cmd.indexCount);
			mesh->unbind();
		}

		m_shader->unbind();
		m_target->unbind();

		setWritten(m_target);
	}

}
