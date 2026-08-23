#include "vepch.h"
#include "CloudPass.h"
#include "Renderer/RenderCommand.h"
#include "Core/Application.h"
#include "Renderer/Buffer.h"

namespace ve {

	void CloudPass::init() {
		auto& shaderLib = Application::get().getShaderLibrary();
		shaderLib.load("SandBox/assets/shaders/volume_cloud_pass.glsl");
		m_cloudShader = shaderLib.get("volume_cloud_pass");

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

	void CloudPass::execute(RenderContext& ctx) {
		if (!m_cloudBuffer) {
			return;
		}

		m_cloudBuffer->bind();
		// bind() already forces the half-res viewport; set it explicitly anyway.
		// Do NOT use ctx.viewPortWidth/Height — that is full-res and would crop
		// the quad to the top-left quadrant of the half-res buffer.
		RenderCommand::setViewport(0, 0, m_cloudBuffer->getWidth(), m_cloudBuffer->getHeight());
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // alpha 1 = fully transparent
		RenderCommand::clear();

		// The shader samples the baked Worley textures, so they must all be
		// present: an unbound sampler3D reads (0,0,0,1) -> worleyFbm=0 ->
		// shape=1 -> solid white shell. No bake, no clouds (buffer stays cleared
		// to transparent).
		bool enabled = ctx.hasAtmosphere && m_cloudShader
			&& ctx.cloudNoiseTexture && ctx.cloudDetailTexture && ctx.cloudWarpTexture && ctx.cloudWeatherMap;
		if (enabled) {
			const AtmosphereParams& a = ctx.atmosphere;
			const CloudParams& c = ctx.clouds;
			float inner = a.planetRadius + c.bottomAltitude;
			float outer = a.planetRadius + c.topAltitude;
			enabled = outer > inner;   // invalid/disabled layer -> leave cleared to transparent
			if (enabled) {
				m_cloudShader->bind();
				m_cloudShader->setMat4("u_InvViewProj", glm::inverse(ctx.projMatrix * ctx.viewMatrix));
				m_cloudShader->setFloat3("u_CameraPos", ctx.cameraPosition);
				m_cloudShader->setFloat3("u_PlanetCenter", ctx.planetCenter);
				m_cloudShader->setFloat3("u_SunDirection", glm::normalize(a.sunDirection));
				m_cloudShader->setFloat("u_SunIntensity", a.sunIntensity);
				m_cloudShader->setFloat("u_PlanetRadius", a.planetRadius);
				m_cloudShader->setFloat("u_AtmosphereHeight", a.atmosphereHeight);
				// Physical sun color for the cloud in-scatter (raymarchClouds in
				// volume_cloud.glsl): sampled from the transmittance LUT, which this
				// pass binds itself — HDRBufferPass binds it only for its own shader.
				m_cloudShader->setTexture("u_TransmittanceLUT", ctx.transmittanceTexture, 12);
				m_cloudShader->setFloat("u_CloudInnerRadius", inner);
				m_cloudShader->setFloat("u_CloudOuterRadius", outer);
				m_cloudShader->setFloat("u_CloudSigma", c.sigma);
				m_cloudShader->setFloat("u_CloudLightAbsorption", c.lightAbsorption);
				m_cloudShader->setFloat("u_CloudBaseFrequency", c.baseFrequency);
				m_cloudShader->setFloat4("u_CloudShapeWeights", c.shapeNoiseWeights);
				m_cloudShader->setFloat("u_CloudStep", c.stepSize);
				m_cloudShader->setFloat("u_CloudCoverage", c.coverage);
				m_cloudShader->setFloat("u_CloudShapeMin", c.shapeMin);
				m_cloudShader->setFloat("u_CloudShapeMax", c.shapeMax);
				m_cloudShader->setFloat("u_CloudAmbient", c.ambient);
				m_cloudShader->setFloat("u_CloudFrame", (float)ctx.frameIndex);
				m_cloudShader->setFloat3("u_CloudShadowColor", c.shadowColor);
				m_cloudShader->setFloat3("u_CloudMidColor", c.midColor);
				m_cloudShader->setFloat("u_CloudRampOffset1", c.rampOffset1);
				m_cloudShader->setFloat("u_CloudRampOffset2", c.rampOffset2);
				m_cloudShader->setFloat("u_CloudDetailErodeWeight", c.detailErodeWeight);
				m_cloudShader->setFloat("u_CloudDensityMultiplier", c.densityMultiplier);

				// Wind advection: normalized horizontal direction * speed, as a
				// world-space velocity (m/s). Time comes from ctx.totalTime (the
				// engine clock); sampleShape applies velocity * time as a position
				// offset. Zero length direction (or zero speed) -> static deck.
				float windLen = glm::length(c.windDirection);
				glm::vec3 windVel = windLen > 1e-6f
					? glm::vec3(c.windDirection.x / windLen, 0.0f, c.windDirection.y / windLen) * c.windSpeed
					: glm::vec3(0.0f);
				m_cloudShader->setFloat3("u_CloudWind", windVel);
				m_cloudShader->setFloat("u_CloudTime", ctx.totalTime);

				// Per-layer wind: the detail band scrolls relative to the shape wind
				// (scale 1 = locked to shape advection), and the weather coverage drifts
				// at the wind's angular speed around the cloud layer (2π·radius) scaled
				// by weatherWindScale.
				m_cloudShader->setFloat("u_CloudDetailWindScale", c.detailWindScale);
				float cloudRadius = a.planetRadius + c.bottomAltitude;
				glm::vec2 weatherDrift = glm::vec2(windVel.x, windVel.z)
					/ (2.0f * 3.14159265359f * cloudRadius) * c.weatherWindScale;
				m_cloudShader->setFloat2("u_CloudWeatherDrift", weatherDrift);

				m_cloudShader->setTexture3D("u_Shape3D", ctx.cloudNoiseTexture, 9);
				m_cloudShader->setFloat("u_CloudShapeCells", ctx.cloudWorleyCells);
				m_cloudShader->setTexture3D("u_DetailWorley3D", ctx.cloudDetailTexture, 10);
				m_cloudShader->setFloat("u_CloudDetailCells", ctx.cloudDetailCells);
				m_cloudShader->setFloat("u_CloudDetailFrequency", c.detailFrequency);
				m_cloudShader->setTexture3D("u_Warp3D", ctx.cloudWarpTexture, 11);
				m_cloudShader->setFloat("u_CloudWarpCells", ctx.cloudWarpCells);
				m_cloudShader->setTexture("u_WeatherMap", ctx.cloudWeatherMap, 13);
				m_cloudShader->setFloat("u_CloudHeightGradientWeight", c.heightGradientWeight);

				m_fullscreenQuad->bind();
				RenderCommand::drawIndexed(m_fullscreenQuad);
				m_fullscreenQuad->unbind();
				m_cloudShader->unbind();
			}
		}

		ctx.inputTextures["clouds"] = m_cloudBuffer->getColorTexture(0);
		m_cloudBuffer->unbind();
	}

}
