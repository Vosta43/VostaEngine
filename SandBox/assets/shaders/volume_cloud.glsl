
// volume_cloud.glsl — volumetric cloud raymarch library (M1: Worley FBM + single scattering).
// Inlined into pbrlighting.glsl AFTER atmosphere.glsl (reuses raySphere, u_SunDirection, u_SunIntensity).
// Gated by u_CloudOuterRadius > u_CloudInnerRadius: leave both 0 to disable clouds entirely.

uniform float u_CloudInnerRadius;    // planet radius + cloud bottom altitude (meters)
uniform float u_CloudOuterRadius;    // planet radius + cloud top altitude  (meters)
uniform float u_CloudSigma;           // view-side extinction (Unity _lightAbsorptionThroughCloud)
uniform float u_CloudLightAbsorption; // sun-side self-shadow (Unity _lightAbsorptionTowardSun)
uniform float u_CloudBaseFrequency;  // base noise frequency (1/meters)
uniform float u_CloudStep;           // fixed march step in meters (Unity-style art constant)

uniform float u_CloudCoverage; // 0 = clear sky, 1 = overcast
uniform float u_CloudFrame;    // frame index (Camera's halton-jitter counter) — animates the jitter phase

// Wind advection: u_CloudWind is a horizontal world-space velocity (m/s, y=0),
// u_CloudTime the engine clock in seconds. sampleShape translates the sample
// position by wind * time so the whole deck drifts.
uniform vec3  u_CloudWind;
uniform float u_CloudTime;
// Per-layer wind, relative to u_CloudWind: u_CloudDetailWindScale scrolls the
// detail field over the shape (1 = locked to the shape advection, >1 = detail
// runs ahead — turbulence eddies move faster than the bulk); u_CloudWeatherDrift
// drifts the weather-map coverage at its own rate in UV units/sec (computed on
// the CPU from the wind's angular speed at the cloud layer).
uniform float u_CloudDetailWindScale = 1.5;
uniform vec2  u_CloudWeatherDrift = vec2(0.0);

// Baked periodic 3D textures (see WorleyNoiseBaker.cpp). Three bands:
//  - u_Shape3D: Perlin-Worley packed into RGBA (R = Perlin base billow, G/B/A =
//    finer Worley F1 octaves), sampled ONCE — the shape FBM loop is gone.
//  - u_Warp3D: two pre-summed FBM fields in R/G (independent seeds) that bend
//    the shape sample position; also a single sample.
//  - u_DetailWorley3D: 3-octave Worley F1 in RGB that erodes the exposed edges.
// The shape and detail bands are independent volumes: different cells/edge AND
// different hash seed, so detail erodes with a Voronoi structure unrelated to
// the shape field instead of a scaled copy of it.
uniform sampler3D u_Shape3D;
uniform float     u_CloudShapeCells;    // base cells per edge of the shape texture (8)
uniform sampler3D u_DetailWorley3D;
uniform float     u_CloudDetailCells;   // cells per edge of the detail texture (16)
uniform float     u_CloudDetailFrequency = 8.0;  // detail band samples Nx the base frequency
uniform sampler3D u_Warp3D;
uniform float     u_CloudWarpCells;     // cells per edge of the warp texture (4)
// Weather map (see WeatherMapBaker): R channel = per-position coverage sampled
// by heightProfile to position the cloud band; G/B unused.
uniform sampler2D u_WeatherMap;
// Unity HDRP "height weights": lerps the stratus (0) and cumulus (1) vertical
// profiles in heightGradient — the cloud-type morph.
uniform float u_CloudHeightGradientWeight = 0.5;
const float CLOUD_PI = 3.14159265359;
// Ambient floor for the in-scatter sum — a uniform so the editor can tune it
// (CloudParams::ambient). Prevents cloud shadows from going pitch black.
uniform float u_CloudAmbient = 0.05;
// 3-stop tone ramp on the cloud's own light transmittance (Unity-style lightmarch
// color map): T_sun -> shadow -> mid -> lit(sunColor). rampOffset1 positions the
// mid->lit transition, rampOffset2 (with its pow3 knee) the shadow->mid one.
uniform vec3  u_CloudShadowColor;
uniform vec3  u_CloudMidColor;
uniform float u_CloudRampOffset1;
uniform float u_CloudRampOffset2;
// Detail-erosion strength (Unity _detailNoiseWeight): scales the detail FBM term
// (baseDensity - fbm * (1-shape)³ * weight, see applyDetail). Higher = sharper
// crevices; too high aliases edges. Hillaire uses 0.2.
uniform float u_CloudDetailErodeWeight = 0.1;
// Final density scale (Unity _densityMultiplier): multiplies the eroded density
// before the saturate. <1 thins the whole cloud, >1 fattens it past the clamp.
uniform float u_CloudDensityMultiplier = 1.0;
// Shape-noise remap bounds [u_CloudShapeMin, u_CloudShapeMax] -> [0,1]: bracket
// what shapeNoise actually produces so the top of the distribution saturates to
// solid cloud and the bottom fades out (see coverageRemap).
uniform float u_CloudShapeMin = 0.35;
uniform float u_CloudShapeMax = 0.90;
// Consecutive rho==0 samples that trigger an early-out in raymarchClouds. The
// shell is thin (2300 m) and at low coverage most rays skim past clumps, so a
// run this long means nothing lies ahead on this ray; exit instead of burning
// the remaining sampleShape calls. At grazing angles a trailing clump beyond a
// wide gap could be cut — raise the count if that shows up on the horizon.
const int CLOUD_EMPTY_BREAK = 6;

