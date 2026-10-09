#shader vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec3 a_Tangent;

uniform mat4 u_Model;
uniform mat4 u_ViewProj;

out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_Tangent;
out vec3 v_Bitangent;
out vec4 v_WorldPosition;

void main()
{
    v_WorldPosition = u_Model * vec4(a_Position, 1.0);
    gl_Position = u_ViewProj * v_WorldPosition;

    v_TexCoord = a_TexCoord;
    v_Normal = normalize(mat3(u_Model) * a_Normal);
    v_Tangent = normalize(mat3(u_Model) * a_Tangent);
    v_Bitangent = cross(v_Normal, v_Tangent);
}

#shader fragment
#version 460 core

in vec2 v_TexCoord;
in vec3 v_Normal;
in vec3 v_Tangent;
in vec3 v_Bitangent;
in vec4 v_WorldPosition;

const int MAX_LAYERS = 4;

// Layer 0 is the base: always alpha-composited first so the normalized layers
// can never collapse the whole surface to black.
uniform int   u_LayerCount;
uniform sampler2D u_LayerAlbedo[MAX_LAYERS];   // rgb = albedo, a = height
uniform sampler2D u_LayerNormal[MAX_LAYERS];   // rgb = normal, a = roughness
uniform float u_LayerTiling[MAX_LAYERS];       // world metres per UV tile
uniform int   u_LayerBlend[MAX_LAYERS];        // 0 weight, 1 alpha, 2 height
uniform int   u_LayerUseSlope[MAX_LAYERS];
uniform vec2  u_LayerSlopeRange[MAX_LAYERS];   // slope = 1 - worldNormal.y
uniform int   u_LayerUseHeight[MAX_LAYERS];
uniform vec2  u_LayerHeightRange[MAX_LAYERS];  // worldY / u_HeightScale
uniform float u_LayerNoiseStrength[MAX_LAYERS];
uniform float u_LayerNoiseScale[MAX_LAYERS];

uniform float u_HeightScale;

// Macro variation: a low-frequency albedo tint that hides the texture's own
// repeat rhythm across large distances.
uniform float u_MacroStrength;
uniform float u_MacroScale;

// Per-terrain control map: rgb = painted weight for layers 1..3. Defaults (a
// 1x1 white map with u_TerrainInvSize 0) collapse the sample to 1, so a
// material with no instance overriding it behaves exactly as before.
uniform sampler2D u_Weights;
uniform vec2  u_TerrainOrigin;
uniform float u_TerrainInvSize;

layout(location = 0) out vec4 o_Albedo;
layout(location = 1) out vec4 o_Normal;
layout(location = 2) out vec4 o_Material;
layout(location = 3) out vec4 o_Emissive;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// 1 inside [lo, hi], soft edges, with the endpoints 0 and 1 meaning "unbounded".
float softBand(float x, float lo, float hi, float soft)
{
    float rising  = (lo <= 0.0) ? 1.0 : smoothstep(lo - soft, lo + soft, x);
    float falling = (hi >= 1.0) ? 1.0 : (1.0 - smoothstep(hi - soft, hi + soft, x));
    return rising * falling;
}

