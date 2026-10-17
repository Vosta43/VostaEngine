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
uniform mat4 u_PrevViewProj;
uniform vec3 u_CameraPos;
uniform vec3 u_PlanetCenter;

uniform sampler2D u_DepthMap;   // GBuffer depth (scene surface distance)
uniform sampler2D u_History;    // previous frame's accumulated result

// Medium + march (from VolumetricLightParams).
uniform float u_Density;
uniform float u_Scattering;
uniform float u_Anisotropy;
uniform float u_MaxDistance;
uniform int   u_StepCount;
uniform float u_Intensity;
uniform float u_HeightFalloff;
uniform float u_BaseHeight;

// Temporal.
uniform float u_BlendAlpha;     // weight of THIS frame in the history blend
uniform vec2  u_TexelSize;
uniform float u_FrameIndex;

layout(location = 0) out vec4 o_FragColor;

// atmosphere.glsl declares u_SunDirection / u_SunIntensity / u_TransmittanceLUT and
// sunTransmittance; shadow_lib.glsl declares the cascade uniforms and reads
// u_SunDirection, so the include order matters. Includes are inlined before the
// stage split, so this must sit after "#shader fragment".
#include "atmosphere.glsl"
#include "shadow_lib.glsl"

const float PI = 3.14159265359;

vec3 reconstructWorldPosition(vec2 texCoord, float depth, mat4 invViewProj)
{
    // Reverse-Z / 0..1 clip space: the stored depth is already the NDC z.
    vec4 ndc = vec4(texCoord * 2.0 - 1.0, depth, 1.0);
    vec4 world = invViewProj * ndc;
    return world.xyz / world.w;
}

// Henyey-Greenstein phase function. cosTheta = dot(viewDir, sunDir).
float henyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * pow(max(denom, 1e-4), 1.5));
}

// Per-pixel dither so the fixed steps of adjacent pixels land at different
// depths — breaks the banding into noise the temporal blend can average out.
float ditherNoise(vec2 fragCoord)
{
    vec3 p = fract(vec3(fragCoord.xy + u_FrameIndex * 5.588238, u_FrameIndex) * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

void main()
{
    // Outward view ray through the far plane, same reconstruction pbrlighting and
    // the cloud pass use so depth/occlusion stay aligned. Reverse-Z: far = 0.
    vec4 ndc      = vec4(v_TexCoord * 2.0 - 1.0, 0.0, 1.0);
    vec4 farPlane = u_InvViewProj * ndc;
    vec3 farWorld = farPlane.xyz / farPlane.w;
    vec3 rayDir   = normalize(farWorld - u_CameraPos);

    float depth = texture(u_DepthMap, v_TexCoord).r;

    // March only up to the first opaque surface (fog in front of it), else the
    // configured reach.
    vec3  surfacePos = reconstructWorldPosition(v_TexCoord, depth, u_InvViewProj);
    float marchDist  = u_MaxDistance;
    if (depth > 0.0)
        marchDist = min(marchDist, length(surfacePos - u_CameraPos));

    vec3 sunDir = normalize(u_SunDirection);
    float phase = henyeyGreenstein(dot(rayDir, sunDir), u_Anisotropy);

    int steps = clamp(u_StepCount, 4, 64);
    float stepLen = marchDist / float(steps);
    float dither  = ditherNoise(gl_FragCoord.xy);

    vec3  scattered = vec3(0.0);
    float viewT = 1.0;   // view-ray transmittance so far (Beer-Lambert)

    for (int i = 0; i < steps; ++i)
    {
        float t = (float(i) + dither) * stepLen;
        vec3 pos = u_CameraPos + rayDir * t;

        // Exponential height falloff of the medium.
        float height = exp(-max(u_HeightFalloff, 0.0) * (pos.y - u_BaseHeight));
        float sigma  = u_Density * height;
        if (sigma <= 0.0)
            continue;

        // Shadowed in-scatter: the sun light reaching this sample is occluded
        // wherever geometry blocks it, which is what carves out the shafts.
        float viewDepth = -(u_ViewMatrix * vec4(pos, 1.0)).z;
        float shadow = shadowFactor(pos, sunDir, viewDepth);
        if (shadow <= 0.0)
            continue;

        vec3 sunColor = u_SunIntensity * sunTransmittance(pos - u_PlanetCenter, sunDir);
        float slabExt = exp(-sigma * stepLen);
        // Analytic in-slab integral of sigma_s * T_view * L_sun, with
        // sigma_s = u_Scattering * sigma.
        scattered += u_Scattering * sunColor * shadow * (1.0 - slabExt) * viewT;
        viewT *= slabExt;
    }

    scattered *= (phase * u_Intensity);

    // Temporal accumulation. Reproject through the scene depth for surfaces, or
    // the max-distance endpoint for sky, so the history parallaxes correctly.
    vec3 reprojPos = (depth > 0.0) ? surfacePos : (u_CameraPos + rayDir * marchDist);
    vec4 prevClip  = u_PrevViewProj * vec4(reprojPos, 1.0);
    vec2 prevUV    = prevClip.xy / prevClip.w * 0.5 + 0.5;

    vec3 result = scattered;
    if (prevClip.w > 0.0 && all(greaterThanEqual(prevUV, vec2(0.0))) && all(lessThanEqual(prevUV, vec2(1.0))))
    {
        vec3 history = texture(u_History, prevUV).rgb;
        result = mix(history, scattered, clamp(u_BlendAlpha, 0.0, 1.0));
    }

    o_FragColor = vec4(result, 1.0);
}
