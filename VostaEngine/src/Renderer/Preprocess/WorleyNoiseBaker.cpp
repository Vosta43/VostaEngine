#include "vepch.h"
#include "WorleyNoiseBaker.h"
#include "Core/Log.h"

#include <glm.hpp>
#include <algorithm>

namespace ve {

namespace {

	int wrapCell(int v, int cells) {
		v %= cells;
		return v < 0 ? v + cells : v;
	}

	// CPU twin of the integer-lattice hash. Deterministic; it only shapes the
	// bake, so the shader no longer pays for it inside the raymarch. `seed`
	// shifts the feature-point positions so different bands get distinct grids.
	glm::vec3 hash3(glm::vec3 p, float seed) {
		p = glm::fract(p * glm::vec3(0.1031f, 0.1030f, 0.0973f) + seed);
		p += glm::dot(p, glm::vec3(p.y, p.x, p.z) + 33.33f);
		return glm::fract(glm::vec3((p.x + p.y) * p.z, 2.0f * p.x * p.y, (p.x + p.y) * p.x));
	}

	// Adds one octave of seamless-periodic Worley F1, scaled by `weight`, into
	// `out` at channel `channel` of a `numChannels`-wide interleaved layout.
	// `cells` is the octave's cells per tile edge (base << octave). Feature point
	// position uses the UNWRAPPED cell index so distances stay continuous across
	// the tile seam; only the hash seed wraps mod cells, so the field repeats with
	// period `cells`:
	//     value(x) == value(x + cells)
	// Shared by all three bakes: single-octave channels (weight 1.0 into one
	// channel), multi-octave shape (each octave into its own channel), and the
	// pre-summed warp FBM (all octaves accumulated into one channel).
	void addWorleyOctave(std::vector<float>& out, uint32_t numChannels, uint32_t channel,
	                     uint32_t cells, uint32_t resolution, uint32_t seed, float weight) {
		const float cf = (float)cells;
		const float texelsPerCell = float(resolution) / cf;
		size_t idx = 0;
		for (uint32_t z = 0; z < resolution; z++)
		for (uint32_t y = 0; y < resolution; y++)
		for (uint32_t x = 0; x < resolution; x++) {
			glm::vec3 c = glm::vec3(x, y, z) / texelsPerCell;   // fractional cell coords in [0, cells)
			glm::ivec3 base = glm::ivec3(glm::floor(c));
			float d = 2.0f;                                     // F1 <= sqrt(3) < 2
			for (int dx = -1; dx <= 1; dx++)
			for (int dy = -1; dy <= 1; dy++)
			for (int dz = -1; dz <= 1; dz++) {
				glm::ivec3 n = base + glm::ivec3(dx, dy, dz);
				glm::vec3 fp = glm::vec3(n) + hash3(glm::vec3(
					(float)wrapCell(n.x, (int)cells),
					(float)wrapCell(n.y, (int)cells),
					(float)wrapCell(n.z, (int)cells)), (float)seed);
				d = std::min(d, glm::length(c - fp));
			}
			out[idx * numChannels + channel] += weight * d;
			idx++;
		}
	}

