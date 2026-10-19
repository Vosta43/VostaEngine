#pragma once

#include "Core.h"
#include "AssetHandle.h"
#include "Core/ResourceMemory.h"
#include "Log.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>


namespace ve {

	// One monotonic frame counter shared by every storage; advanced by
	// ResourceManager::tick(). Only ages LRU entries.
	class VE_API ResourceLifeTimeTickManager {
	public:
		static ResourceLifeTimeTickManager& get();

		void tick();
		uint64_t resourceClock() const;

	private:
		uint64_t m_clock = 0;
	};

	template<typename T>
	class VE_API ResourceStorage {
	public:
        enum class LoadState {
            Unloaded,
            Loading,
            Loaded
        };

        AssetHandle store(const std::string& path, Ref<T> resource,
            LoadState state = LoadState::Loaded) {

            auto it = m_pathToHandle.find(path);
            if (it != m_pathToHandle.end()) {
                // A slot kept alive by unload() takes the payload back, so
                // handles handed out before the unload stay usable.
                Entry& entry = m_entries[it->second.index()];
                if (entry.loadState == LoadState::Unloaded && resource) {
                    entry.resource = std::move(resource);
                    entry.bytes = state == LoadState::Loaded
                        ? estimateBytes(*entry.resource) : 0;
                    entry.loadState = state;
                    entry.lastUseTick = state == LoadState::Loaded
                        ? ResourceLifeTimeTickManager::get().resourceClock() : 0;
                    entry.wanted = false;
                    entry.retryAfterTick = 0;
                }
                return it->second;
            }

            // Allocate a slot: pull from free list if available, otherwise expand the pool.
            uint32_t index;
            if (m_freeSlots.empty()) {
                index = static_cast<uint32_t>(m_entries.size());
                m_entries.emplace_back();
            }
            else {
                index = m_freeSlots.back();
                m_freeSlots.pop_back();
            }
            // Fill the slot and bump the generation counter.
            // This invalidates any external AssetHandles that still reference this index
            // from a previous life — they will now fail generation checks in get().
            Entry& entry = m_entries[index];
            entry.resource = std::move(resource);
            entry.bytes = state == LoadState::Loaded && entry.resource
                ? estimateBytes(*entry.resource) : 0;
            entry.path = path;
            entry.generation++;
            entry.loadState = state;
            entry.lastUseTick = state == LoadState::Loaded
                ? ResourceLifeTimeTickManager::get().resourceClock() : 0;
            entry.wanted = false;
            entry.retryAfterTick = 0;
            entry.pinned = false;

            AssetHandle handle(index, entry.generation);
            m_pathToHandle[path] = handle;
            return handle;
        }

        // Registers a slot whose payload is still being loaded; the placeholder
        // stands in until AssetLoadQueue::pump() installs the upload via replace().
        AssetHandle storeLoading(const std::string& path, Ref<T> resource) {
            return store(path, std::move(resource), LoadState::Loading);
        }

        // Marks a resident entry as used and returns its payload. A loading entry
        // returns its placeholder, as before. An unloaded entry is a miss but
        // keeps its handle: the demand is recorded so the residency layer can
        // schedule a reload into the same slot.
        Ref<T> get(AssetHandle handle) {
            if (!handle.isValid()) return nullptr;
            if (handle.index() >= m_entries.size()) return nullptr;

            Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return nullptr;

            if (entry.loadState == LoadState::Unloaded) {
                entry.wanted = true;
                return nullptr;
            }

            entry.lastUseTick = ResourceLifeTimeTickManager::get().resourceClock();
            return entry.resource;
        }

        // Installs the finished payload of an async load into its existing slot.
        bool replace(AssetHandle handle, Ref<T> resource) {
            if (!handle.isValid()) return false;
            if (handle.index() >= m_entries.size()) return false;
            if (!resource) return false;

            Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return false;

            entry.resource = std::move(resource);
            entry.bytes = estimateBytes(*entry.resource);
            entry.loadState = LoadState::Loaded;
            entry.lastUseTick = ResourceLifeTimeTickManager::get().resourceClock();
            entry.wanted = false;
            entry.retryAfterTick = 0;

            return true;
        }

