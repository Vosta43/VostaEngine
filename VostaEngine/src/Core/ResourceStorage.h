#pragma once

#include "Core.h"
#include "AssetHandle.h"
#include <string>
#include <vector>
#include <unordered_map>


namespace ve {

	template<typename T>
	class VE_API ResourceStorage {
	public:	
        AssetHandle store(const std::string& path, Ref<T> resource) {

            auto it = m_pathToHandle.find(path);
            if (it != m_pathToHandle.end()) {
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
            entry.resource = resource;
            entry.path = path;
            entry.generation++;

            AssetHandle handle(index, entry.generation);
            m_pathToHandle[path] = handle;
            return handle;
        }

        Ref<T> get(AssetHandle handle) const {
            if (!handle.isValid()) return nullptr;
            if (handle.index() >= m_entries.size()) return nullptr;

            const Entry& entry = m_entries[handle.index()];
            if (entry.generation != handle.generation()) return nullptr;
            if (!entry.resource) return nullptr;

            return entry.resource;
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
            m_freeSlots.push_back(handle.index());
        }

        void remove(const std::string& path) {
            AssetHandle handle = find(path);
            if (handle.isValid()) remove(handle);
        }

        bool contains(AssetHandle handle) const {
            return get(handle) != nullptr;
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
		};

		std::vector<Entry> m_entries;
		std::vector<uint32_t> m_freeSlots;
		std::unordered_map<std::string, AssetHandle> m_pathToHandle;
	};

}