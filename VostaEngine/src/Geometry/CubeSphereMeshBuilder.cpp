#include "vepch.h"
#include "CubeSphereMeshBuilder.h"

#include <cmath>

namespace ve {

	namespace {

		const CubeFaceBasis kFaces[6] = {
			{ { 1, 0, 0}, {0, 1, 0}, {0, 0, 1} }, // +X: cross(+Y,+Z)=+X
			{ {-1, 0, 0}, {0, 0, 1}, {0, 1, 0} }, // -X: cross(+Z,+Y)=-X
			{ { 0, 1, 0}, {0, 0, 1}, {1, 0, 0} }, // +Y: cross(+Z,+X)=+Y
			{ { 0,-1, 0}, {1, 0, 0}, {0, 0, 1} }, // -Y: cross(+X,+Z)=-Y
			{ { 0, 0, 1}, {1, 0, 0}, {0, 1, 0} }, // +Z: cross(+X,+Y)=+Z
			{ { 0, 0,-1}, {0, 1, 0}, {1, 0, 0} }, // -Z: cross(+Y,+X)=-Z
		};

	}

	const CubeFaceBasis* cubeFaceBasis() { return kFaces; }

	glm::dvec3 cubeToSphere(const glm::dvec3& p) {
		const double x2 = p.x * p.x, y2 = p.y * p.y, z2 = p.z * p.z;
		// Nowell's quadratic warp; already unit-length on the cube surface.
		return glm::dvec3(
			p.x * std::sqrt(1.0 - y2 * 0.5 - z2 * 0.5 + y2 * z2 / 3.0),
			p.y * std::sqrt(1.0 - z2 * 0.5 - x2 * 0.5 + z2 * x2 / 3.0),
			p.z * std::sqrt(1.0 - x2 * 0.5 - y2 * 0.5 + x2 * y2 / 3.0));
	}

	Ref<StaticMeshResource> CubeSphereMeshBuilder::buildTile(const TileAddress& tile, double radius,
	                                                         int tileRes, glm::dvec3& outCenterLocal) {

		if (tileRes < 1) tileRes = 1;
		const CubeFaceBasis& face = cubeFaceBasis()[((tile.face % 6) + 6) % 6];
		const glm::dvec3 N(face.normal), A(face.axisU), B(face.axisV);

		// The tile's (u, v) window inside the face, in [-1, 1]. Level L splits the
		// face into 2^L tiles per axis.
		const int    perAxis = 1 << tile.level;
		const double span    = 2.0 / (double)perAxis;   // tile width in u, v
		const double u0      = -1.0 + span * (double)tile.x;
		const double v0      = -1.0 + span * (double)tile.y;

		const glm::dvec3 center = cubeToSphere(N + (u0 + span * 0.5) * A + (v0 + span * 0.5) * B) * radius;
		outCenterLocal = center;

		const int    vn = tileRes + 1;
		const double du = span / (double)tileRes;

		Ref<StaticMeshResource> mesh = CreateRef<StaticMeshResource>();
		mesh->vertexBuffer.reserve(vn * vn);
		mesh->indexBuffer.reserve(tileRes * tileRes * 6);

		for (int i = 0; i < vn; ++i) {
			const double a = u0 + du * (double)i;
			for (int j = 0; j < vn; ++j) {
				const double b = v0 + du * (double)j;

				const glm::dvec3 dir    = cubeToSphere(N + a * A + b * B);
				const glm::dvec3 sphere = dir * radius;

				Vertex vertex;
				// Tile-local offset: the sphere point and the centre are both
				// planet-scale, so subtract in double and narrow only the small
				// difference.
				vertex.position = glm::vec3(sphere - center);
				vertex.normal   = glm::vec3(dir);
				// Face-local u, v in [0, 1]: continuous across the whole face, so
				// per-face addressing shows up directly as tiling.
				vertex.uv       = glm::vec2((float)((a + 1.0) * 0.5), (float)((b + 1.0) * 0.5));
				vertex.tangent  = glm::vec3(0.0f);
				mesh->vertexBuffer.push_back(vertex);
			}
		}

		// Winding (P00, P10, P01) faces outward: with these bases
		// cross(P10-P00, P01-P00) = cross(axisU, axisV) = face normal.
		for (int i = 0; i < tileRes; ++i) {
			for (int j = 0; j < tileRes; ++j) {
				const int p00 = i * vn + j;
				const int p10 = (i + 1) * vn + j;
				const int p01 = i * vn + (j + 1);
				const int p11 = (i + 1) * vn + (j + 1);

				mesh->indexBuffer.push_back(p00);
				mesh->indexBuffer.push_back(p10);
				mesh->indexBuffer.push_back(p01);

				mesh->indexBuffer.push_back(p10);
				mesh->indexBuffer.push_back(p11);
				mesh->indexBuffer.push_back(p01);
			}
		}

		// Skirt: extrude every border vertex radially inward so a crack against a
		// differently-subdivided neighbour is covered from behind -- the skirt sits
		// deeper than the surface, so it only shows where the surface has a gap.
		const double skirtDepth = 0.05 * radius * span * 0.7853981633974483;

		std::vector<int> border[4], under[4];
		for (int k = 0; k < vn; ++k) {
			border[0].push_back(k * vn);              // b-min
			border[1].push_back(k * vn + (vn - 1));   // b-max
			border[2].push_back(k);                   // a-min
			border[3].push_back((vn - 1) * vn + k);   // a-max
		}
		const bool flip[4] = { true, false, false, true };

		for (int e = 0; e < 4; ++e) {
			under[e].reserve(vn);
			for (int k = 0; k < vn; ++k) {
				Vertex v = mesh->vertexBuffer[border[e][k]];
				v.position -= v.normal * (float)skirtDepth;
				under[e].push_back((int)mesh->vertexBuffer.size());
				mesh->vertexBuffer.push_back(v);
			}
			for (int k = 0; k + 1 < vn; ++k) {
				const int a0 = border[e][k], a1 = border[e][k + 1];
				const int b0 = under[e][k],  b1 = under[e][k + 1];
				if (flip[e]) {
					mesh->indexBuffer.push_back(a0); mesh->indexBuffer.push_back(b0); mesh->indexBuffer.push_back(a1);
					mesh->indexBuffer.push_back(a1); mesh->indexBuffer.push_back(b0); mesh->indexBuffer.push_back(b1);
				} else {
					mesh->indexBuffer.push_back(a0); mesh->indexBuffer.push_back(a1); mesh->indexBuffer.push_back(b0);
					mesh->indexBuffer.push_back(a1); mesh->indexBuffer.push_back(b1); mesh->indexBuffer.push_back(b0);
				}
			}
		}

		// One submesh spanning the tile. Its material handle stays invalid -- the
		// GBuffer pass draws this with the default material directly.
		SubMeshResource sub;
		sub.name = "tile";
		sub.firstIndex = 0;
		sub.indexCount = (uint32_t)mesh->indexBuffer.size();
		mesh->subMeshes.push_back(sub);

		return mesh;
	}

}