        LoadState state(AssetHandle handle) const {
            if (!handle.isValid() || handle.index() >= m_entries.size())
                return LoadState::Unloaded;
            const Entry& entry = m_entries[handle.index()];
            return entry.generation == handle.generation()
                ? entry.loadState : LoadState::Unloaded;
        }

        bool isUnloaded(AssetHandle handle) const {
            return state(handle) == LoadState::Unloaded;
        }

        // Exempts a slot from eviction. For payloads that exist only in memory:
        // there is no path to rebuild them from.
        void pin(AssetHandle handle) {
            if (!handle.isValid()) return;
            if (handle.index() >= m_entries.size()) return;

            Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return;

            entry.pinned = true;
        }

        // Re-issues an unloaded slot's load: installs the placeholder, if any,
        // and flips the state so get() serves it instead of recording demand.
        void markLoading(AssetHandle handle, Ref<T> placeholder = nullptr) {
            if (!handle.isValid() || handle.index() >= m_entries.size())
                return;

            Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return;
            if (entry.loadState != LoadState::Unloaded) return;

            entry.resource = std::move(placeholder);
            entry.bytes = 0;
            entry.loadState = LoadState::Loading;
            entry.lastUseTick = 0;
            entry.wanted = false;
        }

        // Drops the payload but keeps the slot's identity (path and generation),
        // so holders of the handle stay valid and a later load can refill it.
        // Main thread only: releasing the last Ref may run GL destruction.
        void unload(AssetHandle handle) {
            if (!handle.isValid()) return;
            if (handle.index() >= m_entries.size()) return;

            Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return;

            VE_CORE_INFO_PRINT("Resource unloaded: %s (%.1f KB)",
                entry.path.c_str(), (double)entry.bytes / 1024.0);

            entry.resource = nullptr;
            entry.bytes = 0;
            entry.loadState = LoadState::Unloaded;
            entry.wanted = false;
            entry.retryAfterTick = 0;
        }

        // Like unload(), but records a cooldown so a permanently missing file
        // is not re-requested every frame by get().
        void fail(AssetHandle handle, uint64_t retryAtTick) {
            if (!handle.isValid()) return;
            if (handle.index() >= m_entries.size()) return;

            Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return;

            entry.resource = nullptr;
            entry.bytes = 0;
            entry.loadState = LoadState::Unloaded;
            entry.wanted = false;
            entry.retryAfterTick = retryAtTick;
        }

        AssetHandle find(const std::string& path) const {
            auto it = m_pathToHandle.find(path);
            return it != m_pathToHandle.end() ? it->second : INVALID_ASSET_HANDLE;
        }

        const std::string& getPath(AssetHandle handle) const {
            static const std::string empty;
            if (!handle.isValid() || handle.index() >= m_entries.size()) return empty;
            const Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return empty;
            return entry.path;
        }

        void remove(AssetHandle handle) {
            if (!handle.isValid()) return;
            if (handle.index() >= m_entries.size()) return;

            Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return;

            m_pathToHandle.erase(entry.path);
            entry.resource = nullptr;
            entry.path.clear();
            entry.bytes = 0;
            entry.loadState = LoadState::Unloaded;
            entry.wanted = false;
            entry.retryAfterTick = 0;
            entry.generation++;

            m_freeSlots.push_back(handle.index());
        }

        void remove(const std::string& path) {
            AssetHandle handle = find(path);
            if (handle.isValid()) remove(handle);
        }

        // Re-key entries after a file or folder is renamed/moved on disk, keeping
        // the in-memory resources (and any unsaved edits) instead of reloading.
        // `oldPath`/`newPath` are storage-relative. Handles a single file and a
        // folder prefix alike.
        void renamePrefix(const std::string& oldPath, const std::string& newPath) {
            if (oldPath.empty() || oldPath == newPath)
                return;

            const std::string dirPrefix = oldPath + "/";
            std::vector<std::pair<std::string, AssetHandle>> moved;
            for (auto it = m_pathToHandle.begin(); it != m_pathToHandle.end(); ) {
                if (it->first == oldPath) {
                    moved.emplace_back(newPath, it->second);
                    it = m_pathToHandle.erase(it);
                }
                else if (it->first.rfind(dirPrefix, 0) == 0) {
                    moved.emplace_back(newPath + it->first.substr(oldPath.size()), it->second);
                    it = m_pathToHandle.erase(it);
                }
                else {
                    ++it;
                }
            }

            for (auto& [path, handle] : moved) {
                m_pathToHandle[path] = handle;
                if (handle.index() < m_entries.size())
                    m_entries[handle.index()].path = path;
            }
        }

