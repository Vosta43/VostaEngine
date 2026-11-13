#include "vepch.h"
#include "HDRBufferPass.h"
#include "PassBinding.h"
#include "Renderer/RenderCommand.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/SpaceBackgroundUniforms.h"

namespace ve {

	void HDRBufferPass::execute(RenderContext& ctx) {

		if (!beginPass(ctx)) return;

		if (!m_shader) {
			return;
		}

		m_shader->bind();
		glm::mat4 invVP = glm::inverse(ctx.projMatrix * ctx.viewMatrix);
		m_shader->setMat4("u_InvViewProj", invVP);
		// The GBuffer wrote render-space positions, so camera and planet centre go
		// in render-space too. Their difference (the planet-relative camera vector
		// the atmosphere uses) is unchanged by the shared origin.
		m_shader->setFloat3("u_CameraPos", toRenderSpaceChart(ctx.cameraPosition, ctx));

		if (ctx.hasAtmosphere) {
			const AtmosphereParams& a = ctx.atmosphere;
			m_shader->setFloat3("u_PlanetCenter", toRenderSpace(ctx.planetCenter, ctx.renderOrigin));
			m_shader->setFloat3("u_SunDirection", glm::normalize(a.sunDirection));
			m_shader->setFloat3("u_SunIntensity", a.sunIntensity * a.resolveSunColor());
			m_shader->setFloat3("u_RayleighScattering", a.rayleighScattering);
			m_shader->setFloat("u_RayleighScaleHeight", a.rayleighScaleHeight);
			m_shader->setFloat3("u_MieScattering", a.mieScattering);
			m_shader->setFloat("u_MieScaleHeight", a.mieScaleHeight);
			m_shader->setFloat("u_MiePhaseG", a.miePhaseG);
			m_shader->setFloat("u_PlanetRadius", a.planetRadius);
			m_shader->setFloat("u_AtmosphereHeight", a.atmosphereHeight);
			m_shader->setFloat("u_CamAltitude", cameraAltitude(ctx));

			const CloudParams& c = ctx.clouds;
			// Cloud layer bounds stay here — the occlusion test in this shader
			// (rayCloudLayer) uses them. They are altitudes, never radii: the
			// radius difference is what the test needs and it does not survive
			// float32 at planetary scale.
			m_shader->setFloat("u_CloudInnerAltitude", c.bottomAltitude);
			m_shader->setFloat("u_CloudOuterAltitude", c.topAltitude);

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
		else {
			// The shader object is shared with the offscreen preview pipelines
			// (ThumbnailRenderer draws with a full default atmosphere through the
			// same ShaderLibrary entry). Only the branches taken upload uniforms,
			// so a scene with no atmosphere would otherwise inherit the preview's
			// planet radius, altitude and sun for as long as it runs. The space
			// background keys its transmittance and its horizon cull on
			// u_PlanetRadius, so a stale radius tints the stars red and culls the
			// sky; the same leftovers make the atmosphere shaders believe there is
			// a planet at all.
			m_shader->setFloat3("u_PlanetCenter", glm::vec3(0.0f));
			m_shader->setFloat3("u_SunDirection", glm::vec3(0.0f, 1.0f, 0.0f));
			m_shader->setFloat3("u_SunIntensity", glm::vec3(0.0f));
			m_shader->setFloat3("u_RayleighScattering", glm::vec3(0.0f));
			m_shader->setFloat("u_RayleighScaleHeight", 0.0f);
			m_shader->setFloat3("u_MieScattering", glm::vec3(0.0f));
			m_shader->setFloat("u_MieScaleHeight", 0.0f);
			m_shader->setFloat("u_MiePhaseG", 0.0f);
			m_shader->setFloat("u_PlanetRadius", 0.0f);
			m_shader->setFloat("u_AtmosphereHeight", 0.0f);
			m_shader->setFloat("u_CamAltitude", 0.0f);
			m_shader->setFloat("u_CloudInnerAltitude", 0.0f);
			m_shader->setFloat("u_CloudOuterAltitude", 0.0f);
			m_shader->setFloat("u_MultipleScatteringStrength", 0.0f);
		}

		// Exposure closes the HDR pass over the whole frame (sky, geometry and the
		// space background), so it cannot live inside the atmosphere branch: a
		// scene with a space background but no atmosphere has nothing to carry the
		// value, and an unset uniform would multiply the frame by zero.
		m_shader->setFloat("u_Exposure", ctx.hasAtmosphere ? ctx.atmosphere.exposure : 1.0f);

		// Angular size of one pixel for the star footprint AA. The TAA jitter is a
		// clip-space translation, so [1][1] is unchanged by it.
		const float projY = ctx.projMatrix[1][1];
		const float pixelAngle =
			(projY > 1e-6f && ctx.viewPortHeight > 0)
				? 2.0f / (projY * (float)ctx.viewPortHeight)
				: 0.0f;

		// Shared with AtmosphereSkyBaker: the sky that lights the scene and the
		// sky that is drawn must be the same field.
		uploadSpaceBackground(*m_shader,
			ctx.hasSpaceBackground ? &ctx.spaceBackground : nullptr, pixelAngle);

		// Offscreen previews (thumbnails) discard their background in the present
		// pass, so the sky is not evaluated at all there.
		m_shader->setInt("u_DiscardSky", ctx.presentDiscardBackground ? 1 : 0);

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

		// One line whenever the dynamic light set changes. Cheap, and it is the
		// only place that can tell "no lights were submitted" apart from "lights
		// were submitted but did not reach the surface".
		if ((int)ctx.drawLightCommands.size() != m_lastLightCount) {
			m_lastLightCount = (int)ctx.drawLightCommands.size();
			VE_CORE_INFO_PRINT("HDR: %d dynamic light(s) in this frame", m_lastLightCount);
		}

		if (!ctx.drawLightCommands.empty()) {
			std::vector<GpuLightData> gpuLights;
			gpuLights.reserve(ctx.drawLightCommands.size());

			for (auto& cmd : ctx.drawLightCommands) {
				GpuLightData g;
				// Shaded against render-space fragment positions.
				g.position = toRenderSpaceChart(cmd.position, ctx);
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

		drawFullscreenQuad();
		m_shader->unbind();

		m_shader->unbindLightSSBO();

		m_target->unbind();
		setWritten(m_target);
	}

}
