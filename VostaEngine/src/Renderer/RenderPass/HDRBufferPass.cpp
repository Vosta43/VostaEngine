#include "vepch.h"
#include "HDRBufferPass.h"
#include "Renderer/RenderCommand.h"
#include "Core/Application.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/Buffer.h"

namespace ve {


	void HDRBufferPass::init(){

		auto& shaderLib = Application::get().getShaderLibrary();
		shaderLib.load("SandBox/assets/shaders/pbrlighting.glsl");
		m_HDRBufferShader = shaderLib.get("pbrlighting");

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

		if (!m_HDRBuffer) {
			return;
		}

		m_HDRBuffer->bind();
		RenderCommand::setViewport(ctx.viewPortX, ctx.viewPortY, ctx.viewPortWidth, ctx.viewPortHeight);
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		RenderCommand::clear();

		if (!m_HDRBufferShader) {
			return;
		}

		m_HDRBufferShader->bind();
		glm::mat4 invVP = glm::inverse(ctx.projMatrix * ctx.viewMatrix);
		m_HDRBufferShader->setMat4("u_InvViewProj", invVP);
		m_HDRBufferShader->setFloat3("u_CameraPos", ctx.cameraPosition);

		if (ctx.hasAtmosphere) {
			const AtmosphereParams& a = ctx.atmosphere;
			m_HDRBufferShader->setFloat3("u_PlanetCenter", ctx.planetCenter);
			m_HDRBufferShader->setFloat3("u_SunDirection", glm::normalize(a.sunDirection));
			m_HDRBufferShader->setFloat("u_SunIntensity", a.sunIntensity);
			m_HDRBufferShader->setFloat3("u_RayleighScattering", a.rayleighScattering);
			m_HDRBufferShader->setFloat("u_RayleighScaleHeight", a.rayleighScaleHeight);
			m_HDRBufferShader->setFloat3("u_MieScattering", a.mieScattering);
			m_HDRBufferShader->setFloat("u_MieScaleHeight", a.mieScaleHeight);
			m_HDRBufferShader->setFloat("u_MiePhaseG", a.miePhaseG);
			m_HDRBufferShader->setFloat("u_PlanetRadius", a.planetRadius);
			m_HDRBufferShader->setFloat("u_AtmosphereHeight", a.atmosphereHeight);
			m_HDRBufferShader->setFloat("u_Exposure", a.exposure);

			const CloudParams& c = ctx.clouds;
			// Cloud layer bounds stay here — the occlusion test in this shader
			// (rayCloudLayer) needs them. All other cloud params move to CloudPass.
			m_HDRBufferShader->setFloat("u_CloudInnerRadius", a.planetRadius + c.bottomAltitude);
			m_HDRBufferShader->setFloat("u_CloudOuterRadius", a.planetRadius + c.topAltitude);

			if (ctx.transmittanceTexture) {
				m_HDRBufferShader->setTexture("u_TransmittanceLUT", ctx.transmittanceTexture, 9);
			}
			if (ctx.scatteringTexture) {
				m_HDRBufferShader->setTexture3D("u_ScatteringLUT", ctx.scatteringTexture, 10);
			}
			if (ctx.mieScatteringTexture) {
				m_HDRBufferShader->setTexture3D("u_MieScatteringLUT", ctx.mieScatteringTexture, 12);
			}
			if (ctx.multipleScatteringTexture) {
				m_HDRBufferShader->setTexture3D("u_MultipleScatteringLUT", ctx.multipleScatteringTexture, 11);
			}
			m_HDRBufferShader->setFloat("u_MultipleScatteringStrength", a.multipleScattering);
		}

		auto albedoIt = ctx.inputTextures.find("albedo");
		auto normalIt = ctx.inputTextures.find("normal");
		auto materialIt = ctx.inputTextures.find("material");
		auto depthIt = ctx.inputTextures.find("depth");

		if (albedoIt != ctx.inputTextures.end())
			m_HDRBufferShader->setTexture("u_AlbedoMap", albedoIt->second, 0);
		if (normalIt != ctx.inputTextures.end())
			m_HDRBufferShader->setTexture("u_NormalMap", normalIt->second, 1);
		if (materialIt != ctx.inputTextures.end())
			m_HDRBufferShader->setTexture("u_MaterialMap", materialIt->second, 2);
		if (depthIt != ctx.inputTextures.end())
			m_HDRBufferShader->setTexture("u_DepthMap", depthIt->second, 3);

		auto cloudIt = ctx.inputTextures.find("clouds");
		if (cloudIt != ctx.inputTextures.end())
			m_HDRBufferShader->setTexture("u_CloudTex", cloudIt->second, 4);

		if (ctx.skyboxTexture) {
			m_HDRBufferShader->setTextureCube("u_SkyboxMap", ctx.skyboxTexture, 5);
		}
		if (ctx.irradianceMap) {
			m_HDRBufferShader->setTextureCube("u_IrradianceMap", ctx.irradianceMap, 6);
		}
		if (ctx.prefilteredEnvMap) {
			m_HDRBufferShader->setTextureCube("u_PrefilteredEnvMap", ctx.prefilteredEnvMap, 7);
		}
		if (ctx.brdfLUT) {
			m_HDRBufferShader->setTexture("u_BRDFLUT", ctx.brdfLUT, 8);
		}

		if (m_HDRBufferShader && !ctx.drawLightCommands.empty()) {
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

			m_HDRBufferShader->setLightSSBO(gpuLights);
			m_HDRBufferShader->bindLightSSBO(1);
			m_HDRBufferShader->setInt("u_LightCount", (int)gpuLights.size());
		}
		else if (m_HDRBufferShader) {
			m_HDRBufferShader->setInt("u_LightCount", 0);
		}

		m_fullscreenQuad->bind();
		RenderCommand::drawIndexed(m_fullscreenQuad);
		m_HDRBufferShader->unbind();

		m_fullscreenQuad->unbind();

		if (m_HDRBufferShader) m_HDRBufferShader->unbindLightSSBO();

		ctx.inputTextures["hdrColor"] = m_HDRBuffer->getColorTexture(0);

		m_HDRBuffer->unbind();


	}

}
