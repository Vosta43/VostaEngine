#pragma once

#include "Camera.h"
#include "FrameBuffer.h"
#include "StaticMesh.h"
#include "Core/AssetHandle.h"
#include "Light.h"
#include "Atmosphere.h"
#include "Clouds.h"
#include "Texture.h"
#include <glm.hpp>
#include <unordered_map>
#include <string>

namespace ve {
	
	struct DrawMeshCommand {
		AssetHandle meshHandle;
		AssetHandle materialHandle;
		glm::mat4 transform;

		uint32_t startIndex = 0;
		uint32_t indexCount = 0;
	};

	struct DrawLightCommand {
		Light light;
		glm::vec3 position;
	};

	struct DrawSpriteCommand {
		glm::mat4 transform;
		glm::vec2 size;
		AssetHandle textureHandle;
	};

	struct RenderContext {

		glm::vec3 cameraPosition;
		glm::mat4 viewMatrix = glm::mat4(1.0f);
		glm::mat4 projMatrix = glm::mat4(1.0f);
		// The previous frame's jittered view-projection (from Camera::endFrame),
		// used by the TAA pass to reproject history. Zero until the first
		// endFrame() runs.
		glm::mat4 prevViewProjMatrix = glm::mat4(1.0f);
		// Where the pipeline presents its final image (the "@default" source).
		// Owned by the caller: the editor viewport / sandbox target, or a
		// thumbnail's output FBO. Null for callers that don't present.
		Ref<Framebuffer> outputFrameBuffer;
		// The framebuffer written by the last pass that actually ran this frame;
		// what "@previous" resolves to.
		Ref<Framebuffer> previousTarget;
		// Per-frame tables the pass binder reads: pass-name -> framebuffer written
		// this frame, and FBO-name -> framebuffer. Owned by RenderPipeline.
		const std::unordered_map<std::string, Ref<Framebuffer>>* passOutputs = nullptr;
		const std::unordered_map<std::string, Ref<Framebuffer>>* fboRegistry = nullptr;
		// Present pass behaviour: when true, clear the output to transparent and
		// discard sky pixels (thumbnail previews on a transparent background).
		bool presentDiscardBackground = false;

		Ref<TextureCubeMap> skyboxTexture;
		Ref<TextureCubeMap> irradianceMap;
		Ref<TextureCubeMap> prefilteredEnvMap;
		Ref<Texture2D> brdfLUT;

		std::vector<DrawMeshCommand> drawMeshCommands;
		std::vector<DrawLightCommand> drawLightCommands;
		std::vector<DrawSpriteCommand> drawSpriteCommands;

		float deltaTime;
		float totalTime;
		// Camera's frame counter (the one behind the halton projection jitter).
		// Drives the cloud blue-noise phase so it advances a fixed amount every
		// frame regardless of frame rate.
		uint32_t frameIndex = 0;

		int viewPortX = 0;
		int viewPortY = 0;
		int viewPortWidth = 0;
		int viewPortHeight = 0;

		float exposure = 1.0f;
		glm::vec3 ambientLight = glm::vec3(0.05f);

		AtmosphereParams atmosphere;
		bool hasAtmosphere = false;
		glm::vec3 planetCenter = glm::vec3(0.0f);
		// Precomputed transmittance LUT for the current atmosphere params;
		// null when no atmosphere is present (sunTransmittance then stays at
		// the shader's default unit-0 sampling, which is never read anyway).
		Ref<Texture2D> transmittanceTexture;

		// Precomputed single-scattering 3D LUTs (see AtmosphereBaker::bakeScatteringLUT),
		// split into Rayleigh and Mie RGB16F textures; sampled by the HDR pass. Mie
		// gets its own texture so its per-channel color survives — the transmittance
		// it multiplies reddens strongly at dusk, which a single gray channel would
		// wash out. Null when no atmosphere is present.
		Ref<Texture3D> scatteringTexture;
		Ref<Texture3D> mieScatteringTexture;

		// Precomputed multiple-scattering 3D LUT (see
		// AtmosphereBaker::bakeMultipleScatteringLUT); added to the single-scattering
		// radiance without a phase function. Null when no atmosphere is present.
		Ref<Texture3D> multipleScatteringTexture;

	CloudParams clouds;
	// Perlin-Worley shape field (RGBA: R = Perlin base billow, G/B/A = Worley F1
	// octaves, see WorleyNoiseBaker::bakeMultiOctave). Sampled once per step and
	// dotted with FBM weights in the shader. Null when not baked.
	Ref<Texture3D> cloudNoiseTexture;
	float cloudWorleyCells = 1.0f;
	// Second, structurally independent Worley field for the detail band
	// (different cells + seed, see WorleyNoiseBaker). Null when not baked.
	Ref<Texture3D> cloudDetailTexture;
	float cloudDetailCells = 1.0f;
	// Pre-summed warp FBM (RG: field A / field B, see
	// WorleyNoiseBaker::bakeWarp) that bends the shape-sample position. Null
	// when not baked.
	Ref<Texture3D> cloudWarpTexture;
	float cloudWarpCells = 1.0f;
	// 2D weather map (see WeatherMapBaker): R = per-position coverage sampled by
	// the height gradient to position the cloud band; G/B unused. Null when not
	// baked.
	Ref<Texture2D> cloudWeatherMap;

	// Debug: rasterize meshes as wireframe lines instead of filled triangles.
	// Only the GBuffer pass reads it; all full-screen passes are unaffected.
	bool wireframe = false;

		float getAspectRatio() const {
			return (float)viewPortWidth / (float)viewPortHeight;
		}
	};


}