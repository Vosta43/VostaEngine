#include "vepch.h"
#include "EntityRegistry.h"

namespace ve {
    Entity EntityRegistry::create() {
        Entity entity;

        if (!m_freeSlots.empty()) {
            uint32_t freeIdx = m_freeSlots.back();
            m_freeSlots.pop_back();

            entity = m_entities[freeIdx];
            entity.m_id = freeIdx;
            entity.m_generation++;
            m_entities[freeIdx] = entity;
        }
        else {
            entity.m_id = static_cast<uint32_t>(m_entities.size());
            entity.m_generation = 0;
            m_entities.push_back(entity);
        }

        m_liveEntityCount++;
        return entity;
    }

    void EntityRegistry::destroy(Entity entity) {
        uint32_t idx = entity.m_id;

        if (idx >= m_entities.size()) return;
        if (m_entities[idx].m_generation != entity.m_generation) return;

        // Remove every component the entity owns. The storages (and their
        // removeFn callbacks) were recorded at emplace time, so no concrete
        // component type is hardcoded here; each removal is O(1) via the
        // per-type swap-and-pop in removeComponent<T>.
        if (auto it = m_entityStorages.find(idx); it != m_entityStorages.end()) {
            for (ComponentStorage* storage : it->second)
                storage->removeFn(idx);
            m_entityStorages.erase(it);
        }

        m_freeSlots.push_back(idx);
        m_entities[idx].m_id = 0xFFFFFFFF;
        m_liveEntityCount--;
    }

    Entity EntityRegistry::getEntity(uint32_t entityId) {
        if (entityId >= m_entities.size()) return Entity{};
        return m_entities[entityId];
    }

    Entity EntityRegistry::duplicate(Entity source) {
        uint32_t srcId = source.m_id;
        if (srcId >= m_entities.size()) return Entity{};
        if (m_entities[srcId].m_generation != source.m_generation) return Entity{};

        // create() may reuse a freed slot; the fresh handle is clean because
        // destroy() erased the source id's storage list.
        Entity dst = create();

        if (auto it = m_entityStorages.find(srcId); it != m_entityStorages.end()) {
            for (ComponentStorage* storage : it->second) {
                if (storage->copyFn) storage->copyFn(srcId, dst.m_id);
            }
        }
        return dst;
    }

}

