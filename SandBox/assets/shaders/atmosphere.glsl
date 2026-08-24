

uniform vec3 u_SunDirection;
uniform float u_SunIntensity;
uniform vec3  u_RayleighScattering;
uniform float u_RayleighScaleHeight;
uniform vec3  u_MieScattering;
uniform float u_MieScaleHeight;
uniform float u_MiePhaseG;
uniform float u_PlanetRadius;
uniform float u_AtmosphereHeight;
uniform float u_Exposure;
uniform sampler2D u_TransmittanceLUT;

const float ATMOSPHERE_PI = 3.14159265359;

const float kLengthUnit = 1000.0;          // meters per km — LUT baked/queried in km
const float kSunAngularRadius = 0.004675;  // radians, physical sun angular radius

vec2 raySphere(vec3 origin, vec3 dir, float radius) {
    float b = dot(origin, dir);
    float c = dot(origin, origin) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return vec2(1.0, -1.0);
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

float rayleighPhase(float mu) {
    return (3.0 / (16.0 * ATMOSPHERE_PI)) * (1.0 + mu * mu);
}

float miePhase(float mu) {
    float g  = u_MiePhaseG;
    float g2 = g * g;
    return (1.0 / (4.0 * ATMOSPHERE_PI)) * (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * mu, 1.5);
}

// LUT lookups (Bruneton-Neyret). The LUT is CPU-baked in km; positions arrive
// in meters, so r and mu are converted here. One lookup replaces the old 8-step
// ray march entirely.
vec2 getTransmittanceUv(float rKm, float mu) {
    float bottomKm = u_PlanetRadius / kLengthUnit;
    float topKm    = (u_PlanetRadius + u_AtmosphereHeight) / kLengthUnit;
    float H = sqrt(topKm * topKm - bottomKm * bottomKm);
    float rho = sqrt(max(rKm * rKm - bottomKm * bottomKm, 0.0));
    float d = -rKm * mu + sqrt(max(rKm * rKm * (mu * mu - 1.0) + topKm * topKm, 0.0));
    float dMin = topKm - rKm;
    float dMax = rho + H;
    return vec2((d - dMin) / max(dMax - dMin, 1e-4), rho / H);
}

vec3 getTransmittanceToTop(float rKm, float mu) {
    return texture(u_TransmittanceLUT, getTransmittanceUv(rKm, mu)).rgb;
}

vec3 getTransmittanceToSun(float rKm, float mu) {
    float bottomKm = u_PlanetRadius / kLengthUnit;
    float sinThetaH = bottomKm / rKm;
    float cosThetaH = -sqrt(max(1.0 - sinThetaH * sinThetaH, 0.0));
    // Above the horizon, clamp mu to the grazing ray so a below-ground path is
    // never sampled; below it the smoothstep fades T to 0, modeling the sun
    // disk's angular radius at the horizon.
    float muClamped = mu >= cosThetaH ? max(mu, cosThetaH) : mu;
    return getTransmittanceToTop(rKm, muClamped)
         * smoothstep(-sinThetaH * kSunAngularRadius, sinThetaH * kSunAngularRadius, mu - cosThetaH);
}

// Note: sunDir must be normalized.
vec3 sunTransmittance(vec3 pos, vec3 sunDir) {
    float rKm = length(pos) / kLengthUnit;
    float mu  = dot(pos, sunDir) / length(pos);
    return getTransmittanceToSun(rKm, mu);
}

// ---------------------------------------------------------------------------
// Single-scattering LUTs (Bruneton-Neyret). CPU-baked 256x128x32 RGB16F 3D
// textures, one each for Rayleigh and Mie, packed as nu * mu_s along x. Each
// texel holds the UN-phased single-scattered radiance integral along the view
// ray; phase functions and solar irradiance are applied here at lookup time.
// Mie gets its own texture so its per-channel color survives (the transmittance
// it multiplies is strongly wavelength-dependent at dusk); a single RGBA
// texture with A = Mie.r forced it gray and washed out the reddening.
// ---------------------------------------------------------------------------
uniform sampler3D u_ScatteringLUT;      // Rayleigh RGB
uniform sampler3D u_MieScatteringLUT;   // Mie RGB

const float kScatteringRSize   = 32.0;
const float kScatteringMuSize  = 128.0;
const float kScatteringMuSSize = 32.0;
const float kScatteringNuSize  = 8.0;
// cos of the max sun zenith angle (Earth); must match AtmosphereBaker::kMuSMin.
const float kMuSMin = -0.20791169082;

// Maps a value x in [0,1] to a texel-center coordinate in [0.5/n, 1-0.5/n], so
// the boundary values sit exactly at the edge texel centers (no extrapolation).
float getTextureCoordFromUnitRange(float x, float size) {
    return 0.5 / size + x * (1.0 - 1.0 / size);
}

float distanceToTopKm(float rKm, float mu) {
    float topKm = (u_PlanetRadius + u_AtmosphereHeight) / kLengthUnit;
    return -rKm * mu + sqrt(max(rKm * rKm * (mu * mu - 1.0) + topKm * topKm, 0.0));
}

// Forward (r, mu, mu_s, nu) -> 4D texture coordinate, the exact inverse of the
// CPU bake's GetRMuMuSNuFromScatteringTextureUvwz (Bruneton
// GetScatteringTextureUvwzFromRMuMuSNu).
vec4 getScatteringTextureUvwzFromRMuMuSNu(float r, float mu, float muS, float nu, bool intersects) {
    float bottomKm = u_PlanetRadius / kLengthUnit;
    float topKm    = (u_PlanetRadius + u_AtmosphereHeight) / kLengthUnit;
    float H = sqrt(topKm * topKm - bottomKm * bottomKm);
    float rho = sqrt(max(r * r - bottomKm * bottomKm, 0.0));
    float u_r = getTextureCoordFromUnitRange(rho / H, kScatteringRSize);

    // Discriminant of the quadratic for the (r, mu) ray / sphere intersections.
    float rMu  = r * mu;
    float discr = rMu * rMu - r * r + bottomKm * bottomKm;
    float u_mu;
    if (intersects) {
        // Ray hits the ground: mu mapped by distance to the ground.
        float d = -rMu - sqrt(max(discr, 0.0));
        float dMin = r - bottomKm;
        float dMax = rho;
        u_mu = 0.5 - 0.5 * getTextureCoordFromUnitRange(
            dMax == dMin ? 0.0 : (d - dMin) / (dMax - dMin), kScatteringMuSize / 2.0);
    } else {
        // Ray reaches the top boundary: mu mapped by distance to the top.
        float d = -rMu + sqrt(max(discr + H * H, 0.0));
        float dMin = topKm - r;
        float dMax = rho + H;
        u_mu = 0.5 + 0.5 * getTextureCoordFromUnitRange(
            (d - dMin) / (dMax - dMin), kScatteringMuSize / 2.0);
    }

    // mu_s mapped by distance to the top boundary from ground level; increased
    // sampling near the horizon (mu_s = 0), zero at mu_s = mu_s_min.
    float d = distanceToTopKm(bottomKm, muS);
    float dMin = topKm - bottomKm;
    float dMax = H;
    float a = (d - dMin) / (dMax - dMin);
    float D = distanceToTopKm(bottomKm, kMuSMin);
    float A = (D - dMin) / (dMax - dMin);
    float u_mu_s = getTextureCoordFromUnitRange(
        max(1.0 - a / A, 0.0) / (1.0 + a), kScatteringMuSSize);

    float u_nu = (nu + 1.0) / 2.0;
    return vec4(u_nu, u_mu_s, u_mu, u_r);
}

// Single-scattered radiance at (r, mu, mu_s, nu) WITHOUT phase functions
// (Bruneton GetScattering). The shared (r, mu, mu_s, nu) -> uvwz mapping is
// computed once; two lookups per LUT emulate the nu-axis quadrilinear. Mie is
// returned as a full vec3 so its transmittance-driven reddening at dusk survives.
void getSingleScattering(float r, float mu, float muS, float nu, bool intersects,
                         out vec3 rayleigh, out vec3 mie) {
    vec4 uvwz = getScatteringTextureUvwzFromRMuMuSNu(r, mu, muS, nu, intersects);
    float texCoordX = uvwz.x * (kScatteringNuSize - 1.0);
    float texX = floor(texCoordX);
    float lerp = texCoordX - texX;
    vec3 uvw0 = vec3((texX + uvwz.y) / kScatteringNuSize, uvwz.z, uvwz.w);
    vec3 uvw1 = vec3((texX + 1.0 + uvwz.y) / kScatteringNuSize, uvwz.z, uvwz.w);
    rayleigh = texture(u_ScatteringLUT, uvw0).rgb * (1.0 - lerp) +
               texture(u_ScatteringLUT, uvw1).rgb * lerp;
    mie      = texture(u_MieScatteringLUT, uvw0).rgb * (1.0 - lerp) +
               texture(u_MieScatteringLUT, uvw1).rgb * lerp;
}

// ---------------------------------------------------------------------------
// Multiple-scattering LUT (Bruneton-Neyret, isotropic approximation). CPU-baked
// 32x128x32 RGB16F 3D texture laid out (mu_s, mu, r) = (x, y, z), no nu axis.
// Each texel holds the un-phased, isotropic multiple-scattered radiance along
// the view ray (integral of T * (betaR*rhoR + betaM*rhoM) * E(q, mu_s) / 4pi ds),
// where E is the irradiance LUT. Added to the single-scattering result WITHOUT a
// phase function; u_MultipleScatteringStrength is AtmosphereParams.scale.
// ---------------------------------------------------------------------------
uniform sampler3D u_MultipleScatteringLUT;
uniform float u_MultipleScatteringStrength;

vec3 getMultipleScattering(float r, float mu, float muS, bool intersects) {
    vec4 uvwz = getScatteringTextureUvwzFromRMuMuSNu(r, mu, muS, 0.0, intersects);
    // The multiple LUT has no nu axis: x is directly u_mu_s.
    vec3 uvw = vec3(uvwz.y, uvwz.z, uvwz.w);
    return texture(u_MultipleScatteringLUT, uvw).rgb * u_MultipleScatteringStrength;
}

// Sky radiance from the precomputed single-scattering LUT (Bruneton
// GetSkyRadiance, scattering order 1). Replaces the analytic 16-step ray march.
vec3 computeAtmosphereLUT(vec3 origin, vec3 dir) {
    float bottomKm = u_PlanetRadius / kLengthUnit;
    float topKm    = (u_PlanetRadius + u_AtmosphereHeight) / kLengthUnit;
    // No atmosphere entity -> the uniforms stay 0; bail out before sampling a
    // degenerate LUT (NaN coordinates).
    if (topKm <= bottomKm) return vec3(0.0);

    float rKm = clamp(length(origin) / kLengthUnit, bottomKm, topKm);
    vec3 up   = origin / max(length(origin), 1e-6);
    float mu  = clamp(dot(up, dir), -1.0, 1.0);
    float muS = clamp(dot(up, u_SunDirection), -1.0, 1.0);
    float nu  = clamp(dot(dir, u_SunDirection), -1.0, 1.0);
    bool intersects = mu < 0.0 &&
        rKm * rKm * (mu * mu - 1.0) + bottomKm * bottomKm >= 0.0;

    vec3 rayleigh, mie;
    getSingleScattering(rKm, mu, muS, nu, intersects, rayleigh, mie);
    vec3 multiple = getMultipleScattering(rKm, mu, muS, intersects);

    return u_SunIntensity * (rayleigh * rayleighPhase(nu) + mie * miePhase(nu) + multiple);
}

float sunDisk(float mu){
    return smoothstep(0.9995, 0.99995, mu);
}

// Sky radiance reflected along dir — the analytic atmosphere + sun disk evaluated
// in the reflection direction, with a blend amount packed in alpha: 0 where dir
// points below the horizon (the scene cubemap takes over there) and fading to 0
// on rough surfaces (a single direction sample has no mip blur — the cubemap's
// mip blur is the right answer). planetCenteredOrigin = (camera - planet center).
vec4 reflectedSkyRadiance(vec3 planetCenteredOrigin, vec3 dir, float roughness) {
    vec3 sky = vec3(0.0);
    float amount = 0.0;
    if (u_PlanetRadius > 0.0) {   // an atmosphere entity is present
        vec3 sunColor = u_SunIntensity * sunTransmittance(planetCenteredOrigin, u_SunDirection);
        sky = computeAtmosphereLUT(planetCenteredOrigin, dir)
            + sunColor * sunDisk(dot(dir, u_SunDirection));
        // Reflection ray toward the planet -> ground/geometry (keep the cubemap);
        // toward open space -> sky (analytic atmosphere). raySphere returns (1,-1)
        // on a miss (far < near), so a hit in front of the camera needs
        // far >= near && near > 0.
        vec2 planetHit = raySphere(planetCenteredOrigin, dir, u_PlanetRadius);
        bool hitsGround = (planetHit.y >= planetHit.x) && (planetHit.x > 0.0);
        amount = smoothstep(0.0, 10000.0, hitsGround ? planetHit.x : 1.0e9);
        amount *= 1.0 - smoothstep(0.1, 0.3, roughness);
    }
    return vec4(sky, amount);
}
