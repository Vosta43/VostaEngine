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

// Layer cap, injected by the builder as this material's layer count.
/*__MAX_LAYERS_DECL__*/

// Layer 0 is the base: always composited first so the normalized layers can
// never collapse the whole surface to black.
uniform int   u_LayerCount;
uniform float u_LayerTiling[MAX_LAYERS];       // world metres per UV tile
uniform int   u_LayerBlend[MAX_LAYERS];        // 0 weight, 1 alpha, 2 height

uniform float u_MacroStrength;
uniform float u_MacroScale;

// Per-terrain layer weights: one slice per layer, RGB-style. Slice i is layer i's
// weight; slice 0 is unused (the base is the residual). The CPU bake (the rule
// graph, or the layers' own slope/height/noise rules) and the paint brush both
// write here, last writer wins. A 1x1 zero array with u_TerrainInvSize 0 collapses
// the sample to 0, so an unpainted surface shows only the base layer.
uniform sampler2DArray u_Weights;
uniform vec2  u_TerrainOrigin;
uniform float u_TerrainInvSize;
// v_WorldPosition is render space (world minus this), but the terrain origin, the
// macro noise and the grid UV are all world-anchored: add it back where used. The
// RVT bake draws this shader in absolute world and leaves u_WorldOrigin at 0.
uniform vec3  u_WorldOrigin;

// Coordinate mode: 0 = chart world XZ (terrain), 1 = sphere direction (globe tiles,
// whose geomNormal is the planet-local unit direction). Globe mode re-parameterises
// every surface coordinate: the weight lookup stays equirectangular (its atlas is
// baked that way), while the layer content and the macro tint use a cube-face UV
// whose poles are face centres, so no texture collapses to a point at a pole.
uniform int   u_CoordMode;
uniform float u_GlobeTiles;       // layer content tiles across one cube face
uniform float u_GlobeMacroTiles;  // macro tiles across one cube face

// Engine clock, read by Time / Panner nodes inside a layer's material graph.
uniform float u_Time;

layout(location = 0) out vec4 o_Albedo;
layout(location = 1) out vec4 o_Normal;
layout(location = 2) out vec4 o_Material;
layout(location = 3) out vec4 o_Emissive;

// One sampler per layer's material graph (and, for a fixed-slot material, one
// per assigned map), named and unit-bound by the builder.
/*__SAMPLER_DECLS__*/

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

const float VE_PI = 3.14159265359;

// Planet-local unit direction -> equirectangular UV in [0, 1], matching the globe
// weight bake's parameterisation. Singular at the poles (all longitudes converge),
// so only the low-frequency weight atlas uses it; content textures use veCubeUV.
vec2 veEquirect(vec3 d)
{
    return vec2(atan(d.z, d.x) / (2.0 * VE_PI) + 0.5,
                asin(clamp(d.y, -1.0, 1.0)) / VE_PI + 0.5);
}

// Unit direction -> cube-face UV in [0, 1]: pick the dominant axis as the face and
// project the other two onto it. The poles sit at the centre of the +/-Y faces, so
// a texture here does not collapse at either pole; the cube's 12 edges are thin
// seams. No singular point.
vec2 veCubeUV(vec3 d)
{
    vec3 a = abs(d);
    vec2 uv = (a.x >= a.y && a.x >= a.z) ? vec2(d.z, d.y) / a.x
            : (a.y >= a.z)               ? vec2(d.x, d.z) / a.y
            :                              vec2(d.x, d.y) / a.z;
    return uv * 0.5 + 0.5;
}

// One function per layer, spliced by the builder. Each evaluates that layer's
// material (its graph, or its fixed slots) and returns the surface as albedo,
// world normal, roughness, metallic, AO and emissive. The parameter names
// deliberately match what a material graph's codegen emits (v_TexCoord /
// v_WorldPos / v_TBN), so a graph body splices in unchanged.
/*__EVAL_FUNCS__*/

