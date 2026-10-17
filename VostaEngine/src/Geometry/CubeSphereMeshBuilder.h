#pragma once

#include "Core/Core.h"
#include "Asset/StaticMeshResource.h"

#include <glm.hpp>

namespace ve {

	// One cube face's orthonormal basis on the unit cube. cross(axisU, axisV) ==
	// normal, so a grid wound (P00, P10, P01) faces outward.
	struct CubeFaceBasis {
		glm::vec3 normal;
		glm::vec3 axisU;
		glm::vec3 axisV;
	};

	// The six faces (index 0..5), in a fixed order.
	const CubeFaceBasis* cubeFaceBasis();

	// Cube-surface point -> unit sphere direction (Nowell's quadratic warp). Double
	// throughout: the caller scales by a planet radius, and the tile-local
	// subtraction that keeps vertex magnitudes small happens after that scaling.
	glm::dvec3 cubeToSphere(const glm::dvec3& cubePoint);

	// A node of the cube-sphere quad-tree: face `face`, at `level`, addressed by
	// (x, y) in [0, 2^level). The face index is what turns the single flat-chart
	// root into six roots -- the shape the real terrain streamer would take.
	struct TileAddress {
		int face  = 0;
		int level = 0;
		int x     = 0;
		int y     = 0;
	};

	// Builds one tile as a tile-local mesh. Vertices are offsets from the tile's
	// centre, so their magnitude is the tile size rather than the planet radius --
	// the same reason camera-relative rendering exists. The caller places the tile
	// with a pure translation. outCenterLocal receives the tile centre in
	// planet-local space (|centre| == radius).
	class CubeSphereMeshBuilder {
	public:
		static Ref<StaticMeshResource> buildTile(const TileAddress& tile, double radius,
		                                         int tileRes, glm::dvec3& outCenterLocal);
	};

}
