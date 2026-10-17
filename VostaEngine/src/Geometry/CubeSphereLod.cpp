#include "vepch.h"
#include "CubeSphereLod.h"

#include <algorithm>
#include <cmath>

namespace ve {

	namespace {
		constexpr double kPi = 3.14159265358979323846;
		// A cube face spans a quarter turn.
		constexpr double kFaceAngle = kPi * 0.5;

		// A node on the error-priority frontier.
		struct FrontierNode {
			TileAddress addr;
			glm::dvec3  center;
			float       error;
		};
		struct FrontierWorse {
			bool operator()(const FrontierNode& a, const FrontierNode& b) const {
				return a.error < b.error;
			}
		};
	}

	uint64_t CubeSphereLod::pack(const TileAddress& a) {
		return ((uint64_t)(a.face  & 0xFF)     << 56)
		     | ((uint64_t)(a.level & 0xFF)     << 48)
		     | ((uint64_t)(a.x     & 0xFFFFFF) << 24)
		     |  (uint64_t)(a.y     & 0xFFFFFF);
	}

	glm::dvec3 CubeSphereLod::tileCenterLocal(const TileAddress& addr, double radius) const {
		const CubeFaceBasis& face = cubeFaceBasis()[addr.face];
		const double span = 2.0 / (double)(1 << addr.level);
		const double uc = -1.0 + span * ((double)addr.x + 0.5);
		const double vc = -1.0 + span * ((double)addr.y + 0.5);
		return cubeToSphere(glm::dvec3(face.normal)
		                  + uc * glm::dvec3(face.axisU)
		                  + vc * glm::dvec3(face.axisV)) * radius;
	}

	float CubeSphereLod::screenError(int level, const glm::dvec3& tileCenter,
	                                 const glm::dvec3& camRelCenter, double radius,
	                                 float pixelsPerRadian) const {
		// A node at `level` spans theta; rendered as kTileRes cells each spans
		// theta / kTileRes, whose sag below the arc is R(1 - cos(half)). On a bare
		// sphere this curvature sag is the only error there is.
		const double theta = kFaceAngle / (double)(1 << level);
		const double half  = theta / (double)kTileRes * 0.5;
		const double sag   = 2.0 * radius * std::sin(half) * std::sin(half);
		const double dist  = glm::length(tileCenter - camRelCenter);
		return (float)(sag / std::max(dist, 1.0) * (double)pixelsPerRadian);
	}

	bool CubeSphereLod::visible(const glm::dvec3& camRelCenter, const glm::vec3& planetCenterRender,
	                            const glm::mat4& viewProj, double radius,
	                            const glm::dvec3& tileCenter, int level) const {
		const double theta = kFaceAngle / (double)(1 << level);
		const float  bound = (float)(radius * theta);      // conservative: chord < R*angle

		// Shrink the horizon threshold by the node's CORNER reach (theta/sqrt2), not
		// the edge reach (theta/2): a corner pokes past the limb and stays visible.
		const double d = glm::length(camRelCenter);
		if (d > radius) {
			const double angularRadius = theta * 0.7071068;
			if (glm::dot(glm::normalize(tileCenter), camRelCenter / d) <= radius / d - angularRadius)
				return false;
		}

		// Frustum cull by the node's bounding sphere: extract the six planes from the
		// view-projection (Gribb-Hartmann) and reject if any plane leaves the sphere
		// entirely outside.
		const glm::vec3 c = planetCenterRender + glm::vec3(tileCenter);

		auto row = [&](int i) {
			return glm::vec4(viewProj[0][i], viewProj[1][i], viewProj[2][i], viewProj[3][i]);
		};
		const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
		const glm::vec4 planes[6] = { r3 + r0, r3 - r0, r3 + r1, r3 - r1, r3 + r2, r3 - r2 };
		for (int i = 0; i < 6; ++i) {
			const glm::vec3 nrm(planes[i]);
			const float len = glm::length(nrm);
			if (len < 1e-6f) continue;
			if (glm::dot(nrm / len, c) + planes[i].w / len < -bound) return false;
		}
		return true;
	}

	Ref<StaticMesh> CubeSphereLod::acquire(const TileAddress& addr, double radius) {
		const uint64_t key = pack(addr);
		m_used.insert(key);
		auto it = m_cache.find(key);
		if (it != m_cache.end()) return it->second;

		glm::dvec3 center;
		Ref<StaticMesh> mesh = StaticMesh::create(CubeSphereMeshBuilder::buildTile(addr, radius, kTileRes, center));
		m_cache.emplace(key, mesh);
		++m_builtLastFrame;
		return mesh;
	}

	void CubeSphereLod::update(const glm::dvec3& camRelCenter, const glm::vec3& planetCenterRender,
	                           const glm::mat4& viewProj, double radius, float pixelsPerRadian) {
		if (radius != m_radius) {
			m_cache.clear();        // tiles are baked for one radius
			m_radius = radius;
		}
		m_tiles.clear();
		m_used.clear();
		m_maxLevel = 0;
		m_builtLastFrame = 0;

		// Refine the worst tile first. A depth-first walk would spend the budget on
		// whichever face index comes first and strand the rest coarse, so hitting the
		// budget would carve a camera-dependent seam rather than degrade evenly.
		int budgetLeft = kBudget;

		std::priority_queue<FrontierNode, std::vector<FrontierNode>, FrontierWorse> frontier;

		auto pushNode = [&](const TileAddress& addr) {
			const glm::dvec3 center = tileCenterLocal(addr, radius);
			if (!visible(camRelCenter, planetCenterRender, viewProj, radius, center, addr.level))
				return;
			frontier.push(FrontierNode{ addr, center,
				screenError(addr.level, center, camRelCenter, radius, pixelsPerRadian) });
		};

		for (int f = 0; f < 6; ++f)
			pushNode(TileAddress{ f, 0, 0, 0 });

		while (!frontier.empty()) {
			const FrontierNode node = frontier.top();
			frontier.pop();

			if (node.addr.level < kMaxLevel && budgetLeft > 0 && node.error > kThreshold) {
				--budgetLeft;
				for (int q = 0; q < 4; ++q)
					pushNode(TileAddress{ node.addr.face, node.addr.level + 1,
					                      node.addr.x * 2 + (q & 1), node.addr.y * 2 + (q >> 1) });
				continue;
			}

			if (node.addr.level > m_maxLevel) m_maxLevel = node.addr.level;
			m_tiles.push_back(Tile{ acquire(node.addr, radius), ve::Vec3(node.center) });
		}

		if (m_cache.size() > kCacheCap) {
			for (auto it = m_cache.begin(); it != m_cache.end(); ) {
				if (!m_used.count(it->first)) it = m_cache.erase(it);
				else ++it;
			}
		}
	}

}
