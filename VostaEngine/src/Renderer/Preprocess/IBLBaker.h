#pragma once

#include "Renderer/Texture.h"

namespace ve {

    class VE_API IBLBaker {
    public:
        // Convolves the input HDR cubemap with a cosine-weighted hemispherical
        // kernel to produce a 32x32 RGBA16F irradiance cubemap for diffuse IBL.
        // Returns nullptr on failure.
        static Ref<TextureCubeMap> bakeIrradianceMap(
            const Ref<TextureCubeMap>& textureCubeMapInput);

        // Prefilters the input HDR cubemap with GGX importance sampling across 5
        // mip levels (face sizes 128 down to 8, roughness 0.0 to 1.0).
        // Returns a multi-mip RGBA16F cubemap for specular IBL, or nullptr on failure.
        static Ref<TextureCubeMap> bakePrefilteredEnvMap(
            const Ref<TextureCubeMap>& textureCubeMapInput);

        // Integrates the specular BRDF over (NdotV, roughness) using 1024 GGX
        // importance samples. Returns a 512x512 RG16F 2D texture whose R channel
        // holds scale and G channel holds bias for the split-sum Fresnel term.
        // Returns nullptr on failure.
        static Ref<Texture2D> bakeBRDFLUT();
    };

}
