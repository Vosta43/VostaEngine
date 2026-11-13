#pragma once

#include "Core/Core.h"
#include "Network/NetHandle.h"
#include "Network/RepConnection.h"
#include "Network/RepLayout.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ve {

	class Scene;

	// Client-side view of replicated components. Decoded snapshots land in a
	// shadow copy instead of the live component; every logical tick appends one
	// sample per object to a history ring, and each frame samples that history at
	// the clock's render time. Discrete fields (no interpolation op) are written
	// when the tick closes; only continuous fields are interpolated.
	class VE_API SnapshotView {
	public:
		// Depth of the per-object ring. Must cover the interpolation delay plus a
		// tick of jitter: (kHistory - 1) * tickPeriod >= delay.
		static constexpr int kHistory = 4;

		struct InterpStats {
			uint32_t starved = 0;   // render time older than the oldest sample
			uint32_t late = 0;      // render time newer than the newest sample
		};

		void reset();

		// Applies one decoded object block to the shadow state.
		bool applyBlock(const NetHandle& handle, const std::vector<RepChange>& changes);

		// Closes the logical tick: one history sample per object, and the
		// non-interpolated fields of every object seen this tick land in their
		// components.
		void closeTick(Scene& scene, uint32_t tickId, uint32_t timeMs);

		// Samples the history at renderTimeMs and writes interpolated fields.
		InterpStats interpolate(Scene& scene, double renderTimeMs);

		size_t objectCount() const { return m_objects.size(); }

	private:
		struct Sample {
			uint32_t tick = 0;
			uint32_t timeMs = 0;
			// Indexed by RepLayout::fields. Only fields with an interpolation op
			// are filled; everything else stays empty.
			std::vector<Bytes> fields;
		};

		struct ViewObject {
			NetHandle handle;
			const RepLayout* layout = nullptr;
			void* component = nullptr;
			std::vector<Bytes> values;   // authoritative decoded bytes, all fields
			Sample ring[kHistory];
			int count = 0;
			int head = kHistory - 1;     // newest valid slot
			bool present = false;        // appeared in the tick being decoded
		};

		ViewObject& ensure(const NetHandle& handle, const RepLayout& layout);
		void resolve(ViewObject& object, Scene& scene);
		void writeDiscrete(ViewObject& object);

		std::unordered_map<NetHandle, ViewObject, NetHandleHash> m_objects;
	};

}