        size_t evictLRU(size_t bytesToFree, uint64_t now, uint64_t minAge) {
            std::vector<AssetHandle> victims;
            for (uint32_t i = 0; i < (uint32_t)m_entries.size(); ++i) {
                const auto& entry = m_entries[i];
                if (entry.loadState != LoadState::Loaded) continue;
                if (entry.pinned) continue;
                // Without a size estimate nothing can be reclaimed by evicting it.
                if (entry.bytes == 0) continue;
                if (now - entry.lastUseTick < minAge) continue;
                if (entry.resource.use_count() > 1) continue;   // someone holds a Ref
                victims.emplace_back(i, entry.generation);
            }

            std::sort(victims.begin(), victims.end(), [&](AssetHandle a, AssetHandle b) {
                return m_entries[a.index()].lastUseTick < m_entries[b.index()].lastUseTick;
            });

            size_t freed = 0;
            for (AssetHandle h : victims) {
                if (freed >= bytesToFree) break;
                freed += m_entries[h.index()].bytes;
                unload(h);
            }

            return freed;
        }

        // Drains the demand marks left by get() on unloaded slots. Entries in
        // their failure cooldown are skipped; a still-live caller re-marks them
        // on the next get().
        std::vector<std::string> takeWantedPaths() {
            const uint64_t now = ResourceLifeTimeTickManager::get().resourceClock();
            std::vector<std::string> paths;
            for (Entry& entry : m_entries) {
                if (!entry.wanted) continue;
                entry.wanted = false;
                if (entry.loadState != LoadState::Unloaded) continue;
                if (now < entry.retryAfterTick) continue;
                paths.push_back(entry.path);
            }
            return paths;
        }

        size_t registeredCount() const { return m_pathToHandle.size(); }

        size_t residentCount() const {
            size_t count = 0;
            for (const Entry& entry : m_entries)
                if (entry.loadState == LoadState::Loaded) count++;
            return count;
        }

        size_t residentBytes() const {
            size_t bytes = 0;
            for (const Entry& entry : m_entries)
                if (entry.loadState == LoadState::Loaded) bytes += entry.bytes;
            return bytes;
        }

        size_t wantedCount() const {
            size_t count = 0;
            for (const Entry& entry : m_entries)
                if (entry.wanted) count++;
            return count;
        }

        // Resolves to something drawable (real payload or loading placeholder).
        // Deliberately does not call get(): a query must not mark the entry as
        // demanded.
        bool contains(AssetHandle handle) const {
            if (!handle.isValid()) return false;
            if (handle.index() >= m_entries.size()) return false;

            const Entry& entry = m_entries[handle.index()];
            return entry.generation == handle.generation() && entry.loadState != LoadState::Unloaded;
        }

        bool contains(const std::string& path) const {
            return find(path).isValid();
        }

        size_t size() const {
            return m_pathToHandle.size();
        }

        const auto& entries() const { return m_pathToHandle; }

	private:
        // A single slot in the resource pool. Lives in m_entries at a stable
        // index for its entire lifetime. When freed, the slot is NOT destroyed
        // — it is recycled via the free list and its generation is bumped on
        // the next store().
		struct Entry {
			Ref<T> resource;
			std::string path;
			uint32_t generation = 0;
            uint64_t lastUseTick = 0;
            size_t bytes = 0;
            LoadState loadState = LoadState::Unloaded;
            bool wanted = false;
            bool pinned = false;
            uint64_t retryAfterTick = 0;
		};

		std::vector<Entry> m_entries;
		std::vector<uint32_t> m_freeSlots;
		std::unordered_map<std::string, AssetHandle> m_pathToHandle;
	};

}