// Noise-band scale constants. The density model is three bands, each one a
// SINGLE baked-texture sample per step:
//  - warp:   lowest frequency, bends the sample position (domain warping)
//  - shape:  low frequency, billowy masses (base octave + 3-octave FBM in RGBA)
//  - detail: highest frequency, 3-octave FBM erosion (single fetch in RGB)
const float WARP_FREQ = 0.0005;        // ~1.5x the base wavelength (tunable)
const float WARP_AMT  = 0.6;           // warp displacement, in base-noise units

// Empty-space gate threshold, scaled to coverage. The coverage remap clips
// everything below 1-coverage, so the visible-shape bar rises as coverage
// drops; a FIXED gate only matches one coverage and makes low-coverage clouds
// pay the full shape sample on points that resolve to zero density.
// For the Hillaire shape remap, shapeNoise(base, fbm) = (base - lower)/(1 - lower)
// with lower = fbm - 1 is monotone decreasing in fbm, so it peaks at fbm = 0:
// shapeNoise <= (base + 1)/2. A sample can be visible only if that upper bound
// clears the visible bar, so cull iff base <= 2*visibleBar - 1. No positive
// floor: one would over-cull deep gaps at high coverage whose FBM boost makes
// them visible.
float cloudGateThreshold() {
    float visibleBar = u_CloudShapeMin + (u_CloudShapeMax - u_CloudShapeMin) * (1.0 - u_CloudCoverage);
    return max(0.0, 2.0 * visibleBar - 1.0);
}

