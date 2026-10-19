#pragma once

#include "Core.h"
#include "Core/AssetHandle.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ve {

	// Hands results from worker threads to the main (GL) thread: GPU upload and
	// ResourceStorage writes may only happen there, so workers queue a resolve
	// closure instead of installing the resource themselves.
	//
	// Threading contract:
	//   track / state / pump -- main thread only
	//   complete            -- any thread
	class VE_API AssetLoadQueue {
	public:

		enum class State {
			Unknown,	// never tracked, or already resolved
			Loading
		};

		static AssetLoadQueue& get();

		void track(AssetHandle handle);

		void complete(AssetHandle handle, std::function<bool()> resolve);

		// Runs up to kMaxResolvesPerFrame resolves and carries the rest over.
		void pump();

		State state(AssetHandle handle) const;

		static constexpr size_t kMaxResolvesPerFrame = 4;

	private:
		struct Done {
			AssetHandle handle;
			std::function<bool()> resolve;
		};

		static uint64_t keyOf(AssetHandle handle) {
			return (uint64_t(handle.index()) << 32) | handle.generation();
		}

		std::mutex m_mutex;      // Done queue, MPSC guard.
		std::vector<Done> m_done;
		std::unordered_map<uint64_t, State> m_states;
	};
}
