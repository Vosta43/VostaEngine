#include "vepch.h"
#include "AtmosphereBaker.h"
#include "Core/Log.h"

#include <cmath>
#include <algorithm>

namespace ve {

	namespace {
		// Bake and query in km, not meters: r^2 ~ 4e13 in meters loses float
		// precision near the ground (rho = sqrt(r^2 - bottom^2) bands), while
		// in km r stays ~6.4e3 and the mapping stays smooth.
		constexpr float kLengthUnit = 1000.0f; // meters per km
		constexpr uint32_t kLUTWidth = 256;    // mu axis (sun-zenith cosine)
		constexpr uint32_t kLUTHeight = 64;    // r axis
		constexpr int kRaySteps = 500;

		// Scattering LUT: the 4D (r, mu, mu_s, nu) parameter space is packed into
		// a 3D texture, nu * mu_s along x (Bruneton's layout).
		constexpr uint32_t kScatteringRSize = 32;
		constexpr uint32_t kScatteringMuSize = 128;
		constexpr uint32_t kScatteringMuSSize = 32;
		constexpr uint32_t kScatteringNuSize = 8;
		constexpr uint32_t kScatteringWidth = kScatteringNuSize * kScatteringMuSSize; // 256
		constexpr uint32_t kScatteringHeight = kScatteringMuSize;                     // 128
		constexpr uint32_t kScatteringDepth = kScatteringRSize;                       // 32

		constexpr int kSampleCount = 50;              // trapezoid intervals for single scattering
		constexpr float kPi = 3.14159265359f;

		// Irradiance LUT: 2D (r, mu_s), 256x64 RGB16F. mu_s mapped linearly across
		// x ([-1,1] -> [0,1]); r mapped as rho/H (same as the transmittance LUT).
		constexpr uint32_t kIrrWidth = 256;
		constexpr uint32_t kIrrHeight = 64;
		constexpr int kIrrMuSteps = 16;   // hemisphere cosine samples
		constexpr int kIrrPhiSteps = 24;  // azimuth samples (single-scattering part only)
		constexpr int kMultipleIterations = 3;
		// cos of the max sun zenith angle (Earth); the engine has no such param.
		const float kMuSMin = std::cos(102.0f * kPi / 180.0f);
		// radians, physical sun angular radius (matches atmosphere.glsl).
		constexpr float kSunAngularRadius = 0.004675f;

		// Raw CPU-side single-scattering LUT data, kept as two RGB16F buffers so
		// the Mie channel keeps its full per-channel color (see ScatteringLUTPair).
		struct ScatteringLUTData {
			std::vector<float> rayleigh;   // 256*128*32 * 3
			std::vector<float> mie;        // 256*128*32 * 3
		};

		struct BakeAtmosphere {
			float bottomRadius;  // km
			float topRadius;     // km
			float expScaleR;     // km^-1
			float expScaleM;     // km^-1
			float atmoHeight;    // km
			glm::vec3 betaR;     // km^-1, per-channel Rayleigh extinction == scattering
			glm::vec3 betaM;     // km^-1, per-channel Mie extinction == scattering
		};

		BakeAtmosphere makeAtmosphere(const AtmosphereParams& p) {
			BakeAtmosphere a;
			a.bottomRadius = p.planetRadius / kLengthUnit;
			a.topRadius = (p.planetRadius + p.atmosphereHeight) / kLengthUnit;
			a.expScaleR = kLengthUnit / p.rayleighScaleHeight;
			a.expScaleM = kLengthUnit / p.mieScaleHeight;
			a.atmoHeight = p.atmosphereHeight / kLengthUnit;
			a.betaR = p.rayleighScattering * kLengthUnit;
			a.betaM = p.mieScattering * kLengthUnit;
			return a;
		}

		float clampCosine(float mu) {
			return std::clamp(mu, -1.0f, 1.0f);
		}

		float clampRadius(float r, const BakeAtmosphere& a) {
			return std::clamp(r, a.bottomRadius, a.topRadius);
		}

		float distanceToTop(float r, float mu, const BakeAtmosphere& a) {
			// Guard: r^2*(mu^2-1) + top^2 stays >= 0 mathematically but rounding can
			// dip it slightly negative near the top boundary, and sqrt(NaN) poisons
			// the whole bake.
			return -r * mu + std::sqrt(std::max(r * r * (mu * mu - 1.0f) + a.topRadius * a.topRadius, 0.0f));
		}