// Intersect the view ray with the spherical cloud shell [inner, outer].
// The shell is the region BETWEEN the two spheres: "inside outer" AND "outside inner".
// max(near)/min(far) would give the ball INTERSECTION (below the cloud base), which is wrong.
vec2 rayCloudLayer(vec3 rayOrigin, vec3 rayDir) {
    vec2 innerHit = raySphere(rayOrigin, rayDir, u_CloudInnerRadius);
    vec2 outerHit = raySphere(rayOrigin, rayDir, u_CloudOuterRadius);

    float camDist = length(rayOrigin);
    float entryT = 0.0;
    float exitT  = -1.0;

    if (camDist < u_CloudInnerRadius) {
        // Camera below the cloud base: enter at the base, exit at the top.
        entryT = innerHit.y;
        exitT  = outerHit.y;
    } else if (camDist > u_CloudOuterRadius) {
        // Camera above the cloud top: enter at the top, exit at the base.
        entryT = outerHit.x;
        exitT  = innerHit.x;
    } else {
        // Camera inside the layer: exit whichever boundary lies ahead.
        entryT = 0.0;
        exitT  = max(outerHit.y, innerHit.x);
    }

    entryT = max(entryT, 0.0);
    if (exitT <= entryT) return vec2(-1.0, -1.0);   // miss

    // Planet occlusion — same check as atmosphere.glsl: if the ray hits the planet,
    // stop the cloud raymarch at the surface so clouds behind the planet are hidden.
    vec2 planetHit = raySphere(rayOrigin, rayDir, u_PlanetRadius);
    if (planetHit.x > 0.0)
        exitT = min(exitT, planetHit.x);

    if (exitT <= entryT) return vec2(-1.0, -1.0);
    return vec2(entryT, exitT);
}

// Interleaved Gradient Noise — a one-line, texture-free blue-noise-ish pattern.
// Two hard rules for using it as cloud jitter:
// 1. Feed it PIXEL coordinates (gl_FragCoord.xy). A 0-1 UV changes by only
//    ~0.0006 between neighboring pixels — far smaller than IGN's sub-pixel
//    gradient, so the pattern would stay screen-coherent and the ring bands
//    would survive. Pixel coords change by 1.0 = exactly the pattern period.
// 2. Advance the phase with a fixed golden-ratio step per FRAME, not a
//    wall-clock offset. At variable frame rate a clock offset moves a
//    non-constant amount each frame, so the phase sequence is no longer a
//    uniform sweep of [0,1] and TAA converges worse. u_CloudFrame is the same
//    counter Camera uses for its halton projection jitter, so it always
//    advances by 0.618 per frame and the time coverage stays even.
float cloudBlueNoise(vec2 px) {
    return fract(52.9829189 * fract(dot(px, vec2(0.06711056, 0.00583715))) + u_CloudFrame * 0.6180339887);
}

// Linear rescale of v from [low1, high1] onto [low2, high2].
float remap(float v, float low1, float high1, float low2, float high2) {
    return low2 + (v - low1) * (high2 - low2) / (high1 - low1);
}

// Shape weights for the FBM channels. Channel R of u_Shape3D is the base
// octave (Perlin billow, cells = u_CloudShapeCells); G/B/A hold finer Worley
// octaves at (base << k) cells/edge, so the GBA dot below is a 3-octave FBM in
// one fetch. Hillaire (GDC 2016) style: the base octave is remapped against a
// per-pixel lower bound driven by the FBM, so the fine octaves FILL the gaps
// between Worley cells (connected slabs) instead of merely summing on top
// (isolated puffs). The R weight is unused (the base octave is taken as-is);
// only the GBA ratios matter, normalized to sum 1 at use.
uniform vec4 u_CloudShapeWeights;

float shapeNoise(vec3 q) {
    vec4 n = texture(u_Shape3D, q / u_CloudShapeCells);
    float base = 1.0 - n.r;                        // base octave density
    vec3 fbmw = u_CloudShapeWeights.gba / dot(u_CloudShapeWeights.gba, vec3(1.0));
    float fbm  = 1.0 - dot(n.gba, fbmw);           // finer-octave FBM
    float lower = fbm - 1.0;                       // Hillaire: -(1-FBM)
    return clamp((base - lower) / (1.0 - lower), 0.0, 1.0);
}

// Domain warp: two pre-summed FBM fields (u_Warp3D R/G, independent seeds) act
// as a wind field that deforms the SAMPLE POSITION, stretching neighboring
// Worley cells into connected masses instead of isolated specks. Warp frequency
// is LOWER than the base shape (bigger scale) so it bends whole groups of cells,
// not individual ones; only the horizontal axes (x,z) are warped — clouds shear
// far more horizontally than vertically. (warp - 0.5) centers the field so it
// does not bias the whole cloud deck in one direction. Returns the warped
// position in base-noise CELL units (p * baseFrequency + displacement), which
// shape/detail expect.
vec3 warpDomain(vec3 p) {
    vec3 s = p * WARP_FREQ;
    vec2 w = texture(u_Warp3D, s / u_CloudWarpCells).rg;
    return p * u_CloudBaseFrequency + vec3(w.x - 0.5, 0.0, w.y - 0.5) * WARP_AMT;
}

