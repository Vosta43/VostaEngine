#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"

namespace ve {

	// Bakes periodic Worley F1 distance fields into 3D textures that the
	// volumetric cloud shader samples for its noise bands. One-time startup
	// cost, decoupled from the per-frame pass — same role as IBLBaker, but for
	// cloud noise. All textures use repeat wrap so the period is seamless.
	class VE_API WorleyNoiseBaker {
	public:
		// Single-octave Worley F1 as an R16F 3D texture. Used directly for the
		// cloud detail band (a single sample per step in the shader). Returns
		// nullptr on failure.
		static Ref<Texture3D> bake(uint32_t cellsPerEdge, uint32_t resolution,
		                           uint32_t seed, uint32_t& outCells);
		// Perlin-Worley packed into an RGBA16F 3D texture: channel 0 (R) holds a
		// periodic Perlin billow with `cellsPerEdge` cells per edge; channels
		// 1..3 (G/B/A) hold Worley F1 fields with `cellsPerEdge << k` cells per
		// edge. Used for the cloud shape band — the shader samples ONCE and dots
		// the GBA channels with FBM weights, so a multi-octave shape costs one
		// texture fetch instead of a shader loop. `octaves` is clamped to [1, 4].
		// `outCells` receives cellsPerEdge (the Perlin base's cell count).
		static Ref<Texture3D> bakeMultiOctave(uint32_t cellsPerEdge, uint32_t resolution,
		                                      uint32_t seed, uint32_t octaves,
		                                      uint32_t& outCells);
		// All-Worley detail erosion field: `octaves` Worley F1 octaves (clamped to
		// [1,4]) packed into the R/G/B channels of an RGBA16F 3D texture (A unused),
		// at cellsPerEdge << k cells/edge. Unlike bakeMultiOctave there is NO Perlin
		// base — applyDetail dots .rgb with Hillaire weights, and a smooth Perlin R
		// channel would soften the erosion into smooth edges instead of the cellular
		// cauliflower breakup that reads as fragmented clouds.
		static Ref<Texture3D> bakeDetailWorley(uint32_t cellsPerEdge, uint32_t resolution,
		                                       uint32_t seed, uint32_t octaves,
		                                       uint32_t& outCells);
		// Two independent 4-octave Worley FBM fields, pre-summed on the CPU into
		// an RG16F 3D texture (R = field A, G = field B, different hash seeds).
		// Used for the cloud domain warp: the shader fetches both warp fields in
		// one sample and applies (warp - 0.5) as a position displacement. Moving
		// the octave loop into the bake is what makes the shader-side warp cheap.
		static Ref<Texture3D> bakeWarp(uint32_t cellsPerEdge, uint32_t resolution,
		                               uint32_t seed, uint32_t& outCells);
	};

}
