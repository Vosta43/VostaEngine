#pragma once

#include <vector>
#include <any>
#include <typeindex>
#include <array>
#include <algorithm>

#include "Core/Core.h"
#include "Entity.h"

namespace ve {
	class EntityRegistry {
	public:

		Entity create();
		void destroy(Entity entity);
		// Returns the live handle stored for entityId (correct m_generation),
		// or an invalid handle if the id is out of range.
		Entity getEntity(uint32_t entityId);
		// Creates a new entity that is a copy of source: same components, same
		// values. Component copying is type-erased via each storage's copyFn.
		Entity duplicate(Entity source);

        //template<typename T, typename... Args>
        //T& emplace(Entity entity, Args&&... args) {
        //   
        //   auto& compVec = getComponentVector<T>();
        //    auto& compMap = getComponentMap<T>();
        //    auto& idxToEntity = getIndexToEntityMap<T>();

        //    compVec.emplace_back(std::forward<Args>(args)...);
        //    size_t index = compVec.size() - 1;
        //    compMap[entity.m_id] = index;
        //    idxToEntity.push_back(entity.m_id);

        //    return compVec.back();
        //}

        template<typename T, typename... Args>
        T& emplace(Entity entity, Args&&... args) {
            auto& compVec = getComponentVector<T>();
            auto& compMap = getComponentMap<T>();
            auto& idxToEntity = getIndexToEntityMap<T>();
            auto& storage = m_storage[std::type_index(typeid(T))];

            // Store the getPtr lambda once
            if (!storage.getPtr) {
                storage.getPtr = [this](uint32_t eid) -> void* {
                    auto& cMap = getComponentMap<T>();
                    auto& cVec = getComponentVector<T>();
                    auto iter = cMap.find(eid);
                    if (iter != cMap.end()) {
                        return &cVec[iter->second];
                    }
                    return nullptr;
                    };
            }

            // Store the type-erased remove callback once. destroy() uses it to
            // remove this component type from an entity without the caller
            // knowing T.
            if (!storage.removeFn) {
                storage.removeFn = [this](uint32_t eid) {
                    Entity e; e.m_id = eid;
                    removeComponent<T>(e);
                };
            }

            // Store the type-erased copy callback once. duplicate() uses it to
            // copy this component type from a source entity to a new one.
            if (!storage.copyFn) {
                storage.copyFn = [this](uint32_t srcId, uint32_t dstId) {
                    auto& cMap = getComponentMap<T>();
                    auto& cVec = getComponentVector<T>();
                    auto iter = cMap.find(srcId);
                    if (iter == cMap.end()) return;
                    Entity dst; dst.m_id = dstId;
                    emplace<T>(dst, cVec[iter->second]);
                };
            }

            // has<T> must be checked before the map insert below — otherwise it
            // is always true after inserting, and the storage would never be
            // recorded, leaving destroy() with nothing to remove.
            bool alreadyHad = has<T>(entity);

            compVec.emplace_back(std::forward<Args>(args)...);
            size_t index = compVec.size() - 1;
            compMap[entity.m_id] = index;
            idxToEntity.push_back(entity.m_id);

            // Record this component type as owned by the entity, so destroy()
            // can find it. Guarded so re-emplacing the same type on the same
            // entity doesn't double-register.
            if (!alreadyHad)
                m_entityStorages[entity.m_id].push_back(&storage);

            return compVec.back();
        }

        template<typename T>
        bool has(Entity entity) {
            auto& map = getComponentMap<T>();
            return map.find(entity.m_id) != map.end();
        }

        template<typename T>
        bool has(uint32_t entityId) {
            auto& map = getComponentMap<T>();
            return map.find(entityId) != map.end();
        }

        // Retrieve component T belonging to the specified entity.
        // An absent component returns a per-type placeholder instead of reading
        // past the dense array, so a stale id — or an entity the editor/AI made
        // without this component — can never dereference garbage. Callers that
        // must tell "absent" apart should test has<T>() first.
        template<typename T>
        T& get(Entity entity) {
            return get<T>(entity.getId());
        }

        template<typename T>
        T& get(uint32_t entityId) {
            auto& componentMap = getComponentMap<T>();
            auto& componentVec = getComponentVector<T>();
            auto it = componentMap.find(entityId);
            if (it != componentMap.end() && it->second < componentVec.size())
                return componentVec[it->second];
            static T placeholder{};
            return placeholder;
        }

        std::vector<uint32_t> each() {
            std::vector<uint32_t> result;
            for (const auto& entity : m_entities) {
                if (entity.m_id != 0xFFFFFFFF) {  // TODO : Replace by Entity::INVALID_ID
                    result.push_back(entity.m_id);
                }
            }
            return result;
        }


        template<typename T>
        auto view() {
            auto& idxToEntity = getIndexToEntityMap<T>();
            struct View {
                std::vector<uint32_t>::iterator m_begin;
                std::vector<uint32_t>::iterator m_end;
                auto begin() { return m_begin; }
                auto end() { return m_end; }
                bool empty() { return m_begin == m_end; }
            };
            return View{ idxToEntity.begin(), idxToEntity.end() };
        }

