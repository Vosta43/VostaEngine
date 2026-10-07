#include "vepch.h"
#include "CloudPass.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Buffer.h"

namespace ve {

	void CloudPass::init() {
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
		if (!m_target) {
			return;
		}

		m_target->bind();
		// bind() already forces the half-res viewport; set it explicitly anyway.
		// Do NOT use ctx.viewPortWidth/Height — that is full-res and would crop
		// the quad to the top-left quadrant of the half-res buffer.
		RenderCommand::setViewport(0, 0, m_target->getWidth(), m_target->getHeight());
		RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // alpha 1 = fully transparent
		RenderCommand::clear();

		// The shader samples the baked Worley textures, so they must all be
		// present: an unbound sampler3D reads (0,0,0,1) -> worleyFbm=0 ->
		// shape=1 -> solid white shell. No bake, no clouds (buffer stays cleared
		// to transparent).
		bool enabled = ctx.hasAtmosphere && m_shader
			&& ctx.cloudNoiseTexture && ctx.cloudDetailTexture && ctx.cloudWarpTexture && ctx.cloudWeatherMap;
		if (enabled) {
			const AtmosphereParams& a = ctx.atmosphere;
			const CloudParams& c = ctx.clouds;
			float inner = a.planetRadius + c.bottomAltitude;
			float outer = a.planetRadius + c.topAltitude;
			enabled = outer > inner;   // invalid/disabled layer -> leave cleared to transparent
			if (enabled) {
				m_shader->bind();
				m_shader->setMat4("u_InvViewProj", glm::inverse(ctx.projMatrix * ctx.viewMatrix));
				m_shader->setFloat3("u_CameraPos", ctx.cameraPosition);
				m_shader->setFloat3("u_PlanetCenter", ctx.planetCenter);
				m_shader->setFloat3("u_SunDirection", glm::normalize(a.sunDirection));
				m_shader->setFloat("u_SunIntensity", a.sunIntensity);
				m_shader->setFloat("u_PlanetRadius", a.planetRadius);
				m_shader->setFloat("u_AtmosphereHeight", a.atmosphereHeight);
				// Physical sun color for the cloud in-scatter (raymarchClouds in
				// volume_cloud.glsl): sampled from the transmittance LUT, which this
				// pass binds itself — HDRBufferPass binds it only for its own shader.
				m_shader->setTexture("u_TransmittanceLUT", ctx.transmittanceTexture, 12);
				m_shader->setFloat("u_CloudInnerRadius", inner);
				m_shader->setFloat("u_CloudOuterRadius", outer);
				m_shader->setFloat("u_CloudSigma", c.sigma);
				m_shader->setFloat("u_CloudLightAbsorption", c.lightAbsorption);
				m_shader->setFloat("u_CloudBaseFrequency", c.baseFrequency);
				m_shader->setFloat4("u_CloudShapeWeights", c.shapeNoiseWeights);
				m_shader->setFloat("u_CloudStep", c.stepSize);
				m_shader->setFloat("u_CloudCoverage", c.coverage);
				m_shader->setFloat("u_CloudShapeMin", c.shapeMin);
				m_shader->setFloat("u_CloudShapeMax", c.shapeMax);
				m_shader->setFloat("u_CloudAmbient", c.ambient);
				m_shader->setFloat("u_CloudFrame", (float)ctx.frameIndex);
				m_shader->setFloat3("u_CloudShadowColor", c.shadowColor);
				m_shader->setFloat3("u_CloudMidColor", c.midColor);
				m_shader->setFloat("u_CloudRampOffset1", c.rampOffset1);
				m_shader->setFloat("u_CloudRampOffset2", c.rampOffset2);
				m_shader->setFloat("u_CloudDetailErodeWeight", c.detailErodeWeight);
				m_shader->setFloat("u_CloudDensityMultiplier", c.densityMultiplier);

				// Wind advection: normalized horizontal direction * speed, as a
				// world-space velocity (m/s). Time comes from ctx.totalTime (the
				// engine clock); sampleShape applies velocity * time as a position
				// offset. Zero length direction (or zero speed) -> static deck.
				float windLen = glm::length(c.windDirection);
				glm::vec3 windVel = windLen > 1e-6f
					? glm::vec3(c.windDirection.x / windLen, 0.0f, c.windDirection.y / windLen) * c.windSpeed
					: glm::vec3(0.0f);
				m_shader->setFloat3("u_CloudWind", windVel);
				m_shader->setFloat("u_CloudTime", ctx.totalTime);

				// Per-layer wind: the detail band scrolls relative to the shape wind
				// (scale 1 = locked to shape advection), and the weather coverage drifts
				// at the wind's angular speed around the cloud layer (2π·radius) scaled
				// by weatherWindScale.
				m_shader->setFloat("u_CloudDetailWindScale", c.detailWindScale);
				float cloudRadius = a.planetRadius + c.bottomAltitude;
				glm::vec2 weatherDrift = glm::vec2(windVel.x, windVel.z)
					/ (2.0f * 3.14159265359f * cloudRadius) * c.weatherWindScale;
				m_shader->setFloat2("u_CloudWeatherDrift", weatherDrift);

				m_shader->setTexture3D("u_Shape3D", ctx.cloudNoiseTexture, 9);
				m_shader->setFloat("u_CloudShapeCells", ctx.cloudWorleyCells);
				m_shader->setTexture3D("u_DetailWorley3D", ctx.cloudDetailTexture, 10);
				m_shader->setFloat("u_CloudDetailCells", ctx.cloudDetailCells);
				m_shader->setFloat("u_CloudDetailFrequency", c.detailFrequency);
				m_shader->setTexture3D("u_Warp3D", ctx.cloudWarpTexture, 11);
				m_shader->setFloat("u_CloudWarpCells", ctx.cloudWarpCells);
				m_shader->setTexture("u_WeatherMap", ctx.cloudWeatherMap, 13);
				m_shader->setFloat("u_CloudHeightGradientWeight", c.heightGradientWeight);

				m_fullscreenQuad->bind();
				RenderCommand::drawIndexed(m_fullscreenQuad);
				m_fullscreenQuad->unbind();
				m_shader->unbind();
			}
		}

		m_target->unbind();
		setWritten(m_target);
	}

}
