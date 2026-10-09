#include "vepch.h"
#include "HDRBufferPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/Buffer.h"

namespace ve {


	void HDRBufferPass::init(){

		float vertices[] = {
			-1.0f, -1.0f,  // bottom left
			1.0f, -1.0f,  // bottom right
			1.0f,  1.0f,  // top right
			-1.0f,  1.0f   // top left
		};
		uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

		m_fullscreenQuad = VertexArray::create();
		auto vb = VertexBuffer::create(vertices, sizeof(vertices));
		BufferLayout layout = {
			{ ShaderDataType::Float2, "a_Position" }
		};
		vb->setLayout(layout);
		m_fullscreenQuad->addVertexBuffer(vb);

		auto ib = IndexBuffer::create(indices, 6);
		m_fullscreenQuad->setIndexBuffer(ib);
	}

	void HDRBufferPass::execute(RenderContext& ctx) {

		if (!m_target) {
			return;
		}

		m_target->bind();
		RenderCommand::setViewport(ctx.viewPortX, ctx.viewPortY, ctx.viewPortWidth, ctx.viewPortHeight);
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		RenderCommand::clear();

		if (!m_shader) {
			return;
		}

		m_shader->bind();
		glm::mat4 invVP = glm::inverse(ctx.projMatrix * ctx.viewMatrix);
		m_shader->setMat4("u_InvViewProj", invVP);
		m_shader->setFloat3("u_CameraPos", ctx.cameraPosition);

		if (ctx.hasAtmosphere) {
			const AtmosphereParams& a = ctx.atmosphere;
			m_shader->setFloat3("u_PlanetCenter", ctx.planetCenter);
			m_shader->setFloat3("u_SunDirection", glm::normalize(a.sunDirection));
			m_shader->setFloat("u_SunIntensity", a.sunIntensity);
			m_shader->setFloat3("u_RayleighScattering", a.rayleighScattering);
			m_shader->setFloat("u_RayleighScaleHeight", a.rayleighScaleHeight);
			m_shader->setFloat3("u_MieScattering", a.mieScattering);
			m_shader->setFloat("u_MieScaleHeight", a.mieScaleHeight);
			m_shader->setFloat("u_MiePhaseG", a.miePhaseG);
			m_shader->setFloat("u_PlanetRadius", a.planetRadius);
			m_shader->setFloat("u_AtmosphereHeight", a.atmosphereHeight);
			m_shader->setFloat("u_Exposure", a.exposure);

			const CloudParams& c = ctx.clouds;
			// Cloud layer bounds stay here — the occlusion test in this shader
			// (rayCloudLayer) and the glossy-sky reflection march both need them.
			m_shader->setFloat("u_CloudInnerRadius", a.planetRadius + c.bottomAltitude);
			m_shader->setFloat("u_CloudOuterRadius", a.planetRadius + c.topAltitude);

			if (ctx.transmittanceTexture) {
				m_shader->setTexture("u_TransmittanceLUT", ctx.transmittanceTexture, 9);
			}
			if (ctx.scatteringTexture) {
				m_shader->setTexture3D("u_ScatteringLUT", ctx.scatteringTexture, 10);
			}
			if (ctx.mieScatteringTexture) {
				m_shader->setTexture3D("u_MieScatteringLUT", ctx.mieScatteringTexture, 12);
			}
			if (ctx.multipleScatteringTexture) {
				m_shader->setTexture3D("u_MultipleScatteringLUT", ctx.multipleScatteringTexture, 11);
			}
			m_shader->setFloat("u_MultipleScatteringStrength", a.multipleScattering);
		}

		// GBuffer attachments, the accumulated cloud buffer and the skybox/IBL
		// cubes come from the pass's declared inputs.
		bindPassInputs(*this, ctx);
		applyPassUniforms(*this);

		// Cascaded shadow data, computed on the CPU this frame (no per-frame UBO).
		m_shader->setMat4("u_ViewMatrix", ctx.viewMatrix);
		m_shader->setInt("u_ShadowCascadeCount", ctx.shadowCascadeCount);
		m_shader->setFloat4("u_ShadowSplitFar", glm::vec4(ctx.shadowSplitFar[0], ctx.shadowSplitFar[1], ctx.shadowSplitFar[2], 0.0f));
		m_shader->setFloat4("u_ShadowTexelWorld", glm::vec4(ctx.shadowTexelWorld[0], ctx.shadowTexelWorld[1], ctx.shadowTexelWorld[2], 0.0f));
		for (int i = 0; i < ctx.shadowCascadeCount; ++i)
			m_shader->setMat4("u_ShadowVP" + std::to_string(i), ctx.shadowLightVP[i]);

		if (!ctx.drawLightCommands.empty()) {
			std::vector<GpuLightData> gpuLights;
			gpuLights.reserve(ctx.drawLightCommands.size());

			for (auto& cmd : ctx.drawLightCommands) {
				GpuLightData g;
				g.position = cmd.position;
				g.range = cmd.light.range;
				g.color = cmd.light.color;
				g.intensity = cmd.light.intensity;
				g.attenuationLinear = cmd.light.attenuationLinear;
				g.attenuationQuadratic = cmd.light.attenuationQuadratic;
				g.type = 0;
				g.pad0 = 0;
				gpuLights.push_back(g);
			}

			m_shader->setLightSSBO(gpuLights);
			m_shader->bindLightSSBO(1);
			m_shader->setInt("u_LightCount", (int)gpuLights.size());
		}
		else {
			m_shader->setInt("u_LightCount", 0);
		}

		m_fullscreenQuad->bind();
		RenderCommand::drawIndexed(m_fullscreenQuad);
		m_shader->unbind();

		m_fullscreenQuad->unbind();

		m_shader->unbindLightSSBO();

		m_target->unbind();
		setWritten(m_target);
	}

}
