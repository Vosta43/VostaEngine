#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/Atmosphere.h"

#include <vector>

namespace ve {

	// Raw CPU-side single-scattering LUT data, kept as two RGB16F buffers so
	// the Mie channel keeps its full per-channel color (see ScatteringLUTPair).
	struct ScatteringLUTData {
		std::vector<float> rayleigh;
		std::vector<float> mie;
	};

	// Bakes the Bruneton-Neyret atmospheric LUTs on the CPU. One-time startup
	// cost, like IBLBaker.
	class VE_API AtmosphereBaker {
	public:
		// Returns the transmittance LUT (256x64 RGB16F) for the given atmosphere
		// parameters. Samples a (r, mu) grid, integrating optical depth to the top
		// atmosphere boundary along each ray; the HDR pass then replaces its
		// analytic 8-step sunTransmittance with a single lookup. Transmittance is
		// stored per channel at 680/550/440 nm (RGB), matching the engine's
		// rayleighScattering ordering. Returns nullptr on failure.
		static Ref<Texture2D> bakeTransmittanceLUT(const AtmosphereParams& params);

		// Holds the two single-scattering 3D LUTs over the shared 4D
		// (r, mu, mu_s, nu) domain, split into separate Rayleigh and Mie RGB16F
		// textures. Mie keeps its own texture because the transmittance it
		// multiplies is strongly wavelength-dependent at dusk; packing Mie into a
		// single channel (as one RGBA texture would force) turns it gray and
		// washes out the sun disk's reddening near the horizon.
		struct ScatteringLUTPair {
			Ref<Texture3D> rayleighTexture;   // 256x128x32 RGB16F
			Ref<Texture3D> mieTexture;        // 256x128x32 RGB16F
		};

		// Returns the single-scattering LUT pair for the given atmosphere
		// parameters. The 4D (r, mu, mu_s, nu) domain is packed into a 3D texture
		// (nu * mu_s along x). Each texel stores the un-phased single-scattered
		// radiance integral along the view ray
		// (integral of T(p->q) * beta * rho(q) * T_sun(q) ds), RGB per channel for
		// both Rayleigh and Mie. Phase functions and solar irradiance are applied
		// at render time.
		static ScatteringLUTPair bakeScatteringLUT(const AtmosphereParams& params);

		// Returns the multiple-scattering 3D LUT (32x128x32 RGB16F, (mu_s, mu, r))
		// for the given atmosphere parameters. Bruneton-Neyret isotropic
		// approximation: the single-scattering LUT is integrated over the local-up
		// hemisphere into a 2D irradiance LUT (256x64, r x mu_s), then the
		// multiple-scattered radiance is a view-ray integral of
		// (betaR*rhoR + betaM*rhoM) * E(q, mu_s) / 4pi, iterated to converge
		// irradiance <-> scattering. Stored un-phased (isotropic phase 1/4pi already
		// folded in); the HDR pass adds it to the single-scattering result without a
		// phase function, scaled by AtmosphereParams::multipleScattering. Returns
		// nullptr on failure.
		static Ref<Texture3D> bakeMultipleScatteringLUT(const AtmosphereParams& params);

		static ScatteringLUTData computeScatteringData(const AtmosphereParams& params);
		static ScatteringLUTPair buildScatteringTextures(const ScatteringLUTData& data);
		static std::vector<float> computeMultipleScatteringData(const AtmosphereParams& params);
		static Ref<Texture3D> buildMultipleScatteringTexture(const std::vector<float>& data);
	};

}
