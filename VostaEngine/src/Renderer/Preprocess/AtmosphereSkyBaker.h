#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/Atmosphere.h"

#include <glm.hpp>

namespace ve {

    // Renders the analytic atmosphere (the scattering LUTs) into a cubemap so it
    // can feed the image-based-lighting bake. GPU side, one-time/on-change cost
    // like IBLBaker.
    //
    // The sun disk is deliberately excluded: the lighting pass adds the direct
    // sun analytically, so including the disk here would double-count it.
    class VE_API AtmosphereSkyBaker {
    public:
        // Sky radiance sampled along all six cube faces at the given
        // planet-relative origin. faceSize is the per-face resolution; the sky is
        // smooth, so a small face (64) is enough. Returns nullptr on failure or
        // when any LUT is missing.
        static Ref<TextureCubeMap> bakeSkyCubemap(
            const AtmosphereParams& params,
            const Ref<Texture2D>& transmittance,
            const Ref<Texture3D>& scattering,
            const Ref<Texture3D>& mieScattering,
            const Ref<Texture3D>& multipleScattering,
            const glm::vec3& planetRelOrigin,
            uint32_t faceSize = 64);
    };

}
