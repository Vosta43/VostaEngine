// space_background.glsl — direction-only star field and galaxy nebula.
//
// Stars live on a cube-sphere grid: the view direction is projected onto one of
// six cube faces, that face is split into a per-layer grid of cells, and one
// hash per cell picks that cell's star (position, brightness, temperature).
// Sampling is therefore O(layers) with no star catalogue and no texture.
//
// The nebula is a banded, dust-cut emission field sampled as a shell around the
// viewer — sparse gas crests, dark lanes, one narrow palette.
//
// Both are pure functions of a world direction: they never parallax, never move
// with a clock, and do not follow the planet's rotation. Callers add them BEHIND
// the atmosphere — veSpaceRadiance attenuates them through the transmittance LUT
// and lets the planet occlude them.

#include "noise.glsl"
#include "atmosphere.glsl"

uniform int   u_SpaceEnabled        = 0;
uniform int   u_SpaceSeed           = 0;
uniform int   u_StarLayers          = 4;
uniform float u_StarDensity         = 64.0;
uniform float u_StarBrightness      = 1.0;
uniform float u_StarAngularSizeRad  = 2.0e-4;
uniform int   u_StarUseTemperature  = 1;
uniform vec3  u_StarColor           = vec3(1.0);
uniform float u_StarTemperatureMinK = 3000.0;
uniform float u_StarTemperatureMaxK = 25000.0;
uniform float u_SpacePixelAngle     = 0.001;

uniform int   u_NebulaEnabled    = 0;
uniform float u_NebulaBrightness = 0.8;
uniform float u_NebulaScale      = 2.5;
uniform float u_NebulaWarp       = 1.0;
uniform vec3  u_NebulaTint       = vec3(1.0);
uniform vec3  u_NebulaBandAxis   = vec3(0.0, 1.0, 0.0);
uniform float u_NebulaBandWidth  = 0.55;

// ---------------------------------------------------------------------------
// Cube-sphere parameterization.
//
// A direction is stored as (face, c) with c in [-1, 1] being the face-local
// coordinates: c.x along the face's tangent, c.y along its bitangent. Every
// face keeps the same handedness, so a star found on one face is where the
// neighbouring face's grid would put it — the seam is only a grid discontinuity,
// never a mirrored field.
// ---------------------------------------------------------------------------
vec3 veStarFaceDir(int face, vec2 c)
{
    if (face == 0) return normalize(vec3( 1.0,  c.y, -c.x));
    if (face == 1) return normalize(vec3(-1.0,  c.y,  c.x));
    if (face == 2) return normalize(vec3( c.x,  1.0, -c.y));
    if (face == 3) return normalize(vec3( c.x, -1.0,  c.y));
    if (face == 4) return normalize(vec3( c.x,  c.y,  1.0));
    return               normalize(vec3(-c.x,  c.y, -1.0));
}

void veStarFaceFromDir(vec3 dir, out int face, out vec2 c)
{
    vec3 a = abs(dir);
    if (a.x >= a.y && a.x >= a.z) {
        if (dir.x >= 0.0) { face = 0; c = vec2(-dir.z,  dir.y) / a.x; }
        else              { face = 1; c = vec2( dir.z,  dir.y) / a.x; }
    } else if (a.y >= a.z) {
        if (dir.y >= 0.0) { face = 2; c = vec2(dir.x, -dir.z) / a.y; }
        else              { face = 3; c = vec2(dir.x,  dir.z) / a.y; }
    } else {
        if (dir.z >= 0.0) { face = 4; c = vec2( dir.x, dir.y) / a.z; }
        else              { face = 5; c = vec2(-dir.x, dir.y) / a.z; }
    }
}

// ---------------------------------------------------------------------------
// Blackbody chromaticity — the GLSL twin of ve::blackbodyRGB (Atmosphere.h).
//
// CIE xy Planckian-locus fit (Kim et al.) -> linear sRGB, normalized so the
// brightest channel is 1: switching the star color model never changes
// exposure, exactly like the Atmosphere's "Use Temperature" sun model.
// ---------------------------------------------------------------------------
vec3 veStarBlackbodyRGB(float kelvin)
{
    float T = clamp(kelvin, 1000.0, 40000.0);

    float x;
    if (T <= 4000.0)
        x = -0.2661239e9 / (T * T * T) - 0.2343589e6 / (T * T) + 0.8776956e3 / T + 0.179910;
    else
        x = -3.0258469e9 / (T * T * T) + 2.1070379e6 / (T * T) + 0.2226347e3 / T + 0.240390;

    float y;
    if (T <= 2222.0)      y = -1.1063814 * x * x * x - 1.34811020 * x * x + 2.18555832 * x - 0.20219683;
    else if (T <= 4000.0) y = -0.9549476 * x * x * x - 1.37418593 * x * x + 2.09137015 * x - 0.16748867;
    else                  y =  3.0817580 * x * x * x - 5.87338670 * x * x + 3.75112997 * x - 0.37001483;

    float X = x / y;
    float Z = (1.0 - x - y) / y;

    // Y = 1, so the Y column of the sRGB matrix folds into the constants.
    vec3 rgb = vec3(
         3.2406 * X - 1.5372 - 0.4986 * Z,
        -0.9689 * X + 1.8758 + 0.0415 * Z,
         0.0557 * X - 0.2040 + 1.0570 * Z);

    rgb = max(rgb, vec3(0.0));
    float m = max(rgb.r, max(rgb.g, rgb.b));
    return m > 0.0 ? rgb / m : vec3(1.0);
}