		float distanceToBottom(float r, float mu, const BakeAtmosphere& a) {
			return -r * mu - std::sqrt(std::max(r * r * (mu * mu - 1.0f) + a.bottomRadius * a.bottomRadius, 0.0f));
		}

		// Engine density profiles, replicated verbatim from atmosphere.glsl
		// (h in km, expScale per km).
		float rayleighDensity(float h, const BakeAtmosphere& a) {
			return std::max(0.0f, std::exp(-a.expScaleR * h) - std::exp(-a.expScaleR * a.atmoHeight));
		}

		float mieDensity(float h, const BakeAtmosphere& a) {
			return std::exp(-a.expScaleM * h) * (h < a.atmoHeight ? 1.0f : 0.0f);
		}

		// (u - 0.5/size) / (1 - 1/size): inverse of GetTextureCoordFromUnitRange.
		// Maps a texel-center coordinate back to [0,1] with the boundary values
		// stored at the edge texel centers (avoids extrapolation on lookup).
		float getUnitRangeFromTextureCoord(float u, int size) {
			return (u - 0.5f / (float)size) / (1.0f - 1.0f / (float)size);
		}

		float smoothstepF(float e0, float e1, float x) {
			const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		// Shared CPU transmittance table (256x64x3 RGB). Texel (x, y) stores
		// T(r, mu) with the naive texel-center parameter mapping (u = (x+0.5)/W),
		// matching the layout the engine's atmosphere.glsl samples at render time.
		std::vector<float> computeTransmittanceTable(const BakeAtmosphere& a) {
			const float H = std::sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);
			const float bottom = a.bottomRadius;
			const float top = a.topRadius;

			std::vector<float> lut(kLUTWidth * kLUTHeight * 3);

			for (uint32_t y = 0; y < kLUTHeight; ++y) {
				for (uint32_t x = 0; x < kLUTWidth; ++x) {

					// --- texel center -> (r, mu): GetRMuFromTransmittanceTextureUv ---
					const float u = (float)(x + 0.5) / (float)kLUTWidth;   // mu axis
					const float v = (float)(y + 0.5) / (float)kLUTHeight;  // r axis
					const float rho = H * v;
					const float r = std::sqrt(rho * rho + bottom * bottom);
					const float dMin = top - r;
					const float dMax = rho + H;
					const float d = dMin + (dMax - dMin) * u;
					float mu = (H * H - rho * rho - d * d) / (2.0f * r * d);
					mu = clampCosine(mu);

					// --- distance to the top atmosphere boundary (positive root) ---
					const float sTop = distanceToTop(r, mu, a);

					// --- trapezoidal optical depth to the top boundary ---
					float odR = 0.0f, odM = 0.0f;
					const float ds = sTop / (float)(kRaySteps - 1);
					for (int i = 0; i < kRaySteps; ++i) {
						const float s = (float)i * ds;
						// r(s)^2 = r^2 + s^2 + 2rs*mu (law of cosines about the planet center).
						const float h = std::max(std::sqrt(r * r + s * s + 2.0f * s * r * mu) - bottom, 0.0f);
						const float dr = rayleighDensity(h, a);
						const float dm = mieDensity(h, a);
						const float w = (i == 0 || i == kRaySteps - 1) ? 0.5f : 1.0f;
						odR += w * dr * ds;
						odM += w * dm * ds;
					}

					// T = exp(-(odR*betaR + odM*betaM)); od in km, beta in km^-1 -> dimensionless.
					const glm::vec3 opticalDepth = odR * a.betaR + odM * a.betaM;
					const glm::vec3 transmittance = glm::exp(-opticalDepth);

					const uint32_t idx = (y * kLUTWidth + x) * 3;
					lut[idx + 0] = transmittance.r;
					lut[idx + 1] = transmittance.g;
					lut[idx + 2] = transmittance.b;
				}
			}
			return lut;
		}

		// CPU replica of atmosphere.glsl getTransmittanceUv + texture(): same naive
		// texel-center mapping, bilinear interpolation, clamp-to-edge. Guarantees the
		// scattering bake reads exactly what the GPU transmittance LUT returns.
		glm::vec3 sampleTransmittance(const std::vector<float>& table, float r, float mu, const BakeAtmosphere& a) {
			// Domain guard: a degenerate r/mu (NaN from a rounding-dipped sqrt, or a
			// query point outside the shell) would make x/y non-finite, and
			// (int)std::floor(NaN) is UB that indexes the table out of bounds. Clamp
			// r/mu to the valid shell (matching the GPU shader's rKm clamp) and bail
			// out with T = 1 if the mapping still isn't finite.
			r = clampRadius(r, a);
			mu = clampCosine(mu);

			const float H = std::sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);
			const float rho = std::sqrt(std::max(r * r - a.bottomRadius * a.bottomRadius, 0.0f));
			const float d = distanceToTop(r, mu, a);
			const float dMin = a.topRadius - r;
			const float dMax = rho + H;
			const float x = (d - dMin) / std::max(dMax - dMin, 1e-4f);
			const float y = rho / H;
			if (!std::isfinite(x) || !std::isfinite(y))
				return glm::vec3(1.0f);

