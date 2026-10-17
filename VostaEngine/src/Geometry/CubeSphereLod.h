#pragma once

#include "Core/Core.h"
#include "Core/Types.h"
#include "Renderer/StaticMesh.h"
#include "Geometry/CubeSphereMeshBuilder.h"

#include <glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

namespace ve {

	// Screen-space-error quad-tree over the six cube-sphere faces. Each frame it
	// picks the tiles whose projected geometric error is under a threshold, and
	// builds + caches one mesh per tile. The tile key is face-indexed
	// (face, level, x, y) -- the six roots are the addressing rewrite that a real
	// terrain streamer would adopt. Seams between differently-subdivided neighbours
	// are NOT stitched: cracks at LOD boundaries are expected.
	class CubeSphereLod {
	public:
		struct Tile {
			Ref<StaticMesh> mesh;
			ve::Vec3        centerLocal{ 0.0 };   // |centre| == planet radius
		};

		// camRelCenter: camera minus the sphere centre, planet-local (double).
		// planetCenterRender: the sphere centre in render space (float).
		// viewProj: the NON-jittered view-projection (jitter would make the LOD
		// flicker); pixelsPerRadian converts a world error at a distance to pixels.
		void update(const glm::dvec3& camRelCenter, const glm::vec3& planetCenterRender,
		            const glm::mat4& viewProj, double radius, float pixelsPerRadian);

		const std::vector<Tile>& tiles() const { return m_tiles; }
		int    maxLevel() const { return m_maxLevel; }
		size_t tileCount() const { return m_tiles.size(); }
		int    builtLastFrame() const { return m_builtLastFrame; }

	private:
		bool       visible(const glm::dvec3& camRelCenter, const glm::vec3& planetCenterRender,
		                   const glm::mat4& viewProj, double radius,
		                   const glm::dvec3& tileCenter, int level) const;
		float      screenError(int level, const glm::dvec3& tileCenter,
		                       const glm::dvec3& camRelCenter, double radius, float pixelsPerRadian) const;
		glm::dvec3 tileCenterLocal(const TileAddress& addr, double radius) const;
		Ref<StaticMesh> acquire(const TileAddress& addr, double radius);
		static uint64_t pack(const TileAddress& addr);

		std::vector<Tile>                             m_tiles;
		std::unordered_map<uint64_t, Ref<StaticMesh>> m_cache;
		std::unordered_set<uint64_t>                  m_used;
		double m_radius = -1.0;      // tiles are baked for one radius; cache on change
		int    m_maxLevel = 0;
		int    m_builtLastFrame = 0;

		static constexpr int    kTileRes   = 16;
		static constexpr float  kThreshold = 2.0f;    // screen-space error, pixels
		static constexpr int    kMaxLevel  = 16;
		static constexpr int    kBudget    = 24;      // subdivisions per frame
		static constexpr size_t kCacheCap  = 4096;
	};

}
