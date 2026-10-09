#include "vepch.h"
#include "NoiseCPU.h"

// The only place FastNoiseLite is pulled in. Keeping it here (not in the header)
// stops the dependency leaking into the rest of the engine and confines the
// ~2600-line header to a single translation unit.
#include "FastNoiseLite.h"

namespace ve {

	namespace {

		// Our enums are ordered to match FastNoiseLite's, so the mapping is a
		// plain cast. DomainWarp is the exception: it carries a leading None that
		// the backend has no equivalent for.
		void apply(const NoiseSettings& s, FastNoiseLite& fnl) {
			fnl.SetSeed(s.seed);
			fnl.SetFrequency(s.frequency);
			fnl.SetNoiseType(static_cast<FastNoiseLite::NoiseType>(s.type));
			fnl.SetFractalType(static_cast<FastNoiseLite::FractalType>(s.fractal));
			fnl.SetFractalOctaves(s.octaves);
			fnl.SetFractalLacunarity(s.lacunarity);
			fnl.SetFractalGain(s.gain);
			fnl.SetFractalWeightedStrength(s.weightedStrength);
			fnl.SetFractalPingPongStrength(s.pingPongStrength);
			fnl.SetCellularDistanceFunction(
				static_cast<FastNoiseLite::CellularDistanceFunction>(s.cellularDistance));
			fnl.SetCellularReturnType(
				static_cast<FastNoiseLite::CellularReturnType>(s.cellularReturn));
			fnl.SetCellularJitter(s.cellularJitter);
		}

		void applyWarp(const NoiseSettings& s, FastNoiseLite& fnl) {
			fnl.SetDomainWarpType(static_cast<FastNoiseLite::DomainWarpType>(
				static_cast<int>(s.domainWarp) - 1));
			fnl.SetDomainWarpAmp(s.domainWarpAmp);
		}

		inline float remap(const NoiseSettings& s, float v) {
			const float t = v * 0.5f + 0.5f;
			return s.outputMin + t * (s.outputMax - s.outputMin);
		}

	}

	float NoiseCPU::sample(const NoiseSettings& settings, float x, float y) {
		FastNoiseLite fnl;
		apply(settings, fnl);

		if (settings.domainWarp != NoiseSettings::DomainWarp::None) {
			applyWarp(settings, fnl);
			fnl.DomainWarp(x, y);
		}

		return remap(settings, fnl.GetNoise(x, y));
	}

	float NoiseCPU::sample(const NoiseSettings& settings, float x, float y, float z) {
		FastNoiseLite fnl;
		apply(settings, fnl);

		if (settings.domainWarp != NoiseSettings::DomainWarp::None) {
			applyWarp(settings, fnl);
			fnl.DomainWarp(x, y, z);
		}

		return remap(settings, fnl.GetNoise(x, y, z));
	}

}
