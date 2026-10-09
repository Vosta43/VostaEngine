#pragma once

#include "Scene/Archive.h"

#include <cstdint>
#include <string>

namespace ve {

	// Backend-agnostic description of a noise field. This is the shared vocabulary
	// every consumer (terrain, PCG, materials, fog) edits and stores; the actual
	// evaluation lives in NoiseCPU / NoiseGLSL, never here.
	//
	// Deliberately free of any third-party types: the FastNoiseLite backend is
	// hidden in NoiseCPU.cpp. The enum order below mirrors FastNoiseLite's, so the
	// backend mapping stays a plain cast, and keeps the door open for a GLSL port
	// using the same ordering.
	//
	// Defaults match FastNoiseLite's constructor, so a default-constructed
	// NoiseSettings behaves like a default FastNoiseLite object.
	struct NoiseSettings {

		// Which base noise the field is built from.
		enum class Type {
			OpenSimplex2,
			OpenSimplex2S,
			Cellular,
			Perlin,
			ValueCubic,
			Value
		};

		// How octaves are combined. None = single octave.
		enum class Fractal {
			None,
			Fbm,
			Ridged,
			PingPong
		};

		// Cellular (Worley) only.
		enum class CellularDistance {
			Euclidean,
			EuclideanSq,
			Manhattan,
			Hybrid
		};

		// Cellular (Worley) only.
		enum class CellularReturn {
			CellValue,
			Distance,
			Distance2,
			Distance2Add,
			Distance2Sub,
			Distance2Mul,
			Distance2Div
		};

		// Domain warp applied to the input coordinates before sampling.
		// None disables warping entirely.
		enum class DomainWarp {
			None,
			OpenSimplex2,
			OpenSimplex2Reduced,
			BasicGrid
		};

		Type  type = Type::OpenSimplex2;
		int   seed = 1337;
		float frequency = 0.01f;

		Fractal fractal = Fractal::None;
		int     octaves = 3;
		float   lacunarity = 2.0f;
		float   gain = 0.5f;
		// Octave shaping. weightedStrength applies to Fbm/Ridged; pingPongStrength
		// only to PingPong.
		float   weightedStrength = 0.0f;
		float   pingPongStrength = 2.0f;

		CellularDistance cellularDistance = CellularDistance::EuclideanSq;
		CellularReturn   cellularReturn = CellularReturn::Distance;
		float            cellularJitter = 1.0f;

		DomainWarp domainWarp = DomainWarp::None;
		float      domainWarpAmp = 1.0f;

		// Remap of the backend's native [-1, 1] output. The defaults leave it
		// untouched; consumers that want 0..1 set outputMin = 0, outputMax = 1.
		float outputMin = -1.0f;
		float outputMax = 1.0f;

		// No type tag here: NoiseSettings is embedded as a field by whatever stores
		// it (a noise asset, a terrain component), and that container owns identity.
		void serialize(Archive& ar) const {
			ar << static_cast<int32_t>(type);
			ar << seed;
			ar << frequency;

			ar << static_cast<int32_t>(fractal);
			ar << octaves;
			ar << lacunarity;
			ar << gain;
			ar << weightedStrength;
			ar << pingPongStrength;

			ar << static_cast<int32_t>(cellularDistance);
			ar << static_cast<int32_t>(cellularReturn);
			ar << cellularJitter;

			ar << static_cast<int32_t>(domainWarp);
			ar << domainWarpAmp;

			ar << outputMin;
			ar << outputMax;
		}

		void deserialize(Archive& ar) {
			int32_t e = 0;

			ar >> e; type = static_cast<Type>(e);
			ar >> seed;
			ar >> frequency;

			ar >> e; fractal = static_cast<Fractal>(e);
			ar >> octaves;
			ar >> lacunarity;
			ar >> gain;
			ar >> weightedStrength;
			ar >> pingPongStrength;

			ar >> e; cellularDistance = static_cast<CellularDistance>(e);
			ar >> e; cellularReturn = static_cast<CellularReturn>(e);
			ar >> cellularJitter;

			ar >> e; domainWarp = static_cast<DomainWarp>(e);
			ar >> domainWarpAmp;

			ar >> outputMin;
			ar >> outputMax;
		}
	};

}
