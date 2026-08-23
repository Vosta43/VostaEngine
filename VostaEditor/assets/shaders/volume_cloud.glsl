
// volume_cloud.glsl — volumetric cloud raymarch library (M1: Worley FBM + single scattering).
// Inlined into pbrlighting.glsl AFTER atmosphere.glsl (reuses raySphere, u_SunDirection, u_SunIntensity).
// Gated by u_CloudOuterRadius > u_CloudInnerRadius: leave both 0 to disable clouds entirely.

uniform float u_CloudInnerRadius;    // planet radius + cloud bottom altitude (meters)
uniform float u_CloudOuterRadius;    // planet radius + cloud top altitude  (meters)
uniform float u_CloudSigma;          // extinction coefficient
uniform float u_CloudBaseFrequency;  // base noise frequency (1/meters)

uniform float u_CloudCoverage; // 0 = clear sky, 1 = overcast
uniform float u_CloudTime;     // wall clock (seconds) — animates the jitter phase each frame

// Baked periodic Worley 3D textures (see WorleyNoiseBaker.cpp). Three bands:
//  - u_Shape3D: 4-octave Worley F1 packed into RGBA channels (one octave per
//    channel), sampled ONCE and dotted with weights — the shape FBM loop is gone.
//  - u_Warp3D: two pre-summed FBM fields in R/G (independent seeds) that bend
//    the shape sample position; also a single sample.
//  - u_DetailWorley3D: single-octave Worley F1 that erodes the exposed edges.
// The shape and detail bands are independent volumes: different cells/edge AND
// different hash seed, so detail erodes with a Voronoi structure unrelated to
// the shape field instead of a scaled copy of it.
uniform sampler3D u_Shape3D;
uniform float     u_CloudShapeCells;    // base cells per edge of the shape texture (8)
uniform sampler3D u_DetailWorley3D;
uniform float     u_CloudDetailCells;   // cells per edge of the detail texture (16)
uniform sampler3D u_Warp3D;
uniform float     u_CloudWarpCells;     // cells per edge of the warp texture (4)
const float CLOUD_PI = 3.14159265359;
const float CLOUD_AMBIENT = 0.05;
const float CLOUD_COARSE_GATE = 0.15;   // gate floor — keeps culling conservative at high coverage
const float CLOUD_SHAPE_MIN = 0.35;
const float CLOUD_SHAPE_MAX = 0.90;
// Consecutive rho==0 samples that trigger an early-out in raymarchClouds. The
// shell is thin (2300 m) and at low coverage most rays skim past clumps, so a
// run this long means nothing lies ahead on this ray; exit instead of burning
// the remaining sampleShape calls. At grazing angles a trailing clump beyond a
// wide gap could be cut — raise the count if that shows up on the horizon.
const int CLOUD_EMPTY_BREAK = 6;

// Noise-band scale constants. The density model is three bands, each one a
// SINGLE baked-texture sample per step:
//  - warp:   lowest frequency, bends the sample position (domain warping)
//  - shape:  low frequency, big billowy masses (4-octave FBM in RGBA)
//  - detail: highest frequency, erodes exposed edges (single octave)
const float WARP_FREQ = 0.0005;        // ~1.5x the base wavelength (tunable)
const float WARP_AMT  = 0.6;           // warp displacement, in base-noise units
const float DETAIL_FREQ_SCALE = 8.0;   // detail band samples 8x the base frequency

// Empty-space gate threshold, scaled to coverage. The coverage remap clips
// everything below 1-coverage, so the visible-shape bar rises as coverage
// drops; a FIXED gate only matches one coverage and makes low-coverage clouds
// pay the full shape sample on points that resolve to zero density.
// Shape is bounded above by 0.5 + 0.5*firstOctave (the first octave carries
// half the FBM weight; higher octaves only add FBM, lowering shape), so any
// sample whose upper bound is below the visible bar can never be visible.
float cloudGateThreshold() {
    float visibleBar = CLOUD_SHAPE_MIN + (CLOUD_SHAPE_MAX - CLOUD_SHAPE_MIN) * (1.0 - u_CloudCoverage);
    return max(CLOUD_COARSE_GATE, 2.0 * visibleBar - 1.0);
}