// ---------------------------------------------------------------------------
// Star field radiance, unattenuated. dir must be normalized.
// ---------------------------------------------------------------------------
vec3 veStarField(vec3 dir)
{
    if (u_SpaceEnabled == 0 || u_StarLayers <= 0 || u_StarDensity < 1.0)
        return vec3(0.0);

    int  face;
    vec2 c;
    veStarFaceFromDir(dir, face, c);

    float pixel  = max(u_SpacePixelAngle, 1.0e-7);
    float radius = max(u_StarAngularSizeRad, 1.0e-7);

    // A star narrower than ~1.5 px beats against the TAA jitter and sparkles.
    // The PSF is therefore never drawn smaller than that, and the peak is
    // scaled by the inverse area ratio so the star's total flux is unchanged:
    // the light is spread over the pixels it covers instead of being added to
    // them. Reference is the pixel footprint, not the configured radius, so the
    // default sub-pixel size still reads at full `Brightness`; raising Angular
    // Size past a pixel spreads the same light wider and dimmer, as a real disc
    // of fixed luminosity would.
    float reference = pixel * 1.5;
    float footprint = max(radius, reference);
    float energy    = (reference * reference) / (footprint * footprint);

    vec2  st = c * 0.5 + 0.5;   // [0, 1] on the face

    vec3 sum = vec3(0.0);

    for (int i = 0; i < u_StarLayers; ++i) {
        // Layer 0 is the base grid; each further layer is finer. Per-star flux
        // is scaled by the inverse cell-count ratio, so every layer carries the
        // same total flux and adding layers never changes the sky's exposure.
        float scale = pow(1.6, float(i));
        float cells = min(u_StarDensity * scale, 4096.0);
        float gain  = 1.0 / (scale * scale);

        vec2 g    = st * cells;
        vec2 cell = floor(g);

        uint h = veHash3(uvec3(uint(int(cell.x)), uint(int(cell.y)), uint(face + i * 6)),
                         uint(u_SpaceSeed));
        vec4 s = veHashUnit4(h);

        // Star centre: the cell centre plus a jitter that stays inside the cell,
        // so a PSF never straddles a cell boundary and neighbouring cells can
        // never disagree about which star owns a pixel.
        vec2 centre   = (cell + 0.5 + (s.xy - 0.5) * 0.6) / cells;
        vec3 starDir  = veStarFaceDir(face, centre * 2.0 - 1.0);

        vec3  delta = dir - starDir;
        float ang   = length(delta);   // 2*sin(theta/2), i.e. theta for small theta

        // Beyond ~3.5 sigma the Gaussian has underflowed: no other star in this
        // cell can reach a pixel farther out than this.
        if (ang > footprint * 3.5)
            continue;

        // Flux: a steep power law, so most stars are faint and a few dominate.
        float flux       = pow(s.z, 5.0);
        float brightness = u_StarBrightness * gain * flux;
        if (brightness <= 0.0)
            continue;

        vec3 color = (u_StarUseTemperature != 0)
            ? veStarBlackbodyRGB(mix(u_StarTemperatureMinK, u_StarTemperatureMaxK, s.w))
            : u_StarColor;

        // Core: Gaussian PSF, 1/e at the angular radius.
        float shape = exp(-(ang * ang) / (footprint * footprint));

        sum += color * (shape * brightness * energy);
    }

    return sum;
}

// ---------------------------------------------------------------------------
// Galaxy nebula.
//
// A banded emission nebula: a domain-warped fbm density, cut by dust lanes, held
// to a band around the galactic plane and lit by a narrow palette — cold blue
// body, rose where the gas peaks, pale warm crests. Every term is a function of
// world direction, so the sky is fixed in space.
//
// This replaced an earlier port of Xavier Benech's "Galaxy Trip". That field's
// marble cos() grain plus its per-channel colour ramps (pow(x, 2.18) on red,
// pow(z, 1.88) on blue) turned into rainbow stripes once mapped onto a sphere,
// which reads as psychedelic rather than as a sky.
// ---------------------------------------------------------------------------

