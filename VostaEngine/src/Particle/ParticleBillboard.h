#pragma once

#include "Core/Core.h"
#include "EmitterAsset.h"
#include "ParticleDataSet.h"
#include "Renderer/Texture.h"

#include <functional>

#include <glm.hpp>

namespace ve {

	// The one place a particle simulation becomes quads. The particle editor's
	// preview and the scene viewport both draw through it, so an effect cannot
	// look different in one than in the other.
	namespace ParticleBillboard {

		// The soft-dot sprite every particle quad is textured with.
		VE_API Ref<Texture2D> sprite();

		// Maps a particle position in asset space to render space.
		using SpaceMap = std::function<glm::vec3(const glm::vec3&)>;

		// Appends one camera-facing quad per live particle to the frame's instance
		// list. No GPU work happens here; the caller owns the blend/depth state and
		// pairs this with flush().
		VE_API uint32_t draw(const EmitterAsset& emitter, const ParticleDataSet& particles,
			const SpaceMap& toRenderSpace,
			const glm::vec4& tint = glm::vec4(0.55f, 0.75f, 1.0f, 1.0f),
			float size = 30.0f);

		// One instanced draw for everything appended since the last flush. The
		// camera basis is the inverse view's right/up, so pass the same view the
		// SpaceMap mapped into. A no-op when nothing was appended.
		VE_API void flush(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

		// Particle assets are authored in the editor's Z-up space -- the preview
		// draws them as-is -- while the world is Y-up. This is the single
		// conversion between the two: local +Z (up in the preview) becomes world
		// +Y (up in the scene), so a plume rises in both.
		VE_API glm::mat3 spaceToWorld();

	}

}