// Intersect the view ray with the spherical cloud shell [inner, outer].
// The shell is the region BETWEEN the two spheres: "inside outer" AND "outside inner".
// max(near)/min(far) would give the ball INTERSECTION (below the cloud base), which is wrong.
vec2 rayCloudLayer(vec3 ro, vec3 rd) {
    vec2 tInner = raySphere(ro, rd, u_CloudInnerRadius);
    vec2 tOuter = raySphere(ro, rd, u_CloudOuterRadius);

    float r = length(ro);
    float tEntry = 0.0;
    float tExit  = -1.0;

    if (r < u_CloudInnerRadius) {
        // Camera below the cloud base: enter at the base, exit at the top.
        tEntry = tInner.y;
        tExit  = tOuter.y;
    } else if (r > u_CloudOuterRadius) {
        // Camera above the cloud top: enter at the top, exit at the base.
        tEntry = tOuter.x;
        tExit  = tInner.x;
    } else {
        // Camera inside the layer: exit whichever boundary lies ahead.
        tEntry = 0.0;
        tExit  = max(tOuter.y, tInner.x);
    }

    tEntry = max(tEntry, 0.0);
    if (tExit <= tEntry) return vec2(-1.0, -1.0);   // miss

    // Planet occlusion — same check as atmosphere.glsl: if the ray hits the planet,
    // stop the cloud raymarch at the surface so clouds behind the planet are hidden.
    vec2 tPlanet = raySphere(ro, rd, u_PlanetRadius);
    if (tPlanet.x > 0.0)
        tExit = min(tExit, tPlanet.x);

    if (tExit <= tEntry) return vec2(-1.0, -1.0);
    return vec2(tEntry, tExit);
}

// Interleaved Gradient Noise — a one-line, texture-free blue-noise-ish pattern.
// Two hard rules for using it as cloud jitter:
// 1. Feed it PIXEL coordinates (gl_FragCoord.xy). A 0-1 UV changes by only
//    ~0.0006 between neighboring pixels — far smaller than IGN's sub-pixel
//    gradient, so the pattern would stay screen-coherent and the ring bands
//    would survive. Pixel coords change by 1.0 = exactly the pattern period.
// 2. Animate it with a non-integer pixel translation so each frame samples a
//    fresh phase. That is what lets temporal accumulation (TAA) converge.
float cloudBlueNoise(vec2 px) {
    px += u_CloudTime * 4.0;
    return fract(52.9829189 * fract(dot(px, vec2(0.06711056, 0.00583715))));
}

// Band-shaped density: zero at both layer bounds, plateau in the middle.
float heightGradient(float h) {
    return smoothstep(0.0, 0.25, h) * (1.0 - smoothstep(0.6, 1.0, h));
}

// Linear rescale of v from [low1, high1] onto [low2, high2].
float remap(float v, float low1, float high1, float low2, float high2) {
    return low2 + (v - low1) * (high2 - low2) / (high1 - low1);
}

// FBM octave weights for the shape texture: channel k of u_Shape3D holds F1 at
// (base << k) cells/edge, so the dot below is a 4-octave FBM in a single
// texture fetch. Weights sum to 1, keeping the shape value centered in [0,1].
const vec4 SHAPE_WEIGHTS = vec4(0.5, 0.25, 0.125, 0.125);

