#pragma once

#include "Renderer/Shader.h"
#include "Scene/SpaceBackground.h"

namespace ve {

// Uploads a Space Background component to a shader that includes
// space_background.glsl. Shared by the HDR lighting pass (which draws the field
// directly) and AtmosphereSkyBaker (which bakes it into the sky cubemap that
// feeds the irradiance convolution), so the visible sky and the baked sky can
// never disagree.
//
// pixelAngle is the angular size of one target pixel, in radians. The star PSF
// is never drawn smaller than that, so a coarse bake spreads each star over its
// pixel instead of dropping it: the flux is preserved either way, which is all
// the irradiance convolution looks at.
inline void uploadSpaceBackground(Shader& shader,
                                  const SpaceBackgroundParams* space,
                                  float pixelAngle) {
    // A null component means "no space background": the default-constructed
    // params have enabled = true, so passing a struct by reference here would
    // silently light every scene that never asked for one.
    shader.setInt("u_SpaceEnabled", (space && space->enabled) ? 1 : 0);
    shader.setFloat("u_SpacePixelAngle", pixelAngle);
    if (!space || !space->enabled) {
        return;
    }

    const SpaceStarsParams&  stars  = space->stars;
    const SpaceNebulaParams& nebula = space->nebula;

    shader.setInt("u_SpaceSeed", space->seed);

    shader.setInt("u_StarLayers", stars.layers);
    shader.setFloat("u_StarDensity", stars.density);
    shader.setFloat("u_StarBrightness", stars.brightness);
    shader.setFloat("u_StarAngularSizeRad", stars.angularSizeRad);
    shader.setInt("u_StarUseTemperature", stars.useTemperature ? 1 : 0);
    shader.setFloat3("u_StarColor", stars.starColor);
    shader.setFloat("u_StarTemperatureMinK", stars.temperatureMinK);
    shader.setFloat("u_StarTemperatureMaxK", stars.temperatureMaxK);

    shader.setInt("u_NebulaEnabled", nebula.enabled ? 1 : 0);
    shader.setFloat("u_NebulaBrightness", nebula.brightness);
    shader.setFloat("u_NebulaScale", nebula.scale);
    shader.setFloat("u_NebulaWarp", nebula.warp);
    shader.setFloat3("u_NebulaTint", nebula.tint);
    shader.setFloat3("u_NebulaBandAxis", nebula.bandAxis);
    shader.setFloat("u_NebulaBandWidth", nebula.bandWidth);
}

}  // namespace ve