			// uv in [0,1] -> texel coordinate (uv * size - 0.5), clamped (CLAMP_TO_EDGE).
			float tx = std::clamp(x * (float)kLUTWidth - 0.5f, 0.0f, (float)(kLUTWidth - 1));
			float ty = std::clamp(y * (float)kLUTHeight - 0.5f, 0.0f, (float)(kLUTHeight - 1));

			const int x0 = (int)std::floor(tx);
			const int y0 = (int)std::floor(ty);
			const int x1 = std::min(x0 + 1, (int)(kLUTWidth - 1));
			const int y1 = std::min(y0 + 1, (int)(kLUTHeight - 1));
			const float fx = tx - (float)x0;
			const float fy = ty - (float)y0;

			auto texel = [&](int xi, int yi) {
				const uint32_t i = ((uint32_t)yi * kLUTWidth + (uint32_t)xi) * 3;
				return glm::vec3(table[i], table[i + 1], table[i + 2]);
			};

			const glm::vec3 t00 = texel(x0, y0);
			const glm::vec3 t10 = texel(x1, y0);
			const glm::vec3 t01 = texel(x0, y1);
			const glm::vec3 t11 = texel(x1, y1);

			return glm::mix(glm::mix(t00, t10, fx), glm::mix(t01, t11, fx), fy);
		}

		// Transmittance between a point and the Sun, replicating atmosphere.glsl
		// getTransmittanceToSun (smoothstep sun-disc fade below the horizon).
		glm::vec3 transmittanceToSun(const std::vector<float>& table, float r, float muS, const BakeAtmosphere& a) {
			const float sinThetaH = a.bottomRadius / r;
			const float cosThetaH = -std::sqrt(std::max(1.0f - sinThetaH * sinThetaH, 0.0f));
			const float muClamped = muS >= cosThetaH ? std::max(muS, cosThetaH) : muS;
			const float fade = smoothstepF(-sinThetaH * kSunAngularRadius, sinThetaH * kSunAngularRadius, muS - cosThetaH);
			return sampleTransmittance(table, r, muClamped, a) * fade;
		}

		// Transmittance along the primary path p -> q (Bruneton GetTransmittance):
		// ratio of two top-boundary transmittances, clamped to <= 1.
		glm::vec3 getTransmittance(const std::vector<float>& table, float r, float mu, float d,
		                           bool intersects, const BakeAtmosphere& a) {
			const float rD = clampRadius(std::sqrt(std::max(d * d + 2.0f * r * mu * d + r * r, 0.0f)), a);
			const float muD = clampCosine((r * mu + d) / rD);

			glm::vec3 t;
			if (intersects) {
				t = sampleTransmittance(table, rD, -muD, a) / sampleTransmittance(table, r, -mu, a);
			} else {
				t = sampleTransmittance(table, r, mu, a) / sampleTransmittance(table, rD, muD, a);
			}
			return glm::min(t, glm::vec3(1.0f));
		}

		// Inverse of the 3D->4D packing: recovers (r, mu, mu_s, nu) + ground-intersection
		// flag from a texel-center uvwz coordinate (Bruneton GetRMuMuSNuFromScatteringTextureUvwz).
		void getRMuMuSNuFromScatteringUvwz(const BakeAtmosphere& a, glm::vec4 uvwz,
			float& r, float& mu, float& muS, float& nu, bool& intersects) {
			const float H = std::sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);

			const float rho = H * getUnitRangeFromTextureCoord(uvwz.w, (int)kScatteringRSize);
			r = std::sqrt(rho * rho + a.bottomRadius * a.bottomRadius);