void main()
{
    vec3 geomNormal = normalize(v_Normal);

    // Every layer's height channel, sampled up front: the heights are distinct
    // named samplers, not one array, so they cannot be indexed inside the weight
    // loop below. Layers without a height map hold 1.0 (fully present).
    float hL[MAX_LAYERS];
    /*__HEIGHT_PREPASS__*/

    // ── Pass 1: layer weights, read from the baked/painted weight map ──────
    vec2 worldXZ = v_WorldPosition.xz + u_WorldOrigin.xz;
    vec2 wuv = (u_CoordMode == 1)
        ? veEquirect(geomNormal)
        : (worldXZ - u_TerrainOrigin) * u_TerrainInvSize;

    float w[MAX_LAYERS];
    float weightSum = 0.0;

    for (int i = 0; i < u_LayerCount; ++i) {
        if (i == 0) {
            w[i] = 1.0;
            continue;
        }

        // Slice i holds layer i's final weight. The slice coordinate is filterable,
        // so a dynamic layer index needs no unrolling.
        float s = texture(u_Weights, vec3(wuv, float(i))).r;

        // HeightBlend layers only show where their own height channel says so.
        if (u_LayerBlend[i] == 2) {
            s *= hL[i];
        }

        w[i] = s;
        if (u_LayerBlend[i] == 0) {
            weightSum += s;
        }
    }

    // WeightBlend layers share the space the base does not cover: scale them down
    // only when their sum overshoots 1 (overlapping bands). Below 1 the base takes
    // the remainder, which is what keeps a rule's soft edges from being flattened.
    if (weightSum > 1.0) {
        for (int i = 1; i < u_LayerCount; ++i) {
            if (u_LayerBlend[i] == 0) {
                w[i] /= weightSum;
            }
        }
    }

    // The base keeps whatever coverage the normalized WeightBlend layers leave.
    float baseWeight = 1.0;
    for (int i = 1; i < u_LayerCount; ++i) {
        if (u_LayerBlend[i] == 0) {
            baseWeight -= w[i];
        }
    }
    baseWeight = clamp(baseWeight, 0.0, 1.0);

    // Macro variation: a low-frequency tint that hides each layer texture's own
    // repeat rhythm across large distances.
    float macro = mix(1.0 - u_MacroStrength, 1.0,
                      valueNoise((u_CoordMode == 1)
                          ? veCubeUV(geomNormal) * u_GlobeMacroTiles
                          : worldXZ / max(u_MacroScale, 1e-4)));

    // UVs are planar world XZ, so the tangent tracks world +X — not the vertex
    // tangent, which the terrain mesh never fills in. Using the empty attribute
    // gave a NaN TBN, hence a NaN normal in the GBuffer: no IBL, broken shadows.
    vec3 refAxis = (abs(geomNormal.x) < 0.99) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 T = normalize(refAxis - geomNormal * dot(refAxis, geomNormal));
    vec3 B = cross(geomNormal, T);
    mat3 TBN = mat3(T, B, geomNormal);

    // ── Pass 2: evaluate every layer, then composite ───────────────────────
    vec3  sAlb[MAX_LAYERS];
    vec3  sNrm[MAX_LAYERS];
    float sRgh[MAX_LAYERS];
    float sMet[MAX_LAYERS];
    float sAoo[MAX_LAYERS];
    vec3  sEmi[MAX_LAYERS];
    for (int i = 0; i < MAX_LAYERS; ++i) {
        sAlb[i] = vec3(0.0);
        sNrm[i] = geomNormal;
        sRgh[i] = 0.5;
        sMet[i] = 0.0;
        sAoo[i] = 1.0;
        sEmi[i] = vec3(0.0);
    }

    // Per-layer surface evaluation (one call per layer, spliced by the builder).
    /*__LAYER_BLEND__*/

    // The base and the WeightBlend layers share one pot, accumulated as a weighted
    // sum so their weights add up exactly instead of compounding.
    vec3  albedo    = vec3(0.0);
    vec3  normalSum = vec3(0.0);
    float roughSum  = 0.0;
    float metalSum  = 0.0;
    float aoSum     = 0.0;
    vec3  emisSum   = vec3(0.0);
    float coverage  = 0.0;

    for (int i = 0; i < u_LayerCount; ++i) {
        if (i != 0 && u_LayerBlend[i] != 0) continue;   // Alpha/Height handled below
        float a = (i == 0) ? baseWeight : clamp(w[i], 0.0, 1.0);
        if (a <= 0.0) continue;

        albedo    += a * sAlb[i] * macro;
        normalSum += a * sNrm[i];
        roughSum  += a * sRgh[i];
        metalSum  += a * sMet[i];
        aoSum     += a * sAoo[i];
        emisSum   += a * sEmi[i];
        coverage  += a;
    }

    vec3  worldN    = geomNormal;
    float roughness = 0.5;
    float metallic  = 0.0;
    float ao        = 1.0;
    vec3  emissive  = vec3(0.0);
    if (coverage > 1e-5) {
        albedo    /= coverage;
        worldN    = normalize(normalSum);
        roughness = roughSum / coverage;
        metallic  = metalSum / coverage;
        ao        = aoSum / coverage;
        emissive  = emisSum / coverage;
    }

    // Alpha / Height layers composite on top, in stack order, at their raw weight.
    for (int i = 1; i < u_LayerCount; ++i) {
        if (u_LayerBlend[i] == 0) continue;
        float a = clamp(w[i], 0.0, 1.0);
        if (a <= 0.0) continue;

        albedo    = mix(albedo, sAlb[i] * macro, a);
        worldN    = normalize(mix(worldN, sNrm[i], a));
        roughness = mix(roughness, sRgh[i], a);
        metallic  = mix(metallic, sMet[i], a);
        ao        = mix(ao, sAoo[i], a);
        emissive  = mix(emissive, sEmi[i], a);
    }

    o_Albedo   = vec4(albedo, 1.0);
    o_Normal   = vec4(worldN * 0.5 + 0.5, 1.0);
    o_Material = vec4(clamp(metallic, 0.0, 1.0), clamp(roughness, 0.04, 1.0), clamp(ao, 0.0, 1.0), 1.0);
    o_Emissive = vec4(emissive, 1.0);
}
