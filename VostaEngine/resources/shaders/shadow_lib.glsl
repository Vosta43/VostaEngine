// Cascaded shadow maps. Vosta has no per-frame UBO, so the HDR pass pushes the
// light view-projections, split depths and per-cascade texel sizes every frame.
// Included by pbrlighting.glsl; u_SunDirection comes from atmosphere.glsl.

uniform mat4 u_ShadowVP0;
uniform mat4 u_ShadowVP1;
uniform mat4 u_ShadowVP2;
uniform vec4 u_ShadowSplitFar;     // x/y = cascade 0/1 far edge; z = outermost far
uniform vec4 u_ShadowTexelWorld;   // x/y/z = cascade 0/1/2 texel size in world units
uniform sampler2DShadow u_ShadowMap0;
uniform sampler2DShadow u_ShadowMap1;
uniform sampler2DShadow u_ShadowMap2;
uniform float SHADOW_SAMPLES;
uniform mat4 u_ViewMatrix;
uniform int  u_ShadowCascadeCount;

mat4 cascadeShadowVP(int i) {
    if (i <= 0) return u_ShadowVP0;
    if (i == 1) return u_ShadowVP1;
    return u_ShadowVP2;
}

// Explicit select: some drivers reject a non-constant component index into a vec4.
float cascadeTexelWorld(int i) {
    if (i <= 0) return u_ShadowTexelWorld.x;
    if (i == 1) return u_ShadowTexelWorld.y;
    return u_ShadowTexelWorld.z;
}

// Soft-shadow spread, in shadow-map UV, and the normal-offset bias in shadow texels.
const float kShadowSoftUV     = 0.0012;
const float kShadowBiasTexels = 2.0;

// Centre, the four axes, then the corners (0.7071 is the diagonal's share of the
// radius). The row selects the first N.
const vec2 kShadowTaps[9] = vec2[9](
    vec2( 0.0,    0.0),
    vec2( 1.0,    0.0),    vec2(-1.0,    0.0),
    vec2( 0.0,    1.0),    vec2( 0.0,   -1.0),
    vec2( 0.7071, 0.7071), vec2(-0.7071, 0.7071),
    vec2(-0.7071,-0.7071), vec2( 0.7071,-0.7071));

// The disk is turned by a per-pixel angle so a straight shadow edge does not
// quantise into the same steps everywhere and read as a stair.
const vec2 kShadowRot[16] = vec2[16](
    vec2( 1.000000,  0.000000), vec2( 0.923880,  0.382683),
    vec2( 0.707107,  0.707107), vec2( 0.382683,  0.923880),
    vec2( 0.000000,  1.000000), vec2(-0.382683,  0.923880),
    vec2(-0.707107,  0.707107), vec2(-0.923880,  0.382683),
    vec2(-1.000000,  0.000000), vec2(-0.923880, -0.382683),
    vec2(-0.707107, -0.707107), vec2(-0.382683, -0.923880),
    vec2( 0.000000, -1.000000), vec2( 0.382683, -0.923880),
    vec2( 0.707107, -0.707107), vec2( 0.923880, -0.382683));

float softShadow(sampler2DShadow sm, vec2 uv, float depth) {
    ivec2 cell = ivec2(mod(gl_FragCoord.xy, 4.0));
    vec2 turn = kShadowRot[cell.y * 4 + cell.x];
    // Column-major: col0 = (cos, sin), col1 = (-sin, cos).
    mat2 rot = mat2(turn.x, turn.y, -turn.y, turn.x);

    int taps = clamp(int(SHADOW_SAMPLES + 0.5), 1, 9);
    float s = 0.0;
    for (int i = 0; i < 9; ++i) {
        if (i >= taps) break;
        s += texture(sm, vec3(uv + rot * (kShadowTaps[i] * kShadowSoftUV), depth));
    }
    return s / float(taps);
}

// Samples one cascade's map. Returns lit when the fragment projects outside it.
float sampleCascadeShadow(int cascade, vec3 worldPos, vec3 normal) {
    // Nudge along the normal by this cascade's own texel size, so the offset matches
    // the depth error the map can resolve. Grazing faces (low NdotL) get more.
    float ndotl = clamp(dot(normal, normalize(u_SunDirection)), 0.0, 1.0);
    worldPos += normal * cascadeTexelWorld(cascade) * kShadowBiasTexels * (2.0 - 0.95 * ndotl);

    vec4 sc = cascadeShadowVP(cascade) * vec4(worldPos, 1.0);
    if (sc.w <= 0.0) return 1.0;

    vec3 ndc = sc.xyz / sc.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    // Margin keeps the soft-shadow taps inside the map.
    float m = kShadowSoftUV + 0.001;
    if (uv.x <= m || uv.x >= 1.0 - m || uv.y <= m || uv.y >= 1.0 - m) return 1.0;

    float depth = ndc.z * 0.5 + 0.5;
    // Outside the light near/far box there is no comparison value; treat as lit.
    if (depth <= 0.0 || depth >= 1.0) return 1.0;

    if (cascade == 0) return softShadow(u_ShadowMap0, uv, depth);
    if (cascade == 1) return softShadow(u_ShadowMap1, uv, depth);
    return softShadow(u_ShadowMap2, uv, depth);
}

// viewDepth = distance from the camera along the view forward axis.
float shadowFactor(vec3 worldPos, vec3 normal, float viewDepth) {
    int lo;
    float split;
    if (viewDepth < u_ShadowSplitFar.x)      { lo = 0; split = u_ShadowSplitFar.x; }
    else if (viewDepth < u_ShadowSplitFar.y) { lo = 1; split = u_ShadowSplitFar.y; }
    else                                     { lo = 2; split = 1.0e9; }

    float s = sampleCascadeShadow(lo, worldPos, normal);

    // Blend with the next cascade across a band near this cascade's far edge so the
    // switch does not show as a seam.
    if (lo < 2) {
        float band = max(split * 0.1, 2.0);
        float f = smoothstep(split - band, split, viewDepth);
        s = mix(s, sampleCascadeShadow(lo + 1, worldPos, normal), f);
    }

    // Fade to fully lit approaching the outermost cascade's far edge.
    float fade = clamp((u_ShadowSplitFar.z - viewDepth) / 16.0, 0.0, 1.0);
    return mix(1.0, s, fade);
}