        // Multi-component : returns a vector of EntityIDs that possess ALL
        // specified component types. The shortest indexToEntity array is chosen as
        // the candidate pool to minimize the number of cross-map lookups.
        template<typename... Components>
        auto group() {

            std::array<const std::vector<uint32_t>*, sizeof...(Components)> arrays = {
                &getIndexToEntityMap<Components>()... };

            const auto* candidateVec = *std::min_element(arrays.begin(), arrays.end(),
                [](auto* a, auto* b) { return a->size() < b->size(); });

            std::array<const std::unordered_map<uint32_t, size_t>*, sizeof...(Components)> maps = {
                &getComponentMap<Components>()... };

            std::vector<uint32_t> result;
            for (auto id : *candidateVec) {
                bool hasAll = true;
                for (auto* map : maps) {
                    if (map->count(id) == 0) {
                        hasAll = false;
                        break;
                    }
                }
                if (hasAll)
                    result.push_back(id);
            }
            return result;
        }

        // Returns the reverse mapping from component array index to EntityID.
        // indexToEntity[i] tells you which entity owns the component stored at
        // position i in the corresponding getComponentVector<T>().
        template<typename T>
        std::vector<uint32_t>& getIndexToEntityMap() {
            
            auto& storage = m_storage[std::type_index(typeid(T))];
            if (!storage.indexToEntity.has_value()) {
                storage.indexToEntity = std::vector<uint32_t>();
            }
            return std::any_cast<std::vector<uint32_t>&>(storage.indexToEntity);
        }

        template<typename T>
        void removeComponent(Entity entity) {

            auto& compVec = getComponentVector<T>();
            auto& compMap = getComponentMap<T>();
            auto& idxToEntity = getIndexToEntityMap<T>();

            auto it = compMap.find(entity.m_id);
            if (it == compMap.end()) return;

            size_t deadIdx = it->second;
            size_t lastIdx = compVec.size() - 1;

            if (deadIdx != lastIdx) {
                uint32_t movedEntityId = idxToEntity[lastIdx];

                std::swap(compVec[deadIdx], compVec[lastIdx]);
                std::swap(idxToEntity[deadIdx], idxToEntity[lastIdx]);

                compMap[movedEntityId] = deadIdx;
            }

            compVec.pop_back();
            idxToEntity.pop_back();
            compMap.erase(it);
        }

        // Returns a reference to the std::vector<T> that stores all instances of
       // component type T. If the vector does not exist yet, it is lazily created.
        template<typename T>
        std::vector<T>& getComponentVector() {
            auto& storage = m_storage[std::type_index(typeid(T))];
            if (!storage.vector.has_value()) {
                storage.vector = std::vector<T>();
            }
            return std::any_cast<std::vector<T>&>(storage.vector);
        }


        std::vector<std::pair<std::string, void*>> getComponentsForEntity(uint32_t entityId) {
            std::vector<std::pair<std::string, void*>> result;
            for (auto& [typeIdx, storage] : m_storage) {
                if (!storage.getPtr) continue;
                void* ptr = storage.getPtr(entityId);
                if (ptr) {
                    std::string raw = typeIdx.name();
                    size_t space = raw.rfind(' ');
                    std::string afterSpace = (space != std::string::npos) ? raw.substr(space + 1) : raw;
                    size_t colon = afterSpace.rfind("::");
                    std::string clean = (colon != std::string::npos) ? afterSpace.substr(colon + 2) : afterSpace;
                    result.emplace_back(clean, ptr);
                }
            }
            return result;
        }

        void clearAllEntity() {

            m_entityStorages.clear();
            m_storage.clear();
            m_entities.clear();
            m_freeSlots.clear();
            m_liveEntityCount = 0;
        }
	
	private:
        
        // Internal storage bucket for one component type.
        struct ComponentStorage {
            std::any vector;             // Component array
            std::any map;                // EntityID to component array index. Lazily created via getComponentMap<T>().
            std::any indexToEntity;      // Component array index to EntityID. Reverse mapping used during swap-and-pop removal to locate the moved entity.

            std::function<void* (uint32_t)> getPtr;  // returns void* to component
            std::function<void(uint32_t)> removeFn;  // removes this component type from an entity by id
            std::function<void(uint32_t, uint32_t)> copyFn;  // copies this component type from src entity id to dst entity id
        };

        // Type-erased component registry. Keyed by std::type_index, which is a
        // globally unique identifier per C++ type. This allows O(1) lookup of any
        // component's backing store by its type alone.
        std::unordered_map<std::type_index, ComponentStorage> m_storage;

        // Per-entity list of the component storages it owns, populated at
        // emplace time. destroy() iterates it to remove every component without
        // hardcoding any concrete type. Stored as pointers because
        // std::unordered_map keeps element addresses stable across rehashes.
        std::unordered_map<uint32_t, std::vector<ComponentStorage*>> m_entityStorages;

        // Returns the EntityID-to-index map for component type T.
        // Key: entity's unique ID (uint32_t). Value: index into the corresponding
        // getComponentVector<T>() where that entity's component instance resides.
        // the exact offset into the dense component array.
        template<typename T>
        std::unordered_map<uint32_t, size_t>& getComponentMap() {
            auto& storage = m_storage[std::type_index(typeid(T))];
            if (!storage.map.has_value()) {
                // Lazily created on first access.
                storage.map = std::unordered_map<uint32_t, size_t>();
            }
            return std::any_cast<std::unordered_map<uint32_t, size_t>&>(storage.map);
        }

        // Flat container of all active entities. Entities are stored by value and
        // indexed by their position in this vector (implicit EntityID = array index).
        std::vector<Entity> m_entities;

        // explicit free list stack
        std::vector<uint32_t> m_freeSlots;
        uint32_t m_liveEntityCount = 0;

	};
}