float shapeNoise(vec3 q) {
    return 1.0 - dot(texture(u_Shape3D, q / u_CloudShapeCells).rgba, SHAPE_WEIGHTS);
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

float heightProfile(vec3 p) {
        float h = (length(p) - u_CloudInnerRadius) / (u_CloudOuterRadius - u_CloudInnerRadius);
        return heightGradient(h);
}

// Two-stage density remap:
//  - [CLOUD_SHAPE_MIN, CLOUD_SHAPE_MAX] -> [0,1]: stretch the noise range so the
//    top of the distribution saturates to solid cloud and the bottom clips to
//    nothing — instead of smoothstep, which leaves a fog of mid-values
//    everywhere. The bounds must bracket what `shape` actually produces on YOUR
//    noise.
//  - [1-coverage, 1] -> [0,1]: drives the solid fraction directly. At coverage
//    0.65 everything below 0.35 clips to zero (separate clumps); at 0.9 it is a
//    near-solid deck. This is what turns "mist everywhere" into "clumps".
float coverageRemap(float v) {
    v = remap(v, CLOUD_SHAPE_MIN, CLOUD_SHAPE_MAX, 0.0, 1.0);
    return remap(v, 1.0 - u_CloudCoverage, 1.0, 0.0, 1.0);
}

bool fastCull(vec3 p) {
    vec3 q0 = p * u_CloudBaseFrequency;
    // Octave-0 channel of the shape texture plays the old single-octave gate: a
    // cheap 1-tap reject before paying for warp + the 4-octave shape sample.
    return (1.0 - texture(u_Shape3D, q0 / u_CloudShapeCells).r) < cloudGateThreshold();
}

float sampleShape(vec3 p, out vec3 q) {
    float fade = heightProfile(p);
    if (fade <= 0.0 || fastCull(p)) { q = vec3(0.0); return 0.0; }
    q = warpDomain(p);
    return coverageRemap(shapeNoise(q)) * fade;
}

float applyDetail(float baseDensity, vec3 q) {
    float d = texture(u_DetailWorley3D, q * DETAIL_FREQ_SCALE / u_CloudDetailCells).r;
    d = 1.0 - d * d;               
    return clamp(baseDensity * d, 0.0, 1.0);
}

// Cheap density for the sun-light march: no warp, a single shape sample.
// Transmittance is a soft integral over 6 steps, so high-frequency detail
// averages out; only the low-frequency shape survives in exp(-sigma * od).
float sampleDensityLight(vec3 p) {
    float h = (length(p) - u_CloudInnerRadius) / (u_CloudOuterRadius - u_CloudInnerRadius);
    float fade = heightGradient(h);
    if (fade <= 0.0) return 0.0;

    vec3 q0 = p * u_CloudBaseFrequency;
    // One fetch returns the whole 4-octave FBM; the shape value doubles as the
    // gate, so no separate coarse reject tap is needed here.
    float shape = 1.0 - dot(texture(u_Shape3D, q0 / u_CloudShapeCells).rgba, SHAPE_WEIGHTS);
    if (shape < cloudGateThreshold()) return 0.0;

    float density = remap(shape, CLOUD_SHAPE_MIN, CLOUD_SHAPE_MAX, 0.0, 1.0);
    density = remap(density, 1.0 - u_CloudCoverage, 1.0, 0.0, 1.0);
    return clamp(density, 0.0, 1.0) * fade;
}

// Henyey-Greenstein phase function.
float hg(float mu, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * CLOUD_PI * pow(1.0 + g2 - 2.0 * g * mu, 1.5));
}

// Dual-lobe: strong forward (g=0.8) blended with backward (g=-0.3) for silver lining.
float phaseCloud(float mu) {
    return mix(hg(mu, 0.8), hg(mu, -0.3), 0.5);
}

// Light-source in-scatter for one sample: Beer's Powder Law instead of pure
// Beer-Lambert transmittance. 1 - exp(-2*sigma*od) saturates as optical depth
// grows, so thin edges and crevices scatter back bright while thick cloud
// absorbs — paired with the view-side Beer attenuation this gives the
// "cracks brighter than bumps" silver-lining look.
float lightTransmittance(vec3 p, vec3 sunDir) {
    const int LIGHT_STEPS = 6;
    float step = clamp(0.4/u_CloudBaseFrequency,4.0,40.0);
    vec3 pos = p + sunDir * step * 0.3 * cloudBlueNoise(gl_FragCoord.xy  + 31.7);
    float opticalDepth = 0.0;
    for(int i = 0;i < LIGHT_STEPS;i ++) {
        opticalDepth += sampleDensityLight(pos) * step;
        pos += sunDir * step;
    }

    return 1.0 - exp(-2.0 * u_CloudSigma * opticalDepth);
}

