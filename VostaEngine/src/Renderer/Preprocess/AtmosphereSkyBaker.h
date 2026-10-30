#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/Atmosphere.h"
#include "Scene/SpaceBackground.h"

#include <glm.hpp>

namespace ve {

    // Renders the analytic atmosphere (the scattering LUTs) and, when one is
    // given, the space background's star field and nebula into a cubemap so the
    // result can feed the image-based-lighting bake. GPU side, one-time/on-change
    // cost like IBLBaker.
    //
    // The sun disk is deliberately excluded: the lighting pass adds the direct
    // sun analytically, so including the disk here would double-count it. The
    // same applies to the space background's stars — they are part of the sky
    // irradiance, never a direct light.
    class VE_API AtmosphereSkyBaker {
    public:
        // Sky radiance sampled along all six cube faces at the given
        // planet-relative origin. faceSize is the per-face resolution; the sky is
        // smooth, so a small face is enough. Returns nullptr on failure or, with
        // an atmosphere, when any LUT is missing.
        //
        // `space` adds the stars and nebula to the baked sky; pass nullptr for
        // the atmosphere alone.
        static Ref<TextureCubeMap> bakeSkyCubemap(
            const AtmosphereParams& params,
            const Ref<Texture2D>& transmittance,
            const Ref<Texture3D>& scattering,
            const Ref<Texture3D>& mieScattering,
            const Ref<Texture3D>& multipleScattering,
            const SpaceBackgroundParams* space,
            const glm::vec3& planetRelOrigin,
            uint32_t faceSize = 64);

        // The same bake with no atmosphere at all: a sky made only of the space
        // background. This is what gives a scene with a Space Background but no
        // Atmosphere entity a diffuse ambient source — its stars and nebula,
        // convolved into an irradiance map.
        static Ref<TextureCubeMap> bakeSpaceSkyCubemap(
            const SpaceBackgroundParams& space,
            uint32_t faceSize = 128);
    };

}