	// Adds one octave of seamless-periodic gradient Perlin noise into `out` at
	// `channel`. Unlike the Worley octaves (distance to feature points), Perlin
	// is a smooth billowy field on the same cell lattice. Used for the R channel
	// of the shape texture: the base billow is Perlin, the finer erosion (GBA)
	// stays Worley — the classic Perlin-Worley split. Stored as 1 - n so the
	// shader's `1 - channel` density read stays valid (low stored = high density,
	// matching the Worley channels' convention).
	void addPerlinOctave(std::vector<float>& out, uint32_t numChannels, uint32_t channel,
	                     uint32_t cells, uint32_t resolution, uint32_t seed, float weight) {
		const float cf = (float)cells;
		const float texelsPerCell = float(resolution) / cf;
		size_t idx = 0;
		for (uint32_t z = 0; z < resolution; z++)
		for (uint32_t y = 0; y < resolution; y++)
		for (uint32_t x = 0; x < resolution; x++) {
			glm::vec3 c = glm::vec3(x, y, z) / texelsPerCell;   // fractional cell coords in [0, cells)
			glm::ivec3 base = glm::ivec3(glm::floor(c));
			glm::vec3 f = c - glm::vec3(base);                  // [0,1) within the cell
			// Quintic fade: C2-continuous, avoids cubic fade's corner artifacts.
			glm::vec3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
			float n = 0.0f;
			for (int dx = 0; dx <= 1; dx++)
			for (int dy = 0; dy <= 1; dy++)
			for (int dz = 0; dz <= 1; dz++) {
				glm::ivec3 corner = base + glm::ivec3(dx, dy, dz);
				glm::vec3 g = hash3(glm::vec3(
					(float)wrapCell(corner.x, (int)cells),
					(float)wrapCell(corner.y, (int)cells),
					(float)wrapCell(corner.z, (int)cells)), (float)seed) * 2.0f - 1.0f;
				float gl = glm::length(g);
				g = gl > 1e-6f ? g / gl : glm::vec3(1.0f, 0.0f, 0.0f);
				glm::vec3 d = f - glm::vec3((float)dx, (float)dy, (float)dz);
				float w = (dx == 0 ? 1.0f - u.x : u.x)
				        * (dy == 0 ? 1.0f - u.y : u.y)
				        * (dz == 0 ? 1.0f - u.z : u.z);
				n += glm::dot(g, d) * w;
			}
			float perlin = 0.5f + 0.5f * n;                     // [-1,1] -> [0,1], high = dense
			out[idx * numChannels + channel] += weight * (1.0f - perlin);   // inverted convention
			idx++;
		}
	}

} // namespace

Ref<Texture3D> WorleyNoiseBaker::bake(uint32_t cellsPerEdge, uint32_t resolution,
                                       uint32_t seed, uint32_t& outCells) {
	std::vector<float> data(resolution * resolution * resolution, 0.0f);
	addWorleyOctave(data, 1, 0, cellsPerEdge, resolution, seed, 1.0f);

	Ref<Texture3D> texture = Texture3D::create(resolution, resolution, resolution, data.data());
	if (!texture) {
		VE_CORE_ERROR_PRINT("%s", "WorleyNoiseBaker: failed to create 3D noise texture");
		return nullptr;
	}

	outCells = cellsPerEdge;
	VE_CORE_SUCCESS_PRINT("WorleyNoiseBaker: baked %ux%ux%u Worley noise (%u cells/edge, seed %u)",
	                      resolution, resolution, resolution, cellsPerEdge, seed);
	return texture;
}

Ref<Texture3D> WorleyNoiseBaker::bakeMultiOctave(uint32_t cellsPerEdge, uint32_t resolution,
                                                  uint32_t seed, uint32_t octaves,
                                                  uint32_t& outCells) {
	const uint32_t n = std::min(octaves, 4u);
	std::vector<float> data(resolution * resolution * resolution * 4, 0.0f);
	// R = base billow (Perlin, one octave at cellsPerEdge); G/B/A = finer
	// erosion octaves (Worley F1 at cellsPerEdge << k). The Perlin-Worley split
	// gives smooth connected masses for the base shape with cellular detail on
	// top — classic Hillaire. The shader's 1 - channel density convention holds
	// because addPerlinOctave already stores 1 - perlin.
	addPerlinOctave(data, 4, 0, cellsPerEdge, resolution, seed, 1.0f);
	for (uint32_t k = 1; k < n; k++)
		addWorleyOctave(data, 4, k, cellsPerEdge << k, resolution, seed, 1.0f);

	Ref<Texture3D> texture = Texture3D::create(resolution, resolution, resolution,
	                                           TextureFormat::RGBA16F, data.data(), true);
	if (!texture) {
		VE_CORE_ERROR_PRINT("%s", "WorleyNoiseBaker: failed to create multi-octave 3D noise texture");
		return nullptr;
	}

	outCells = cellsPerEdge;
	VE_CORE_SUCCESS_PRINT("WorleyNoiseBaker: baked %ux%ux%u Perlin-Worley shape (%u base cells/edge, %u octaves, seed %u)",
	                      resolution, resolution, resolution, cellsPerEdge, n, seed);
	return texture;
}

Ref<Texture3D> WorleyNoiseBaker::bakeDetailWorley(uint32_t cellsPerEdge, uint32_t resolution,
                                                  uint32_t seed, uint32_t octaves,
                                                  uint32_t& outCells) {
	const uint32_t n = std::min(octaves, 4u);
	std::vector<float> data(resolution * resolution * resolution * 4, 0.0f);
	// All-Worley erosion band: every channel is Worley F1 at (base << k) cells/edge.
	// Deliberately NO Perlin base — cellular Worley detail is what carves the
	// cauliflower fragmentation into cloud edges; a Perlin R channel (as in
	// bakeMultiOctave) would smooth the erosion out. The shader dots .rgb with
	// Hillaire weights (0.625, 0.25, 0.125); A stays unused.
	for (uint32_t k = 0; k < n; k++)
		addWorleyOctave(data, 4, k, cellsPerEdge << k, resolution, seed, 1.0f);

	Ref<Texture3D> texture = Texture3D::create(resolution, resolution, resolution,
	                                           TextureFormat::RGBA16F, data.data(), true);
	if (!texture) {
		VE_CORE_ERROR_PRINT("%s", "WorleyNoiseBaker: failed to create detail Worley 3D noise texture");
		return nullptr;
	}

	outCells = cellsPerEdge;
	VE_CORE_SUCCESS_PRINT("WorleyNoiseBaker: baked %ux%ux%u all-Worley detail (%u base cells/edge, %u octaves, seed %u)",
	                      resolution, resolution, resolution, cellsPerEdge, n, seed);
	return texture;
}

Ref<Texture3D> WorleyNoiseBaker::bakeWarp(uint32_t cellsPerEdge, uint32_t resolution,
                                           uint32_t seed, uint32_t& outCells) {
	// Two fields share the octave grids but use different hash seeds, so field B
	// is a structurally independent FBM instead of a copy of field A. Weights
	// mirror the shader's old 4-octave FBM (0.5, 0.25, 0.125, 0.0625) so the
	// warp's look is preserved now that the loop is gone.
	const uint32_t kOctaves = 4;
	std::vector<float> data(resolution * resolution * resolution * 2, 0.0f);
	float weight = 0.5f;
	for (uint32_t k = 0; k < kOctaves; k++) {
		addWorleyOctave(data, 2, 0, cellsPerEdge << k, resolution, seed, weight);
		addWorleyOctave(data, 2, 1, cellsPerEdge << k, resolution, seed + 10000u, weight);
		weight *= 0.5f;
	}

	Ref<Texture3D> texture = Texture3D::create(resolution, resolution, resolution,
	                                           TextureFormat::RG16F, data.data(), true);
	if (!texture) {
		VE_CORE_ERROR_PRINT("%s", "WorleyNoiseBaker: failed to create warp 3D noise texture");
		return nullptr;
	}

	outCells = cellsPerEdge;
	VE_CORE_SUCCESS_PRINT("WorleyNoiseBaker: baked %ux%ux%u warp FBM (%u cells/edge, seed %u)",
	                      resolution, resolution, resolution, cellsPerEdge, seed);
	return texture;
}

}
