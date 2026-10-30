// noise.glsl — reusable procedural noise for shader libraries.
//
// Pure functions only: no uniforms, no samplers. Callers pass frequency and
// seed. Everything is 3D-domain — sky/star/nebula fields must be functions of a
// world direction, never 2D/equirect coordinates (pole collapse + seam).
//
// GLSL has no namespaces and other libraries in the same program
// (volume_cloud.glsl) use unprefixed helpers, so every symbol is ve-prefixed.

// ---------------------------------------------------------------------------
// Hashing (PCG). Integer-only: no sin/fract precision differences across GPUs.
// ---------------------------------------------------------------------------
uint vePcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint veHash3(uvec3 cell, uint seed)
{
    uint h = seed;
    h = vePcgHash(h ^ cell.x);
    h = vePcgHash(h ^ cell.y);
    h = vePcgHash(h ^ cell.z);
    return h;
}

// [0, 1), 24 bits — enough mantissa for shading, cheaper than 32.
float veHashUnit(uint h)
{
    return float(h & 0x00FFFFFFu) * (1.0 / 16777216.0);
}

// Four independent uniforms from one cell hash (e.g. brightness, temperature,
// existence, layer variation). The constants decorrelate the four streams.
vec4 veHashUnit4(uint h)
{
    return vec4(
        veHashUnit(vePcgHash(h ^ 0x2C1B3C6Du)),
        veHashUnit(vePcgHash(h ^ 0x297A2D39u)),
        veHashUnit(vePcgHash(h ^ 0x0E7F4A7Cu)),
        veHashUnit(vePcgHash(h ^ 0x5D1A5F1Bu)));
}

// ---------------------------------------------------------------------------
// Value noise, 3D. Output [0, 1]. Not periodic.
// ---------------------------------------------------------------------------
float veFade(float t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float veValueNoise3(vec3 p, uint seed)
{
    vec3 i = floor(p);
    vec3 f = p - i;
    vec3 u = vec3(veFade(f.x), veFade(f.y), veFade(f.z));
    uvec3 c = uvec3(ivec3(i));

    float n000 = veHashUnit(veHash3(c + uvec3(0u, 0u, 0u), seed));
    float n100 = veHashUnit(veHash3(c + uvec3(1u, 0u, 0u), seed));
    float n010 = veHashUnit(veHash3(c + uvec3(0u, 1u, 0u), seed));
    float n110 = veHashUnit(veHash3(c + uvec3(1u, 1u, 0u), seed));
    float n001 = veHashUnit(veHash3(c + uvec3(0u, 0u, 1u), seed));
    float n101 = veHashUnit(veHash3(c + uvec3(1u, 0u, 1u), seed));
    float n011 = veHashUnit(veHash3(c + uvec3(0u, 1u, 1u), seed));
    float n111 = veHashUnit(veHash3(c + uvec3(1u, 1u, 1u), seed));

    return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
               mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

// ---------------------------------------------------------------------------
// Gradient (Perlin) noise, 3D. Output roughly [-1, 1], zero at integer lattice.
// More isotropic than value noise, at the cost of 8 gradient lookups.
// ---------------------------------------------------------------------------
const vec3 kVePerlinGrad[16] = vec3[16](
    vec3( 1.0,  1.0,  0.0), vec3(-1.0,  1.0,  0.0),
    vec3( 1.0, -1.0,  0.0), vec3(-1.0, -1.0,  0.0),
    vec3( 1.0,  0.0,  1.0), vec3(-1.0,  0.0,  1.0),
    vec3( 1.0,  0.0, -1.0), vec3(-1.0,  0.0, -1.0),
    vec3( 0.0,  1.0,  1.0), vec3( 0.0, -1.0,  1.0),
    vec3( 0.0,  1.0, -1.0), vec3( 0.0, -1.0, -1.0),
    vec3( 1.0,  1.0,  0.0), vec3(-1.0,  1.0,  0.0),
    vec3( 0.0, -1.0,  1.0), vec3( 0.0, -1.0, -1.0));

vec3 vePerlinGradient(uvec3 cell, uint seed)
{
    return kVePerlinGrad[veHash3(cell, seed) & 15u];
}

float vePerlin3(vec3 p, uint seed)
{
    vec3 i = floor(p);
    vec3 f = p - i;
    vec3 u = vec3(veFade(f.x), veFade(f.y), veFade(f.z));
    uvec3 c = uvec3(ivec3(i));

    float n000 = dot(vePerlinGradient(c + uvec3(0u, 0u, 0u), seed), f - vec3(0.0, 0.0, 0.0));
    float n100 = dot(vePerlinGradient(c + uvec3(1u, 0u, 0u), seed), f - vec3(1.0, 0.0, 0.0));
    float n010 = dot(vePerlinGradient(c + uvec3(0u, 1u, 0u), seed), f - vec3(0.0, 1.0, 0.0));
    float n110 = dot(vePerlinGradient(c + uvec3(1u, 1u, 0u), seed), f - vec3(1.0, 1.0, 0.0));
    float n001 = dot(vePerlinGradient(c + uvec3(0u, 0u, 1u), seed), f - vec3(0.0, 0.0, 1.0));
    float n101 = dot(vePerlinGradient(c + uvec3(1u, 0u, 1u), seed), f - vec3(1.0, 0.0, 1.0));
    float n011 = dot(vePerlinGradient(c + uvec3(0u, 1u, 1u), seed), f - vec3(0.0, 1.0, 1.0));
    float n111 = dot(vePerlinGradient(c + uvec3(1u, 1u, 1u), seed), f - vec3(1.0, 1.0, 1.0));

    return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
               mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

// ---------------------------------------------------------------------------
// Fractal sum. Normalized by the octave weight sum, so the result keeps the
// base noise's range (Perlin: ~[-1, 1]) regardless of octave count.
// Each octave gets its own seed, so changing the octave count does not shift
// the lower octaves' pattern.
// ---------------------------------------------------------------------------
float veFbm3(vec3 p, int octaves, float lacunarity, float gain, uint seed)
{
    float sum  = 0.0;
    float norm = 0.0;
    float amp  = 1.0;

    for (int i = 0; i < octaves; ++i)
    {
        sum  += vePerlin3(p, seed + uint(i) * 0x9E3779B9u) * amp;
        norm += amp;
        p    *= lacunarity;
        amp  *= gain;
    }

    return norm > 0.0 ? sum / norm : 0.0;
}