			if (uvwz.z < 0.5f) {
				const float dMin = r - a.bottomRadius;
				const float dMax = rho;
				const float d = dMin + (dMax - dMin) *
				    getUnitRangeFromTextureCoord(1.0f - 2.0f * uvwz.z, (int)(kScatteringMuSize / 2));
				mu = d == 0.0f ? -1.0f : clampCosine(-(rho * rho + d * d) / (2.0f * r * d));
				intersects = true;
			} else {
				const float dMin = a.topRadius - r;
				const float dMax = rho + H;
				const float d = dMin + (dMax - dMin) *
				    getUnitRangeFromTextureCoord(2.0f * uvwz.z - 1.0f, (int)(kScatteringMuSize / 2));
				mu = d == 0.0f ? 1.0f : clampCosine((H * H - rho * rho - d * d) / (2.0f * r * d));
				intersects = false;
			}

			const float xMuS = getUnitRangeFromTextureCoord(uvwz.y, (int)kScatteringMuSSize);
			const float dMin = a.topRadius - a.bottomRadius;
			const float dMax = H;
			const float D = distanceToTop(a.bottomRadius, kMuSMin, a);
			const float A = (D - dMin) / (dMax - dMin);
			const float a_ = (A - xMuS * A) / (1.0f + xMuS * A);
			const float d = dMin + std::min(a_, A) * (dMax - dMin);
			muS = d == 0.0f ? 1.0f : clampCosine((H * H - d * d) / (2.0f * a.bottomRadius * d));