float heightFraction(vec3 p) {
    return (length(p) - u_CloudInnerRadius) / (u_CloudOuterRadius - u_CloudInnerRadius);
}

// Equirectangular UV for the weather map from the sample direction (planet
// centered, so normalize(p) is the radial direction). u wraps in longitude,
// v runs pole to pole.
vec2 weatherUV(vec3 p) {
    vec3 d = normalize(p);
    float u = atan(d.z, d.x) * (0.5 / CLOUD_PI) + 0.5;
    float v = asin(clamp(d.y, -1.0, 1.0)) * (1.0 / CLOUD_PI) + 0.5;
    return vec2(u, v);
}

// Unity HDRP GetDensityHeight: the weather-map coverage positions a vertical
// band in the cloud layer. gMin/gMax rise with coverage. heightGradient is the
// cumulus profile — a flat-topped slab spanning [gMin, gMax]; heightGradient2
// the stratus profile — a thin sheet pinned to the cloud base, capped at the
// coverage height. u_CloudHeightGradientWeight lerps between them: the
// stratus<->cumulus cloud type morph. Deliberately NOT saturated — the slab
// interior overshoots past 1 so the core stays solid after the final clamp,
// and the negative tails are dropped by the rho>0 checks.
float heightGradient(float h, float cov) {
    float gMin = remap(cov, 0.0, 1.0, 0.1, 0.6);
    float gMax = remap(cov, 0.0, 1.0, gMin, 0.9);
    float cumulus = remap(h, 0.0, gMin, 0.0, 1.0) * remap(h, 1.0, gMax, 0.0, 1.0);
    float stratus = remap(h, 0.0, cov, 1.0, 0.0) * remap(h, 0.0, gMin, 0.0, 1.0);
    return mix(cumulus, stratus, u_CloudHeightGradientWeight);
}

float heightProfile(vec3 p) {
    // Per-position coverage: weather map R scaled by the global slider, drifted by
    // the weather-band wind so coverage patterns slide over the planet. The 0.001
    // floor keeps remap(h, 0, cov, ...) finite at zero coverage (div-by-zero).
    vec2 wuv = weatherUV(p) + u_CloudWeatherDrift * u_CloudTime;
    float cov = clamp(texture(u_WeatherMap, wuv).r * u_CloudCoverage, 0.001, 1.0);
    return heightGradient(heightFraction(p), cov);
}

// Two-stage density remap:
//  - [u_CloudShapeMin, u_CloudShapeMax] -> [0,1]: stretch the noise range so the
//    top of the distribution saturates to solid cloud and the bottom fades out.
//    The bounds must bracket what `shape` actually produces on YOUR noise.
//  - Smoothstep over [1-coverage, 1] -> [0,1]: drives the solid fraction. The
//    ramp (not a hard clip) keeps the density continuous at the coverage
//    threshold — the old remap clipped instantly, aliasing into boundary
//    speckle. At coverage 0.65 everything below 0.35 fades out (separate
//    clumps); at 0.9 it is a near-solid deck.
float coverageRemap(float v) {
    v = remap(v, u_CloudShapeMin, u_CloudShapeMax, 0.0, 1.0);
    return smoothstep(1.0 - u_CloudCoverage, 1.0, v);
}

bool fastCull(vec3 p) {
    vec3 q0 = p * u_CloudBaseFrequency;
    // Base-octave gate: one cheap tap on channel R of the shape texture before
    // paying for warp + the base/FBM shape sample. The threshold is the exact
    // worst case for the Hillaire shape remap (see cloudGateThreshold).
    return (1.0 - texture(u_Shape3D, q0 / u_CloudShapeCells).r) < cloudGateThreshold();
}