void main()
{
    vec3 geomNormal = normalize(v_Normal);
    float slope = 1.0 - clamp(geomNormal.y, 0.0, 1.0);
    float heightFrac = clamp(v_WorldPosition.y / max(u_HeightScale, 1e-4), 0.0, 1.0);

    // ── Pass 1: raw weight per layer ───────────────────────────────────────
    float w[MAX_LAYERS];
    float weightSum = 0.0;

    for (int i = 0; i < u_LayerCount; ++i) {
        if (i == 0) {
            w[i] = 1.0;
            continue;
        }

        float s = 1.0;
        if (u_LayerUseSlope[i] == 1) {
            s *= softBand(slope, u_LayerSlopeRange[i].x, u_LayerSlopeRange[i].y, 0.05);
        }
        if (u_LayerUseHeight[i] == 1) {
            s *= softBand(heightFrac, u_LayerHeightRange[i].x, u_LayerHeightRange[i].y, 0.05);
        }

        vec2 uv = v_WorldPosition.xz / max(u_LayerTiling[i], 1e-4);

        // HeightBlend layers only show where their own height channel says so.
        if (u_LayerBlend[i] == 2) {
            s *= texture(u_LayerAlbedo[i], uv).a;
        }

        // Break the rule's clean boundary so the transition is not a hard line.
        if (u_LayerNoiseStrength[i] > 0.0) {
            float n = valueNoise(v_WorldPosition.xz / max(u_LayerNoiseScale[i], 1e-4));
            s = clamp(s + (n - 0.5) * 2.0 * u_LayerNoiseStrength[i], 0.0, 1.0);
        }

        // Painted control map multiplies the rule's weight. Applied after the
        // noise term so a zero mask stays zero.
        vec2 wuv = (v_WorldPosition.xz - u_TerrainOrigin) * u_TerrainInvSize;
        vec3 mask = texture(u_Weights, wuv).rgb;
        float m[3] = float[3](mask.r, mask.g, mask.b);
        s *= m[i - 1];

        w[i] = s;
        if (u_LayerBlend[i] == 0) {
            weightSum += s;
        }
    }

    // WeightBlend layers share the space the base does not cover: normalize them
    // among themselves so their sum tops out at 1.
    if (weightSum > 1e-5) {
        for (int i = 1; i < u_LayerCount; ++i) {
            if (u_LayerBlend[i] == 0) {
                w[i] /= weightSum;
            }
        }
    }

    // The base keeps whatever coverage the normalized WeightBlend layers leave.
    // Stacking alpha-over would instead compose their *transmittance*, so the base
    // bled through wherever two of them overlapped.
    float baseWeight = 1.0;
    for (int i = 1; i < u_LayerCount; ++i) {
        if (u_LayerBlend[i] == 0) {
            baseWeight -= w[i];
        }
    }
    baseWeight = clamp(baseWeight, 0.0, 1.0);

    // ── Pass 2: composite ──────────────────────────────────────────────────
    float macro = mix(1.0 - u_MacroStrength, 1.0,
                      valueNoise(v_WorldPosition.xz / max(u_MacroScale, 1e-4)));

    // UVs are planar world XZ, so the tangent tracks world +X — not the vertex
    // tangent, which the terrain mesh never fills in. Using the empty attribute
    // gave a NaN TBN, hence a NaN normal in the GBuffer: no IBL, broken shadows.
    vec3 refAxis = (abs(geomNormal.x) < 0.99) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 T = normalize(refAxis - geomNormal * dot(refAxis, geomNormal));
    vec3 B = cross(geomNormal, T);
    mat3 TBN = mat3(T, B, geomNormal);

    // The base and the WeightBlend layers share one pot, accumulated as a weighted
    // sum so their weights add up exactly instead of compounding.
    vec3  albedo    = vec3(0.0);
    vec3  normalSum = vec3(0.0);
    float roughSum  = 0.0;
    float coverage  = 0.0;

    for (int i = 0; i < u_LayerCount; ++i) {
        if (i != 0 && u_LayerBlend[i] != 0) continue;   // Alpha/Height handled below
        float a = (i == 0) ? baseWeight : clamp(w[i], 0.0, 1.0);
        if (a <= 0.0) continue;

        vec2 uv = v_WorldPosition.xz / max(u_LayerTiling[i], 1e-4);
        vec4 albedoHeight = texture(u_LayerAlbedo[i], uv);
        vec4 normalRough  = texture(u_LayerNormal[i], uv);

        albedo    += a * albedoHeight.rgb * macro;
        normalSum += a * normalize(TBN * (normalRough.rgb * 2.0 - 1.0));
        roughSum  += a * normalRough.a;
        coverage  += a;
    }

    vec3  worldN    = geomNormal;
    float roughness = 0.5;
    if (coverage > 1e-5) {
        albedo    /= coverage;
        worldN    = normalize(normalSum);
        roughness = roughSum / coverage;
    }

    // Alpha / Height layers composite on top, in stack order, at their raw weight.
    for (int i = 1; i < u_LayerCount; ++i) {
        if (u_LayerBlend[i] == 0) continue;
        float a = clamp(w[i], 0.0, 1.0);
        if (a <= 0.0) continue;

        vec2 uv = v_WorldPosition.xz / max(u_LayerTiling[i], 1e-4);
        vec4 albedoHeight = texture(u_LayerAlbedo[i], uv);
        vec4 normalRough  = texture(u_LayerNormal[i], uv);

        albedo = mix(albedo, albedoHeight.rgb * macro, a);

        vec3 layerN = normalize(TBN * (normalRough.rgb * 2.0 - 1.0));
        worldN = normalize(mix(worldN, layerN, a));

        roughness = mix(roughness, normalRough.a, a);
    }

    o_Albedo   = vec4(albedo, 1.0);
    o_Normal   = vec4(worldN * 0.5 + 0.5, 1.0);
    o_Material = vec4(0.0, clamp(roughness, 0.04, 1.0), 1.0, 1.0);
    o_Emissive = vec4(0.0, 0.0, 0.0, 1.0);
}
