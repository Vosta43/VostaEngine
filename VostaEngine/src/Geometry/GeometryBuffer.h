#pragma once

#include "Core/Core.h"

#include <vector>
#include <cstdint>
#include <glm.hpp>

namespace ve {

	// A CPU-side triangle mesh that flows between geometry-graph nodes. Struct of
	// arrays because subdivision and displacement rebuild topology and scan vertices
	// and faces in bulk. Positions and indices are required; normals/uvs/tangents are
	// optional and filled in as the graph runs (MeshOutput guarantees them at the end).
	struct VE_API GeometryBuffer {
		std::vector<glm::vec3> positions;
		std::vector<uint32_t>  indices;    // triangle list

		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;
		std::vector<glm::vec3> tangents;

		bool hasNormals()  const { return normals.size()  == positions.size(); }
		bool hasUVs()      const { return uvs.size()      == positions.size(); }
		bool hasTangents() const { return tangents.size() == positions.size(); }

		bool empty() const { return positions.empty() || indices.empty(); }

		// Area-weighted vertex normals accumulated from face normals. Vertices are
		// assumed welded, so the result is smooth across the whole surface.
		void computeSmoothNormals();

		// UV-gradient tangents, averaged per vertex. Degenerate UVs fall back to +X.
		void computeTangents();

		void computeBounds(glm::vec3& outMin, glm::vec3& outMax) const;

		// Spherical UVs from unit-direction positions. Seam at +-pi on the X/Z ring.
		void generateSphericalUVs();
	};

}
