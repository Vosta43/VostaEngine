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

// ---------------------------------------------------------------------------
// GBuffer inputs
// ---------------------------------------------------------------------------
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MaterialMap;
uniform sampler2D u_DepthMap;
uniform samplerCube u_SkyboxMap;
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilteredEnvMap;
uniform sampler2D   u_BRDFLUT;
uniform sampler2D   u_CloudTex;   // half-res cloud coverage pass output

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
uniform mat4 u_InvViewProj;
uniform vec3 u_CameraPos;
uniform vec3 u_PlanetCenter;

// ---------------------------------------------------------------------------
// Light SSBO
// ---------------------------------------------------------------------------
struct Light
{
    vec3  position;
    float range;
    vec3  color;
    float intensity;
    float attenuationLinear;
    float attenuationQuadratic;
    int   type;
    int   pad;
};

layout(std430, binding = 1) readonly buffer LightBuffer
{
    Light lights[];
};

uniform int u_LightCount;

// ---------------------------------------------------------------------------
// HDR output
// ---------------------------------------------------------------------------
layout(location = 0) out vec4 o_FragColor;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
const float PI = 3.14159265359;

// Physically-based atmospheric single scattering (shared library)
#include "atmosphere.glsl"
#include "volume_cloud.glsl"

// ---------------------------------------------------------------------------
// PBR helper functions (Cook-Torrance BRDF, metallic-roughness workflow)
// ---------------------------------------------------------------------------

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX / Trowbridge-Reitz normal distribution function
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// Schlick-Beckmann geometry shadowing
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith geometry function
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometrySchlickGGX(NdotV, roughness);
    float ggx1  = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// ---------------------------------------------------------------------------
// Attenuation
// ---------------------------------------------------------------------------
float calculateAttenuation(float distance, float range, float linear, float quadratic)
{
    if (distance > range) return 0.0;
    float att = 1.0 / (1.0 + linear * distance + quadratic * distance * distance);
    return clamp(att, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Reconstruct world position from depth
// ---------------------------------------------------------------------------
vec3 reconstructWorldPosition(vec2 texCoord, float depth, mat4 invViewProj)
{
    vec4 ndc;
    ndc.x = texCoord.x * 2.0 - 1.0;
    ndc.y = texCoord.y * 2.0 - 1.0;
    ndc.z = depth * 2.0 - 1.0;
    ndc.w = 1.0;

    vec4 world = invViewProj * ndc;
    return world.xyz / world.w;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main()
{
    float depth = texture(u_DepthMap, v_TexCoord).r;

    // Outward view ray (far-plane unproject) — same reconstruction the half-res
    // CloudPass uses, so the cloud texture and this occlusion test stay aligned.
    vec4 ndc      = vec4(v_TexCoord * 2.0 - 1.0, 1.0, 1.0);
    vec4 farPlane = u_InvViewProj * ndc;
    vec3 rayDir   = normalize(farPlane.xyz / farPlane.w - u_CameraPos);

    vec3 worldPos = vec3(0.0);   // assigned in the geometry branch; used by the occlusion test
    vec3 finalColor;

    // Background pixels (no geometry) — render the atmospheric sky
    if (depth >= 1.0)
    {
        vec3 atmosphere = computeAtmosphereLUT(u_CameraPos - u_PlanetCenter, rayDir);
        float sunDot    = dot(rayDir, u_SunDirection);
        // Sun disc tinted by the atmosphere along the sun ray: white light,
        // warm and dimmer near the horizon.
        vec3 sunColor   = u_SunIntensity * sunTransmittance(u_CameraPos - u_PlanetCenter, u_SunDirection);
        finalColor      = atmosphere + sunColor * sunDisk(sunDot);
    }
    else
    {
        vec3 albedo    = texture(u_AlbedoMap,   v_TexCoord).rgb;
        vec3 normal    = texture(u_NormalMap,   v_TexCoord).rgb * 2.0 - 1.0;
        vec3 material  = texture(u_MaterialMap, v_TexCoord).rgb;
        float metallic  = material.r;
        float roughness = material.g;
        float ao        = material.b;

        normal = normalize(normal);

        worldPos = reconstructWorldPosition(v_TexCoord, depth, u_InvViewProj);
        vec3 viewDir  = normalize(u_CameraPos - worldPos);

        // Reflectance at normal incidence
        vec3 F0 = mix(vec3(0.04), albedo, metallic);

        // Diffuse IBL — pre-convolved irradiance map sampled by surface normal
        // Metals have no diffuse reflection; (1 - metallic) enforces energy conservation
        vec3 diffuseIBL = texture(u_IrradianceMap, normal).rgb * albedo * (1.0 - metallic) * ao;

        vec3 Lo = vec3(0.0);

        for (int i = 0; i < u_LightCount; ++i)
        {
            Light light = lights[i];
            if (light.type != 0) continue;

            vec3 lightDir   = light.position - worldPos;
            float distance  = length(lightDir);
            if (distance > light.range) continue;

            lightDir = normalize(lightDir);
            vec3 halfDir = normalize(viewDir + lightDir);

            float attenuation = calculateAttenuation(distance, light.range,
                                                     light.attenuationLinear,
                                                     light.attenuationQuadratic);
            vec3 radiance = light.color * light.intensity * attenuation;

            // Cook-Torrance BRDF
            float NdotV = max(dot(normal, viewDir), 0.0);
            float NdotL = max(dot(normal, lightDir), 0.0);

            vec3  F = fresnelSchlick(max(dot(halfDir, viewDir), 0.0), F0);
            float D = distributionGGX(normal, halfDir, roughness);
            float G = geometrySmith(normal, viewDir, lightDir, roughness);

            float denominator = 4.0 * NdotV * NdotL + 0.0001;
            vec3 specular     = D * G * F / denominator;

            // Energy conservation: diffuse + specular <= 1
            vec3 kD = (1.0 - F) * (1.0 - metallic);

            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }

        // Specular IBL — split-sum approximation
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 R = reflect(-viewDir, normal);
        vec3 prefilteredColor = textureLod(u_PrefilteredEnvMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 envBRDF = texture(u_BRDFLUT, vec2(max(dot(normal, viewDir), 0.0), roughness)).rg;
        vec3 specularIBL = prefilteredColor * (F0 * envBRDF.x + envBRDF.y) * ao;

        vec3 color = diffuseIBL + specularIBL + Lo;

        // Clamp HDR highlights, tone mapping will happen in post-process
        finalColor = clamp(color, 0.0, 100.0);
    }

    // Composite the half-res cloud texture over sky AND geometry.
    // Occlusion: if a surface is closer than the cloud layer entry along the
    // outward ray, the cloud is behind the geometry -> fully transparent.
    vec4 cloud = texture(u_CloudTex, v_TexCoord);
    if (depth < 1.0 && cloud.a < 1.0)
    {
        vec2 tLayer = rayCloudLayer(u_CameraPos - u_PlanetCenter, rayDir);
        float sceneDist = length(worldPos - u_CameraPos);
        if (sceneDist < tLayer.x)
            cloud = vec4(0.0, 0.0, 0.0, 1.0);
    }
    finalColor = cloud.rgb + cloud.a * finalColor;

    // Exposure AFTER the cloud blend — scoped to the sky branch, matching the
    // pre-pass behavior (geometry was never exposed; clouds inherit sky exposure).
    if (depth >= 1.0)
        finalColor *= u_Exposure;

    o_FragColor = vec4(finalColor, 1.0);
}