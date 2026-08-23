#shader vertex
#version 460 core

layout(location = 0) in vec2 a_Position;

out vec2 v_TexCoord;

void main()
{
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord  = a_Position * 0.5 + 0.5;
}

#shader fragment
#version 460 core

in vec2 v_TexCoord;

uniform mat4 u_InvViewProj;
uniform vec3 u_CameraPos;
uniform vec3 u_PlanetCenter;

layout(location = 0) out vec4 o_FragColor;

// Includes are inlined by the engine BEFORE the stage split, so they must sit
// after #shader fragment to land only here. atmosphere.glsl first — the cloud
// library reuses its raySphere + u_SunDirection/u_SunIntensity/u_PlanetRadius.
#include "atmosphere.glsl"
#include "volume_cloud.glsl"

void main()
{
    // Outward ray through the far plane — same reconstruction pbrlighting uses,
    // so the half-res cloud texture and the full-res occlusion test stay aligned.
    vec4 ndc      = vec4(v_TexCoord * 2.0 - 1.0, 1.0, 1.0);
    vec4 farPlane = u_InvViewProj * ndc;
    vec3 rayDir   = normalize(farPlane.xyz / farPlane.w - u_CameraPos);

    vec3 origin = u_CameraPos - u_PlanetCenter;
    // Raw (unexposed) in-scattered radiance + remaining transmittance.
    // Exposure is applied later by the HDR pass so clouds aren't double-exposed.
    o_FragColor = raymarchClouds(origin, rayDir, u_SunDirection);
}
