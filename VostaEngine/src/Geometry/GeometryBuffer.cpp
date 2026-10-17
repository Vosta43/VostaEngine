#include "vepch.h"
#include "Geometry/GeometryBuffer.h"

#include <glm.hpp>
#include <algorithm>
#include <cmath>

namespace ve {

	namespace {
		constexpr float kPi = 3.14159265358979323846f;
	}

	void GeometryBuffer::computeSmoothNormals() {
		normals.assign(positions.size(), glm::vec3(0.0f));
		if (indices.size() < 3) return;

		for (size_t i = 0; i + 2 < indices.size(); i += 3) {
			const uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
			if (a >= positions.size() || b >= positions.size() || c >= positions.size()) continue;

			// The unnormalized cross product's length is twice the triangle area, so
			// accumulating it weights each face by area for free.
			const glm::vec3 n = glm::cross(positions[b] - positions[a], positions[c] - positions[a]);
			normals[a] += n;
			normals[b] += n;
			normals[c] += n;
		}

		for (auto& n : normals) {
			const float len = glm::length(n);
			n = len > 1e-8f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}

	void GeometryBuffer::computeTangents() {
		tangents.assign(positions.size(), glm::vec3(0.0f));

		if (!hasUVs() || indices.size() < 3) {
			for (auto& t : tangents) t = glm::vec3(1.0f, 0.0f, 0.0f);
			return;
		}

		for (size_t i = 0; i + 2 < indices.size(); i += 3) {
			const uint32_t i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
			if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) continue;

			const glm::vec3 e1 = positions[i1] - positions[i0];
			const glm::vec3 e2 = positions[i2] - positions[i0];

			const glm::vec2& uv0 = uvs[i0];
			const glm::vec2& uv1 = uvs[i1];
			const glm::vec2& uv2 = uvs[i2];
			const float du1 = uv1.x - uv0.x, du2 = uv2.x - uv0.x;
			const float dv1 = uv1.y - uv0.y, dv2 = uv2.y - uv0.y;

			const float r = du1 * dv2 - du2 * dv1;
			if (std::abs(r) < 1e-8f) continue;

			const glm::vec3 t = (e1 * dv2 - e2 * dv1) / r;
			tangents[i0] += t;
			tangents[i1] += t;
			tangents[i2] += t;
		}

		for (auto& t : tangents) {
			const float len = glm::length(t);
			t = len > 1e-8f ? t / len : glm::vec3(1.0f, 0.0f, 0.0f);
		}
	}

	void GeometryBuffer::computeBounds(glm::vec3& outMin, glm::vec3& outMax) const {
		if (positions.empty()) {
			outMin = outMax = glm::vec3(0.0f);
			return;
		}
		outMin = outMax = positions[0];
		for (const auto& p : positions) {
			outMin = glm::min(outMin, p);
			outMax = glm::max(outMax, p);
		}
	}

	void GeometryBuffer::generateSphericalUVs() {
		uvs.resize(positions.size());
		for (size_t i = 0; i < positions.size(); ++i) {
			const float len = glm::length(positions[i]);
			if (len < 1e-8f) {
				uvs[i] = glm::vec2(0.5f, 0.5f);
				continue;
			}
			const glm::vec3 d = positions[i] / len;
			uvs[i] = glm::vec2(
				std::atan2(d.z, d.x) / (2.0f * kPi) + 0.5f,
				std::asin(std::clamp(d.y, -1.0f, 1.0f)) / kPi + 0.5f);
		}
	}

}
