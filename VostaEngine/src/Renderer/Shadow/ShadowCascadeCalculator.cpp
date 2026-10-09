#include "vepch.h"
#include "ShadowCascadeCalculator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <gtc/matrix_transform.hpp>   // glm::lookAt, glm::ortho

namespace ve {

	namespace {
		// How far above a receiver a caster may sit along the sun. Casters share their
		// receiver's lateral coords, so this only spends depth range; raise it if tall
		// geometry's shadow gets clipped near the top.
		constexpr float kShadowDepthReach = 250.0f;
		constexpr float kHorizonSinFloor  = 1e-3f;
	}

	ShadowCascades ShadowCascadeCalculator::compute(const glm::mat4& view, const glm::mat4& projection,
	                                                const glm::vec3& camPos, const glm::vec3& sunDir,
	                                                float shadowDistance, const int mapRes[kCascades], int count) {
		ShadowCascades r;

		// Camera projection parameters (glm column-major: entry [col][row]).
		float nearP      = projection[3][2] / (projection[2][2] - 1.0f);
		float tanHalfFov = 1.0f / projection[1][1];
		float aspect     = projection[1][1] / projection[0][0];

		// Light basis: rays travel along +sunDir; the shadow camera looks along -sunDir.
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		glm::vec3 refEye = camPos + sunDir;
		glm::mat3 lightAxes = glm::transpose(glm::mat3(glm::lookAt(refEye, camPos, up)));
		glm::vec3 R = lightAxes[0];             // light-space X (world)
		glm::vec3 U = lightAxes[1];             // light-space Y (world)
		glm::vec3 S = glm::normalize(sunDir);   // light-space Z, positive toward sun

		glm::mat4 invView = glm::inverse(view);

		// Practical split scheme: a 50/50 blend of uniform and logarithmic, run from the
		// near plane out to the shadow distance.
		float splits[kCascades];
		for (int i = 1; i <= kCascades; ++i) {
			float t = (float)i / (float)kCascades;
			splits[i - 1] = 0.5f * (nearP + (shadowDistance - nearP) * t)
			              + 0.5f * nearP * std::pow(shadowDistance / nearP, t);
		}

		const float backReach = kShadowDepthReach / std::max(S.y, kHorizonSinFloor);

		float nearSlice = nearP;
		for (int c = 0; c < count; ++c) {
			float farSlice = splits[c];

			// 8 corners of the frustum slice [nearSlice, farSlice], in world space.
			glm::vec3 corners[8];
			int idx = 0;
			for (int sx = -1; sx <= 1; sx += 2) {
				for (int sy = -1; sy <= 1; sy += 2) {
					for (float d : { nearSlice, farSlice }) {
						float hh = tanHalfFov * d;
						float hw = hh * aspect;
						glm::vec4 v = invView * glm::vec4(sx * hw, sy * hh, -d, 1.0f);
						corners[idx++] = glm::vec3(v) / v.w;
					}
				}
			}

			int res = mapRes[c];

			// Tight light-space AABB of the slice.
			const float kBig = std::numeric_limits<float>::max();
			glm::vec3 lmin(kBig);
			glm::vec3 lmax(-kBig);
			for (auto& p : corners) {
				glm::vec3 l(glm::dot(p, R), glm::dot(p, U), glm::dot(p, S));
				lmin = glm::min(lmin, l);
				lmax = glm::max(lmax, l);
			}

			// Snap grid: the slice sphere's texel — the radius is what stays fixed as
			// the view rotates.
			glm::vec3 centroid(0.0f);
			for (auto& p : corners) centroid += p;
			centroid /= 8.0f;
			float radius = 0.0f;
			for (auto& p : corners) radius = std::max(radius, glm::length(p - centroid));
			float grid = std::max((2.0f * radius) / (float)res, 1e-3f);

			// Square half-extent: the larger AABB side, up to the grid plus half a grid of slack.
			float rawHalf = 0.5f * std::max(lmax.x - lmin.x, lmax.y - lmin.y);
			float half = std::ceil(rawHalf / grid + 0.5f) * grid;

			// Centre on the box's own texel grid: one texel of camera motion, one texel of map shift.
			float texelWorld = (2.0f * half) / (float)res;
			float cx = glm::round(0.5f * (lmin.x + lmax.x) / texelWorld) * texelWorld;
			float cy = glm::round(0.5f * (lmin.y + lmax.y) / texelWorld) * texelWorld;
			// Depth keeps the sphere's range: it sets precision, not the map's resolution.
			float cz = glm::dot(centroid, S);
			float zMin = cz - radius;
			float zMax = cz + radius;

			r.texelWorld[c] = texelWorld;
			r.splitFar[c]   = farSlice;

			glm::vec3 lateral = R * cx + U * cy;
			glm::vec3 eye  = lateral + S * (zMax + backReach);
			glm::vec3 look = lateral + S * (zMin - 8.0f);
			float farD = (zMax + backReach) - (zMin - 8.0f);
			glm::mat4 sunView = glm::lookAt(eye, look, up);
			glm::mat4 sunProj = glm::ortho(-half, half, -half, half, 1.0f, farD);
			r.lightVP[c] = sunProj * sunView;

			nearSlice = farSlice;
		}

		r.count = count;
		return r;
	}

}