float stepAt(float t) {
    // Fixed step, sized to the base noise wavelength — constant sampling density
    // inside the cloud shell. The old distance-growing ramp (t / STEP_RAMP)
    // undersampled the detail band on the far side of grazing rays and blurred
    // its erosion back into cotton; rayCloudLayer already bounds the march to
    // the shell, so there is no empty space left to save steps on.
    return clamp(0.5 / u_CloudBaseFrequency,50.0,500.0);
}

// Returns (in-scattered radiance, remaining transmittance).
vec4 raymarchClouds(vec3 ro, vec3 rd, vec3 sunDir) {
    vec2 t = rayCloudLayer(ro, rd);
    if (t.x < 0.0) return vec4(0.0, 0.0, 0.0, 1.0);   // miss -> full transmittance

    // Distance-scaled steps, not a fixed step count: a constant count spreads
    // steps over the whole path, so long horizon rays (~90 km) get huge steps
    // that under-sample the noise and produce speckle. stepAt() sizes the step
    // to the base noise wavelength (uniform sampling density at every view
    // angle) and grows it with distance so far-away, mostly-empty regions are
    // undersampled.
    const int MAX_STEPS = 64;   // perf: was 128 — halves the max march cost (64 steps x 500m = ~32km reach)

    // Entry-only jitter: ONE random offset per pixel applied once to the starting
    // depth, then the steps march at a fixed grid. This is the industry pattern
    // (Horizon Zero Dawn, SIGGRAPH 2015): a single blue-noise phase + temporal
    // accumulation, instead of per-step noise which adds a wasted degree of
    // freedom at every sample. The seed must be per-pixel-discontinuous (pixel
    // coords), NOT ray-direction-smooth — a coherent phase leaves the step-count
    // rings intact, exactly the bug the old `hash13(rd * 7.31)` jitter had.
    float tcur = t.x + stepAt(t.x) * cloudBlueNoise(gl_FragCoord.xy);

    vec3 acc = vec3(0.0);
    float transmittance = 1.0;
    int emptySteps = 0;   // consecutive rho==0 samples along this ray

    for (int i = 0; i < MAX_STEPS; i++) {
        if (tcur > t.y) break;
        float stepLen = stepAt(tcur);
        vec3 p = ro + rd * tcur;

        // Two-band density split. sampleShape() runs every step (fade + gate +
        // warp + 2-octave shape, ~11 taps vs 14 for the old single 5-octave
        // pass) — it is all the transmittance integral needs to resolve.
        // applyDetail() runs on 1 in 4 steps: erode() only carves exposed edges
        // and the march's integral averages out the skipped samples, saving ~2
        // taps/step on average. This biases the deck slightly denser — re-check
        // CLOUD_SHAPE_MIN/MAX after this change.
        vec3 q;
        float rho = sampleShape(p, q);
        if (rho > 0.0 && (i & 3) == 0)
            rho = applyDetail(rho, q);

        if (rho > 0.0) {
            emptySteps = 0;
            float T_sun = lightTransmittance(p, sunDir);
            float mu = dot(rd, sunDir);
            vec3 sunColor = vec3(1.0) * u_SunIntensity;
            vec3 light = sunColor * (u_CloudSigma * phaseCloud(mu) * T_sun + CLOUD_AMBIENT * u_CloudSigma);
            acc += transmittance * light * rho * stepLen;
            transmittance *= exp(-u_CloudSigma * rho * stepLen);
        } else if (++emptySteps >= CLOUD_EMPTY_BREAK) {
            break;   // N empties in a row -> past the cloud on this ray
        }
        tcur += stepLen;
        if (transmittance < 0.01) break;
    }

    return vec4(acc, transmittance);
}
