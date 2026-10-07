#include "vepch.h"
#include "WeatherMapBaker.h"
#include "Core/Log.h"

#include <glm.hpp>

namespace ve {

namespace {

	// CPU twin of the integer-lattice hash used by the cloud noise bakes. The seed
	// shifts the lattice so the weather distribution is structurally independent
	// of the shape/detail/warp fields.
	glm::vec3 hash3(glm::vec3 p, float seed) {
		p = glm::fract(p * glm::vec3(0.1031f, 0.1030f, 0.0973f) + seed);
		p += glm::dot(p, glm::vec3(p.y, p.x, p.z) + 33.33f);
		return glm::fract(glm::vec3((p.x + p.y) * p.z, 2.0f * p.x * p.y, (p.x + p.y) * p.x));
	}

	// Seamless periodic gradient Perlin at a 3D lattice point, roughly [-1, 1].
	// Quintic fade (C2-continuous) avoids cubic fade's corner artifacts. Sampled
	// on the unit sphere the field is continuous in longitude by construction, so
	// no periodic wrapping is ever needed.
	float perlin3(glm::vec3 q, float seed) {
		glm::ivec3 base = glm::ivec3(glm::floor(q));
		glm::vec3 f = q - glm::vec3(base);
		glm::vec3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
		float n = 0.0f;
		for (int dx = 0; dx <= 1; dx++)
		for (int dy = 0; dy <= 1; dy++)
		for (int dz = 0; dz <= 1; dz++) {
			glm::ivec3 corner = base + glm::ivec3(dx, dy, dz);
			glm::vec3 g = hash3(glm::vec3(corner), seed) * 2.0f - 1.0f;
			float gl = glm::length(g);
			g = gl > 1e-6f ? g / gl : glm::vec3(1.0f, 0.0f, 0.0f);
			glm::vec3 d = f - glm::vec3((float)dx, (float)dy, (float)dz);
			float w = (dx == 0 ? 1.0f - u.x : u.x)
			        * (dy == 0 ? 1.0f - u.y : u.y)
			        * (dz == 0 ? 1.0f - u.z : u.z);
			n += glm::dot(g, d) * w;
		}
		return n;
	}

	// 2-octave FBM, normalized back to ~[-1, 1]. Two octaves keep the patches at
	// weather-system scale; the shape band supplies the finer detail.
	float perlinFbm(glm::vec3 q, float seed) {
		float total = 0.0f;
		float norm = 0.0f;
		for (int k = 0; k < 2; k++) {
			float amp = (k == 0 ? 1.0f : 0.5f);
			total += perlin3(q, seed) * amp;
			norm += amp;
			q *= 2.0f;
		}
		return total / norm;
	}

} // namespace

Ref<Texture2D> WeatherMapBaker::bake(uint32_t resolution, uint32_t cells, uint32_t seed) {
	return build(resolution, compute(resolution, cells, seed));
}

std::vector<float> WeatherMapBaker::compute(uint32_t resolution, uint32_t cells, uint32_t seed) {
	// Low-frequency Perlin FBM sampled ON THE SPHERE: each texel (u,v) maps to a
	// unit direction, so the field is seamless in longitude (the sphere wraps)
	// and continuous at the poles. `cells` cells around the globe keeps patches at
	// weather-system scale, far below the shape band's frequency. Remapped to
	// [0.3, 1.0] so the global coverage slider still dominates: effective coverage
	// = R * u_CloudCoverage, with clear-sky dips where R drops.
	const float kPi = 3.14159265359f;
	const float kMinR = 0.3f;
	const float kMaxR = 1.0f;

	std::vector<float> data(resolution * resolution);
	for (uint32_t y = 0; y < resolution; y++) {
		float v = (float(y) + 0.5f) / resolution;
		float lat = (v - 0.5f) * kPi;
		float cl = glm::cos(lat);
		float sl = glm::sin(lat);
		for (uint32_t x = 0; x < resolution; x++) {
			float u = (float(x) + 0.5f) / resolution;
			float lon = (u - 0.5f) * 2.0f * kPi;
			glm::vec3 dir = glm::vec3(cl * glm::sin(lon), sl, cl * glm::cos(lon));
			float n = perlinFbm(dir * (float)cells, (float)seed);   // ~[-1, 1]
			data[y * resolution + x] = kMinR + (0.5f * n + 0.5f) * (kMaxR - kMinR);
		}
	}

	VE_CORE_SUCCESS_PRINT("WeatherMapBaker: baked %ux%u Perlin-FBM weather map (%u cells/edge, seed %u, R in [%.2f, %.2f])",
	                      resolution, resolution, cells, seed, kMinR, kMaxR);
	return data;
}

Ref<Texture2D> WeatherMapBaker::build(uint32_t resolution, const std::vector<float>& data) {
	Ref<Texture2D> texture = Texture2D::create(resolution, resolution, TextureFormat::R16F);
	texture->setData((void*)data.data(), (uint32_t)(data.size() * sizeof(float)));
	return texture;
}

}