vec3 veNebula(vec3 dir)
{
    if (u_NebulaEnabled == 0)
        return vec3(0.0);

    uint  seed = uint(u_SpaceSeed) * 0x9E3779B9u;
    float s    = max(u_NebulaScale, 0.05);
    vec3  p    = dir * s;

    // One level of domain warp: bends the density into filaments without tearing
    // it apart, which is what the old two-level warp did.
    vec3 warp = vec3(
        veFbm3(p * 0.5, 2, 2.0, 0.5, seed),
        veFbm3(p * 0.5 + vec3(5.2), 2, 2.0, 0.5, seed + 0x51ED270Bu),
        veFbm3(p * 0.5 + vec3(11.7), 2, 2.0, 0.5, seed + 0xA511E9B3u));
    vec3 q = p + warp * u_NebulaWarp;

    // Density: fbm remapped to [0,1] and raised to a power, so most of the sky
    // stays empty and only the crests of the field show.
    float d = veFbm3(q, 4, 2.07, 0.5, seed + 0x1B56C4E9u) * 0.5 + 0.5;
    d = pow(clamp(d, 0.0, 1.0), 2.2);

    // Dust lanes: filaments carved OUT of the density, the way real dust sits in
    // front of the emission. Added on top they would only brighten it.
    float dust = veFbm3(q * 2.3 + vec3(3.1), 3, 2.11, 0.5, seed + 0x8F1BBCDCu) * 0.5 + 0.5;
    d *= 1.0 - 0.85 * smoothstep(0.35, 0.75, dust);

    // The galactic plane: a Gaussian in the cosine of the angle from the axis,
    // broken up by a slow variation so the band has thick and thin stretches
    // instead of one uniform stripe.
    float a       = dot(dir, normalize(u_NebulaBandAxis)) / max(u_NebulaBandWidth, 0.01);
    float band    = exp(-a * a);
    float bandVar = veValueNoise3(p * 0.35, seed + 0x27D4EB2Fu);
    float mask    = band * (0.30 + 1.1 * bandVar);

    // Narrow palette, one hue ramp and one highlight — no per-channel exponents,
    // which is what turned the old field into rainbow bands.
    vec3 col = mix(vec3(0.16, 0.22, 0.50), vec3(0.55, 0.22, 0.42),
                   smoothstep(0.25, 0.85, d));
    col = mix(col, vec3(0.95, 0.80, 0.72), 0.75 * smoothstep(0.72, 1.0, d));

    return col * (d * mask * u_NebulaBrightness * u_NebulaTint);
}

// ---------------------------------------------------------------------------
// Space background as seen from planetCenteredOrigin, composited behind the sky.
//
// planetCenteredOrigin is (camera position - planet centre) in metres; it is
// only used for the atmosphere column and the planet occlusion, both of which
// are altitude problems and need the metre-scale vector rather than a chart
// position. Without an atmosphere there is nothing to attenuate or occlude, so
// the field is returned as-is.
// ---------------------------------------------------------------------------
vec3 veSpaceRadiance(vec3 planetCenteredOrigin, vec3 dir)
{
    if (u_SpaceEnabled == 0)
        return vec3(0.0);

    vec3 radiance = veNebula(dir) + veStarField(dir);
    if (radiance.r <= 0.0 && radiance.g <= 0.0 && radiance.b <= 0.0)
        return vec3(0.0);

    if (u_PlanetRadius > 0.0) {
        // Both are at infinity: the whole atmosphere column attenuates them,
        // so the sky reddens and the background drowns in it at the horizon.
        float rKm = length(planetCenteredOrigin) / kLengthUnit;
        float topKm = (u_PlanetRadius + u_AtmosphereHeight) / kLengthUnit;
        rKm = clamp(rKm, u_PlanetRadius / kLengthUnit, topKm);
        float mu = dot(planetCenteredOrigin, dir) / max(length(planetCenteredOrigin), 1.0e-6);
        radiance *= getTransmittanceToTop(rKm, mu);

        // The ground occludes the lower hemisphere; without this the background
        // would show through a planet that has no geometry under the camera.
        vec2 ground = rayShell(planetCenteredOrigin, dir, u_CamAltitude, 0.0);
        if (ground.y >= ground.x && ground.x > 0.0)
            return vec3(0.0);
    }

    return radiance;
}