float sampleShape(vec3 p, out vec3 q) {
    // Wind advection: translate the sample position horizontally before all
    // noise sampling so the whole deck drifts. heightProfile keeps the TRUE
    // position — the altitude fade depends only on length(p), and advecting it
    // would wobble the layer thickness as the deck slides around the planet.
    vec3 pa = p + u_CloudWind * u_CloudTime;
    float fade = heightProfile(p);
    if (fade <= 0.0 || fastCull(pa)) { q = vec3(0.0); return 0.0; }
    q = warpDomain(pa);
    return coverageRemap(shapeNoise(q)) * fade;
}

float applyDetail(float baseDensity, vec3 q) {
    // Hillaire (GDC 2016) / Unity detail erosion. The erode weight (1-shape)³
    // peaks at the cloud boundary: thin edges are carved hard, solid cores
    // survive. fbm is the inverted Worley FBM (high = dense microstructure).
    // The detail sampling coordinate is advected relative to the shape wind by
    // (detailWindScale - 1) * wind * time — multiplied by baseFrequency to stay in
    // base-cell units like q, so scale 1 locks the detail to the shape and >1 makes
    // the fine noise scroll ahead of it (turbulence eddies outrun the bulk cloud).
    vec3 qd = q + (u_CloudDetailWindScale - 1.0) * u_CloudWind * u_CloudBaseFrequency * u_CloudTime;
    float fbm = 1.0 - dot(texture(u_DetailWorley3D, qd * u_CloudDetailFrequency / u_CloudDetailCells).rgb,
                          vec3(0.625, 0.25, 0.125));
    float oneMinusShape = 1.0 - baseDensity;
    float erodeWeight = oneMinusShape * oneMinusShape * oneMinusShape;
    float cloudDensity = baseDensity - fbm * erodeWeight * u_CloudDetailErodeWeight;
    return clamp(cloudDensity * u_CloudDensityMultiplier, 0.0, 1.0);
}

// Full-density sample (shape + height-blended detail erosion) shared by the
// view and light marches. Both MUST sample the SAME field: the old light march
// dropped the warp and detail for speed, but that sampled a DIFFERENT density
// field than the view, so the light transmittance didn't match the visible
// cloud. Costs more (detail per light step), but lighting is consistent and
// silver-lining crevices match what the view erodes. Reused by the view march
// so the two can never drift apart.
float sampleDensity(vec3 p) {
    vec3 q;
    float density = sampleShape(p, q);
    if (density > 0.0)
        density = applyDetail(density, q);
    return density;
}

// Henyey-Greenstein phase function.
float hg(float mu, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * CLOUD_PI * pow(1.0 + g2 - 2.0 * g * mu, 1.5));
}

// Dual-lobe HG (Hillaire GDC 2016): forward (g=0.5) weighted 0.7 with
// backward (g=-0.2) weighted 0.3 for silver lining.
float phaseCloud(float mu) {
    return hg(mu, 0.5) * 0.7 + hg(mu, -0.2) * 0.3;
}



