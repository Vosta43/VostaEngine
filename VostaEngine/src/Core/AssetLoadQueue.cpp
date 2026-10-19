#include "vepch.h"
#include "AssetLoadQueue.h"

namespace ve {
	AssetLoadQueue& AssetLoadQueue::get() {
		static AssetLoadQueue* queue = new AssetLoadQueue();
		return *queue;
	}

	void AssetLoadQueue::track(AssetHandle handle) {
		m_states[keyOf(handle)] = State::Loading;
	}

	void AssetLoadQueue::complete(AssetHandle handle, std::function<bool()> resolve) {
		std::lock_guard lock(m_mutex);
		m_done.push_back({ handle, std::move(resolve) });
	}

	void AssetLoadQueue::pump() {
		std::vector<Done> batch;
		{
			std::lock_guard lock(m_mutex);
			batch.swap(m_done);
		}

		size_t resolved = 0;
		for (Done& done : batch) {
			// Over budget this frame; carry it over to the next pump.
			if (resolved >= kMaxResolvesPerFrame) {
				std::lock_guard lock(m_mutex);
				m_done.push_back(std::move(done));
				continue;
			}
			done.resolve();
			// Only in-flight loads are tracked, so the map stays proportional
			// to the queue instead of growing by one entry per load ever made.
			m_states.erase(keyOf(done.handle));
			resolved++;
		}
	}

	AssetLoadQueue::State AssetLoadQueue::state(AssetHandle handle) const {
		auto it = m_states.find(keyOf(handle));
		return it != m_states.end() ? it->second : State::Unknown;
	}
}
