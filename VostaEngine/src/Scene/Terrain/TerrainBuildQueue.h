#pragma once

#include "Core/Core.h"
#include "Core/JobSystem.h"
#include "Scene/Terrain/QuadTreeTerrain.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace ve {

	// What one tile's build produces off the render thread: the geometry, plus the
	// height field the weights are baked against. The tile only takes them at
	// commit time, so nothing here touches the scene while it is being filled.
	struct TerrainMeshBuild {
		QuadTreeTerrain::BuildResult mesh;
		std::vector<float>           heights;      // raw, [x * heightRes + z]
		int                          heightRes = 0;

		// Layer weights baked on the same worker that built the mesh, so the render
		// thread only has to upload them. Empty when the tile has nothing to bake.
		std::vector<uint8_t>         weightData;   // slice-major, layer * res * res
		int                          weightRes = 0;
		int                          layerCount = 0;
		uint64_t                     weightKey = 0;   // inputs the bake was keyed from

		bool valid() const { return mesh.valid() && heightRes > 0; }
	};

	// Tile builds in flight on worker threads. Lives on the scene's terrain system
	// as runtime-only state, so it dies with the system and, because a play session's
	// scene is a serializer round-trip, never reaches a clone.
	//
	// Main-thread only: the entries are touched by submit and takeFinished, and a
	// product is only read once its future reports ready, which orders it after the
	// worker's writes.
	class TerrainBuildQueue {
	public:
		struct Entry {
			int64_t               coord = 0;   // packed tile coordinate
			uint64_t              key = 0;     // build inputs the job was started from
			Ref<TerrainMeshBuild> product;
			TaskHandle<void>      handle;      // completion only
		};

		// One build per worker, capped: a camera that keeps moving would otherwise
		// pile up whole tiles' worth of CPU geometry behind the render thread.
		static size_t capacity() {
			return std::clamp(JobSystem::get().threadCount(), (size_t)1, (size_t)4);
		}

		// Queue one tile's build. `work` runs on a worker and must fill only the
		// product it is handed. False when this coordinate is already queued or the
		// queue is full, in which case the caller should try again next frame.
		bool submit(int64_t coord, uint64_t key, std::function<void(TerrainMeshBuild&)> work) {
			if (m_entries.size() >= capacity() || m_entries.count(coord) != 0)
				return false;

			Entry entry;
			entry.coord = coord;
			entry.key = key;
			entry.product = CreateRef<TerrainMeshBuild>();
			// The product is owned by the entry and written through a captured Ref, so
			// nothing has to move out of the future when the job finishes.
			entry.handle = JobSystem::get().schedule(
				[product = entry.product, work = std::move(work)] { work(*product); });

			m_entries.emplace(coord, std::move(entry));
			return true;
		}

		// Remove and return up to `max` builds that have finished since the last
		// call, leaving the rest queued. The cap spreads a burst of ready builds
		// over frames, so one frame never pays several tiles' upload at once.
		std::vector<Entry> takeFinished(size_t max) {
			std::vector<Entry> done;
			for (auto it = m_entries.begin(); it != m_entries.end() && done.size() < max;) {
				if (it->second.handle.future.wait_for(std::chrono::seconds(0))
						!= std::future_status::ready) {
					++it;
					continue;
				}
				done.push_back(std::move(it->second));
				it = m_entries.erase(it);
			}
			return done;
		}

	private:
		std::unordered_map<int64_t, Entry> m_entries;
	};

}