// Per-sample light march toward the sun: integrates the cloud optical depth from
// the sample to the sun (full shell crossing), converts it to a Beer-Lambert
// transmittance, maps it through a 3-stop tone ramp (shadow -> mid -> sun color),
// then combines with the phase function and ambient floor. T = 1 at the
// sun-facing surface (bright top) falling to 0 in the interior/underside (dark
// bottom) — the self-shadow behind the classic "bright top, dark bottom" look.
// The march covers the FULL sun-side shell crossing (rayCloudLayer(p, sunDir)):
// the old fixed 0.4/baseFrequency step reached only ~320 m vs a 2300 m shell,
// so deep samples never saw the cloud ahead and the shadow sat constant -> flat.
vec3 lightMarchColor(vec3 p, vec3 sunDir, vec3 sunColor, float mu) {
    const int LIGHT_STEPS = 8;
    vec2 shellT = rayCloudLayer(p, sunDir);
    float step = shellT.x < 0.0 ? 0.0 : max(shellT.y, 0.0) / float(LIGHT_STEPS);
    vec3 pos = p + sunDir * step * cloudBlueNoise(gl_FragCoord.xy  + 31.7);
    float opticalDepth = 0.0;
    for(int i = 0;i < LIGHT_STEPS;i ++) {
        opticalDepth += sampleDensity(pos) * step;
        pos += sunDir * step;
    }

    float T_sun = exp(-u_CloudLightAbsorption * opticalDepth);
    // 3-stop tone ramp on the light transmittance (shadow -> mid -> sunColor).
    vec3 rampCol = mix(u_CloudMidColor, sunColor, clamp(T_sun * u_CloudRampOffset1, 0.0, 1.0));
    rampCol      = mix(u_CloudShadowColor, rampCol, clamp(pow(T_sun * u_CloudRampOffset2, 3.0), 0.0, 1.0));
    return rampCol * (u_CloudSigma * phaseCloud(mu)) + sunColor * (u_CloudAmbient * u_CloudSigma);
}

// Returns (in-scattered radiance, remaining transmittance).
vec4 raymarchClouds(vec3 rayOrigin, vec3 rayDir, vec3 sunDir) {
    vec2 shellT = rayCloudLayer(rayOrigin, rayDir);
    if (shellT.x < 0.0) return vec4(0.0, 0.0, 0.0, 1.0);   // miss -> full transmittance

    // Physical sun color at the shell entry: white light attenuated by the
    // atmosphere along the sun ray (transmittance LUT, bound by CloudPass).
    vec3 sunColor = u_SunIntensity * sunTransmittance(rayOrigin + rayDir * shellT.x, sunDir);

    // Fixed step (u_CloudStep), Unity-style: an art-tuned constant decoupled
    // from the noise frequency. Reach = MAX_STEPS * step; at the default 300 m
    // step the march covers ~19 km, so grazing rays near the horizon get culled.
    const int MAX_STEPS = 64;

    // Entry-only jitter: ONE random offset per pixel applied once to the starting
    // depth, then the steps march at a fixed grid. This is the industry pattern
    // (Horizon Zero Dawn, SIGGRAPH 2015): a single blue-noise phase + temporal
    // accumulation, instead of per-step noise which adds a wasted degree of
    // freedom at every sample. The seed must be per-pixel-discontinuous (pixel
    // coords), NOT ray-direction-smooth — a coherent phase leaves the step-count
    // rings intact, exactly the bug the old `hash13(rayDir * 7.31)` jitter had.
    float dist = shellT.x + u_CloudStep * cloudBlueNoise(gl_FragCoord.xy); // 

    vec3 acc = vec3(0.0);
    float transmittance = 1.0;
    int emptySteps = 0;   // consecutive rho==0 samples along this ray

    for (int i = 0; i < MAX_STEPS; i++) {
        if (dist > shellT.y) break;
        float stepLen = u_CloudStep;
        vec3 pos = rayOrigin + rayDir * dist;

        // Full density from the shared sampleDensity() (shape + (1-shape)³
        // detail erosion, see u_CloudDetailErodeWeight) — the same field the light
        // march samples, so self-shadowing matches the visible cloud.
        float rho = sampleDensity(pos);

        if (rho > 0.0) {
            emptySteps = 0;
            float mu = dot(rayDir, sunDir);
            vec3 light = lightMarchColor(pos, sunDir, sunColor, mu);
            acc += transmittance * light * rho * stepLen;
            transmittance *= exp(-u_CloudSigma * rho * stepLen);
        }
        // else if (++emptySteps >= CLOUD_EMPTY_BREAK) {
        //     break;   // N empties in a row -> past the cloud on this ray
        // }
        dist += stepLen;
        if (transmittance < 0.01) break;
    }

    return vec4(acc, transmittance);
}