			nu = clampCosine(uvwz.x * 2.0f - 1.0f);
		}

		// Single-scattered radiance along the view ray, WITHOUT phase functions and
		// solar irradiance (added at render time). 50-interval trapezoid, replicating
		// Bruneton ComputeSingleScattering. Units cancel: beta_km * ds_km = beta_m * ds_m,
		// so the stored value matches the engine's per-meter analytic scale.
		void computeSingleScattering(const std::vector<float>& table, const BakeAtmosphere& a,
			float r, float mu, float muS, float nu, bool intersects,
			glm::vec3& rayleigh, glm::vec3& mie) {
			const float distance = intersects ? distanceToBottom(r, mu, a) : distanceToTop(r, mu, a);
			const float dx = distance / (float)kSampleCount;

			glm::vec3 rayleighSum(0.0f);
			glm::vec3 mieSum(0.0f);
			for (int i = 0; i <= kSampleCount; ++i) {
				const float d = (float)i * dx;
				const float rD = clampRadius(std::sqrt(std::max(d * d + 2.0f * r * mu * d + r * r, 0.0f)), a);
				const float muSD = clampCosine((r * muS + d * nu) / rD);


				const glm::vec3 transmittance =
				    getTransmittance(table, r, mu, d, intersects, a) *
				    transmittanceToSun(table, rD, muSD, a);

				const float h = rD - a.bottomRadius;
				const float w = (i == 0 || i == kSampleCount) ? 0.5f : 1.0f;
				rayleighSum += transmittance * rayleighDensity(h, a) * w;
				mieSum += transmittance * mieDensity(h, a) * w;
			}

			rayleigh = rayleighSum * dx * a.betaR;
			mie = mieSum * dx * a.betaM;
		}

		// Forward texel-center mapping (inverse of getUnitRangeFromTextureCoord),
		// replicated from atmosphere.glsl getTextureCoordFromUnitRange.
		float getTextureCoordFromUnitRange(float x, int size) {
			return 0.5f / (float)size + x * (1.0f - 1.0f / (float)size);
		}

		// CPU phase functions, replicating atmosphere.glsl (Mie uses the g param).
		float rayleighPhase(float mu) {
			return (3.0f / (16.0f * kPi)) * (1.0f + mu * mu);
		}

		float miePhase(float mu, float g) {
			const float g2 = g * g;
			return (1.0f / (4.0f * kPi)) * (1.0f - g2) / std::pow(1.0f + g2 - 2.0f * g * mu, 1.5f);
		}

		// Forward 4D -> 3D coordinate mapping (Bruneton
		// GetScatteringTextureUvwzFromRMuMuSNu), replicated from atmosphere.glsl.
		// Returns (u_nu, u_mu_s, u_mu, u_r) in texel-center space. The multiple
		// scattering LUT drops the nu axis, so its lookups reuse this and ignore x.
		glm::vec4 getScatteringTextureUvwzFromRMuMuSNu(const BakeAtmosphere& a,
			float r, float mu, float muS, float nu, bool intersects) {
			const float H = std::sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);
			const float rho = std::sqrt(std::max(r * r - a.bottomRadius * a.bottomRadius, 0.0f));
			const float u_r = getTextureCoordFromUnitRange(rho / H, (int)kScatteringRSize);

			const float rMu = r * mu;
			const float discr = rMu * rMu - r * r + a.bottomRadius * a.bottomRadius;
			float u_mu;
			if (intersects) {
				const float d = -rMu - std::sqrt(std::max(discr, 0.0f));
				const float dMin = r - a.bottomRadius;
				const float dMax = rho;
				u_mu = 0.5f - 0.5f * getTextureCoordFromUnitRange(
					dMax == dMin ? 0.0f : (d - dMin) / (dMax - dMin), (int)(kScatteringMuSize / 2));
			} else {
				const float d = -rMu + std::sqrt(std::max(discr + H * H, 0.0f));
				const float dMin = a.topRadius - r;
				const float dMax = rho + H;
				u_mu = 0.5f + 0.5f * getTextureCoordFromUnitRange(
					(d - dMin) / (dMax - dMin), (int)(kScatteringMuSize / 2));
			}

			const float d = distanceToTop(a.bottomRadius, muS, a);
			const float dMin = a.topRadius - a.bottomRadius;
			const float dMax = H;
			const float aVal = (d - dMin) / (dMax - dMin);
			const float D = distanceToTop(a.bottomRadius, kMuSMin, a);
			const float A = (D - dMin) / (dMax - dMin);
			const float u_mu_s = getTextureCoordFromUnitRange(
				std::max(1.0f - aVal / A, 0.0f) / (1.0f + aVal), (int)kScatteringMuSSize);

			return glm::vec4((nu + 1.0f) * 0.5f, u_mu_s, u_mu, u_r);
		}

		// CPU replica of the shader's getSingleScattering: samples the two
		// single-scattering LUTs (Rayleigh + Mie) with linear interpolation across
		// the nu axis and nearest-neighbour in (mu_s, mu, r). Returns both
		// un-phased radiance vectors; Mie stays full-RGB (the shader's packed
		// vec3(single.a) trick would force it gray).
		void sampleSingleScattering(const std::vector<float>& rayleighLUT, const std::vector<float>& mieLUT,
			const BakeAtmosphere& a, float r, float mu, float muS, float nu, bool intersects,
			glm::vec3& rayleigh, glm::vec3& mie) {
			const glm::vec4 uvwz = getScatteringTextureUvwzFromRMuMuSNu(a, r, mu, muS, nu, intersects);
			const float texCoordX = uvwz.x * (float)(kScatteringNuSize - 1);
			const int texX0 = (int)std::floor(texCoordX);
			const float lerp = texCoordX - (float)texX0;
			const int texX1 = std::min(texX0 + 1, (int)(kScatteringNuSize - 1));
			const int texX0c = std::clamp(texX0, 0, (int)(kScatteringNuSize - 1));

			auto texel = [&](const std::vector<float>& lut, int texX) -> glm::vec3 {
				const float u = ((float)texX + uvwz.y) / (float)kScatteringNuSize;
				const int xi = std::clamp((int)std::floor(u * (float)kScatteringWidth), 0, (int)kScatteringWidth - 1);
				const int yi = std::clamp((int)std::floor(uvwz.z * (float)kScatteringHeight), 0, (int)kScatteringHeight - 1);
				const int zi = std::clamp((int)std::floor(uvwz.w * (float)kScatteringDepth), 0, (int)kScatteringDepth - 1);
				const uint32_t i = (((uint32_t)zi * kScatteringHeight + (uint32_t)yi) * kScatteringWidth + (uint32_t)xi) * 3;
				return glm::vec3(lut[i], lut[i + 1], lut[i + 2]);
			};

			rayleigh = glm::mix(texel(rayleighLUT, texX0c), texel(rayleighLUT, texX1), lerp);
			mie      = glm::mix(texel(mieLUT, texX0c), texel(mieLUT, texX1), lerp);
		}

		// 2D irradiance LUT lookup, nearest-neighbour. (r, mu_s) with mu_s linear
		// across x and r as rho/H across y — must match computeIrradianceLUT storage.
		glm::vec3 sampleIrradiance(const std::vector<float>& irr, float r, float muS, const BakeAtmosphere& a) {
			const float H = std::sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);
			const float rho = std::sqrt(std::max(r * r - a.bottomRadius * a.bottomRadius, 0.0f));
			const float u = (clampCosine(muS) + 1.0f) * 0.5f;
			const float v = rho / H;
			const int xi = std::clamp((int)std::floor(u * (float)kIrrWidth), 0, (int)kIrrWidth - 1);
			const int yi = std::clamp((int)std::floor(v * (float)kIrrHeight), 0, (int)kIrrHeight - 1);
			const uint32_t i = ((uint32_t)yi * kIrrWidth + (uint32_t)xi) * 3;
			return glm::vec3(irr[i], irr[i + 1], irr[i + 2]);
		}

		// 3D multiple-scattering LUT lookup, nearest-neighbour. The LUT is laid out
		// (mu_s, mu, r) = (32, 128, 32), no nu axis; the shared forward mapping feeds
		// (u_mu_s, u_mu, u_r) straight into the texture coordinates.
		glm::vec3 sampleMultipleScattering(const std::vector<float>& lut, const BakeAtmosphere& a,
			float r, float mu, float muS, bool intersects) {
			const glm::vec4 uvwz = getScatteringTextureUvwzFromRMuMuSNu(a, r, mu, muS, 0.0f, intersects);
			const int xi = std::clamp((int)std::floor(uvwz.y * (float)kScatteringMuSSize), 0, (int)kScatteringMuSSize - 1);
			const int yi = std::clamp((int)std::floor(uvwz.z * (float)kScatteringMuSize), 0, (int)kScatteringMuSize - 1);
			const int zi = std::clamp((int)std::floor(uvwz.w * (float)kScatteringRSize), 0, (int)kScatteringRSize - 1);
			const uint32_t i = (((uint32_t)zi * kScatteringMuSize + (uint32_t)yi) * kScatteringMuSSize + (uint32_t)xi) * 3;
			return glm::vec3(lut[i], lut[i + 1], lut[i + 2]);
		}

		// Integrates radiance over the local-up hemisphere at (r, mu_s) to get the
		// scalar irradiance E. The single-scattering part samples the single-scattering
		// LUT over (mu, phi) with phase functions applied; the multiple-scattering part
		// is isotropic (no phi dependence), so its azimuthal integral collapses to a
		// factor 2*pi. Returns a kIrrWidth x kIrrHeight x 3 RGB table. E is computed
		// WITHOUT solar irradiance — the render multiplies all terms by u_SunIntensity,
		// so the multiple-scattering source E must stay in the same un-scaled space.
		std::vector<float> computeIrradianceLUT(const BakeAtmosphere& a, float mieG,
			const ScatteringLUTData& single, const std::vector<float>& multiLUT, bool hasMulti) {
			const float H = std::sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);
			std::vector<float> irr(kIrrWidth * kIrrHeight * 3);

			for (uint32_t y = 0; y < kIrrHeight; ++y) {
				const float rho = H * ((float)y + 0.5f) / (float)kIrrHeight;
				const float r = std::sqrt(rho * rho + a.bottomRadius * a.bottomRadius);
				for (uint32_t x = 0; x < kIrrWidth; ++x) {
					const float muS = 2.0f * ((float)x + 0.5f) / (float)kIrrWidth - 1.0f;
					const float sMuS = std::sqrt(std::max(1.0f - muS * muS, 0.0f));

					glm::vec3 eSingle(0.0f);
					for (int i = 0; i < kIrrMuSteps; ++i) {
						const float mu = ((float)i + 0.5f) / (float)kIrrMuSteps;
						const float sMu = std::sqrt(std::max(1.0f - mu * mu, 0.0f));
						for (int j = 0; j < kIrrPhiSteps; ++j) {
							const float phi = 2.0f * kPi * ((float)j + 0.5f) / (float)kIrrPhiSteps;
							const float nu = clampCosine(mu * muS + sMu * sMuS * std::cos(phi));
							glm::vec3 rayleigh, mie;
							sampleSingleScattering(single.rayleigh, single.mie, a, r, mu, muS, nu, false, rayleigh, mie);
							// Both vectors full-RGB; the Mie phase term keeps its
							// per-channel color. No swizzle (.rgb) — the project's
							// GLM build has it disabled.
							const glm::vec3 L = rayleigh * rayleighPhase(nu) + mie * miePhase(nu, mieG);
							eSingle += L * mu;
						}
					}
					eSingle *= (2.0f * kPi / ((float)kIrrMuSteps * (float)kIrrPhiSteps));

					glm::vec3 eMulti(0.0f);
					if (hasMulti) {
						for (int i = 0; i < kIrrMuSteps; ++i) {
							const float mu = ((float)i + 0.5f) / (float)kIrrMuSteps;
							eMulti += sampleMultipleScattering(multiLUT, a, r, mu, muS, false) * mu;
						}
						eMulti *= (2.0f * kPi / (float)kIrrMuSteps);
					}

					const uint32_t i = (y * kIrrWidth + x) * 3;
					const glm::vec3 e = eSingle + eMulti;
					irr[i + 0] = e.r;
					irr[i + 1] = e.g;
					irr[i + 2] = e.b;
				}
			}
			return irr;
		}

		// Multiple-scattering radiance along the view ray (Bruneton
		// ComputeMultipleScattering): each step scatters the isotropic irradiance
		// E(q) into all directions with phase 1/(4*pi), weighted by the total
		// scattering coefficient betaR*rhoR + betaM*rhoM and the view transmittance.
		// No sun transmittance: E already folds it in.
		glm::vec3 computeMultipleScattering(const std::vector<float>& transTable,
			const std::vector<float>& irrLUT, const BakeAtmosphere& a,
			float r, float mu, float muS, bool intersects) {
			const float distance = intersects ? distanceToBottom(r, mu, a) : distanceToTop(r, mu, a);
			const float dx = distance / (float)kSampleCount;

			glm::vec3 sum(0.0f);
			for (int i = 0; i <= kSampleCount; ++i) {
				const float d = (float)i * dx;
				const float rD = clampRadius(std::sqrt(std::max(d * d + 2.0f * r * mu * d + r * r, 0.0f)), a);
				const float h = rD - a.bottomRadius;
				const glm::vec3 T = getTransmittance(transTable, r, mu, d, intersects, a);
				const glm::vec3 E = sampleIrradiance(irrLUT, rD, muS, a);
				const glm::vec3 source = a.betaR * rayleighDensity(h, a) + a.betaM * mieDensity(h, a);
				const float w = (i == 0 || i == kSampleCount) ? 0.5f : 1.0f;
				sum += T * (source * E) * w;
			}
			return sum * (dx / (4.0f * kPi));
		}

		// Full multiple-scattering LUT texel bake. Layout is (mu_s, mu, r) =
		// (32, 128, 32); each texel recovers (r, mu, mu_s, intersects) through the
		// same inverse mapping as the single-scattering LUT.
		std::vector<float> computeMultipleScatteringLUT(const BakeAtmosphere& a,
			const std::vector<float>& transTable, const std::vector<float>& irrLUT) {
			std::vector<float> data(kScatteringMuSSize * kScatteringMuSize * kScatteringRSize * 3);
			for (uint32_t z = 0; z < kScatteringRSize; ++z) {
				for (uint32_t y = 0; y < kScatteringMuSize; ++y) {
					for (uint32_t x = 0; x < kScatteringMuSSize; ++x) {
						const glm::vec4 uvwz(
							0.0f,
							((float)x + 0.5f) / (float)kScatteringMuSSize,
							((float)y + 0.5f) / (float)kScatteringMuSize,
							((float)z + 0.5f) / (float)kScatteringRSize);
						float r, mu, muS, nu;
						bool intersects;
						getRMuMuSNuFromScatteringUvwz(a, uvwz, r, mu, muS, nu, intersects);

						const glm::vec3 m = computeMultipleScattering(transTable, irrLUT, a, r, mu, muS, intersects);
						const uint32_t i = (((uint32_t)z * kScatteringMuSize + (uint32_t)y) * kScatteringMuSSize + (uint32_t)x) * 3;
						data[i + 0] = m.r;
						data[i + 1] = m.g;
						data[i + 2] = m.b;
					}
				}
			}
			return data;
		}

		// Full single-scattering LUT texel bake, shared by bakeScatteringLUT and
		// bakeMultipleScatteringLUT (whose irradiance iteration integrates over it).
		ScatteringLUTData computeSingleScatteringLUT(const BakeAtmosphere& a, const std::vector<float>& transmittanceTable) {
			ScatteringLUTData data;
			data.rayleigh.assign(kScatteringWidth * kScatteringHeight * kScatteringDepth * 3, 0.0f);
			data.mie.assign(kScatteringWidth * kScatteringHeight * kScatteringDepth * 3, 0.0f);
			for (uint32_t z = 0; z < kScatteringDepth; ++z) {
				for (uint32_t y = 0; y < kScatteringHeight; ++y) {
					for (uint32_t x = 0; x < kScatteringWidth; ++x) {
						const uint32_t nuIdx = x / kScatteringMuSSize;
						const uint32_t muSIdx = x % kScatteringMuSSize;

						const glm::vec4 uvwz(
							(float)nuIdx / (float)(kScatteringNuSize - 1),
							((float)muSIdx + 0.5f) / (float)kScatteringMuSSize,
							((float)y + 0.5f) / (float)kScatteringMuSize,
							((float)z + 0.5f) / (float)kScatteringRSize);

						float r, mu, muS, nu;
						bool intersects;
						getRMuMuSNuFromScatteringUvwz(a, uvwz, r, mu, muS, nu, intersects);

						const float nuHalfRange = std::sqrt(std::max((1.0f - mu * mu) * (1.0f - muS * muS), 0.0f));
						nu = std::clamp(nu, mu * muS - nuHalfRange, mu * muS + nuHalfRange);

						glm::vec3 rayleigh, mie;
						computeSingleScattering(transmittanceTable, a, r, mu, muS, nu, intersects, rayleigh, mie);

						const uint32_t idx = ((z * kScatteringHeight + y) * kScatteringWidth + x) * 3;
						data.rayleigh[idx + 0] = rayleigh.r;
						data.rayleigh[idx + 1] = rayleigh.g;
						data.rayleigh[idx + 2] = rayleigh.b;
						data.mie[idx + 0] = mie.r;
						data.mie[idx + 1] = mie.g;
						data.mie[idx + 2] = mie.b;
					}
				}
			}
			return data;
		}
	}

	Ref<Texture2D> AtmosphereBaker::bakeTransmittanceLUT(const AtmosphereParams& p) {
		const BakeAtmosphere a = makeAtmosphere(p);
		const std::vector<float> table = computeTransmittanceTable(a);

		// (w,h,format) factory gives CLAMP_TO_EDGE + LINEAR, which is what a LUT
		// wants; the format-aware setData uploads RGB16F as GL_FLOAT.
		Ref<Texture2D> tex = Texture2D::create(kLUTWidth, kLUTHeight, TextureFormat::RGB16F);
		tex->setData((void*)table.data(), (uint32_t)(table.size() * sizeof(float)));
		return tex;
	}

	AtmosphereBaker::ScatteringLUTPair AtmosphereBaker::bakeScatteringLUT(const AtmosphereParams& p) {
		const BakeAtmosphere a = makeAtmosphere(p);
		// Self-contained: the single-scattering integral samples the transmittance
		// LUT, so re-compute the shared table here (a few ms).
		const std::vector<float> transmittanceTable = computeTransmittanceTable(a);

		// Two RGB16F LUTs instead of one RGBA. Mie is wavelength-independent at
		// its source, but the transmittance it multiplies is not — packing Mie
		// into a single channel (old A) forced the sun glow gray at dusk.
		const ScatteringLUTData data = computeSingleScatteringLUT(a, transmittanceTable);

		ScatteringLUTPair pair;
		pair.rayleighTexture = Texture3D::create(kScatteringWidth, kScatteringHeight, kScatteringDepth,
		                                         TextureFormat::RGB16F, data.rayleigh.data());
		pair.mieTexture = Texture3D::create(kScatteringWidth, kScatteringHeight, kScatteringDepth,
		                                    TextureFormat::RGB16F, data.mie.data());
		return pair;
	}

	Ref<Texture3D> AtmosphereBaker::bakeMultipleScatteringLUT(const AtmosphereParams& p) {
		const BakeAtmosphere a = makeAtmosphere(p);
		const std::vector<float> transmittanceTable = computeTransmittanceTable(a);
		const ScatteringLUTData single = computeSingleScatteringLUT(a, transmittanceTable);

		// Iterate the irradiance <-> multiple-scattering pair: E^0 from single
		// scattering only, then M^i from E^(i-1), then E^i = E_single + E_multi.
		// A few iterations converge because the scattering operator is contractive.
		std::vector<float> irr = computeIrradianceLUT(a, p.miePhaseG, single, {}, false);
		std::vector<float> multi;
		for (int it = 0; it < kMultipleIterations; ++it) {
			multi = computeMultipleScatteringLUT(a, transmittanceTable, irr);
			irr = computeIrradianceLUT(a, p.miePhaseG, single, multi, true);
		}

		// RGB16F: the multiple-scattering source is wavelength-dependent
		// (betaR + betaM), a full vec3 with no packing trick.
		Ref<Texture3D> tex = Texture3D::create(kScatteringMuSSize, kScatteringMuSize, kScatteringRSize,
		                                       TextureFormat::RGB16F, multi.data());
		return tex;
	}

}
