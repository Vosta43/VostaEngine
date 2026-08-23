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

        //removeComponent<Transform>(entity);
        //removeComponent<SpriteRenderer>(entity);
        //removeComponent<RigidBody>(entity);
        // FIXME: Every time a new component type is added to the engine, a
        // corresponding removeComponent<T>(entity) call must be manually added
        // here. This is fragile and will cause silent leaks if forgotten.
        // A better approach: store a type-erased function pointer per component
        // type during emplace(), then iterate and call those here. See notes below.

        m_freeSlots.push_back(idx);
        m_entities[idx].m_id = 0xFFFFFFFF;
        m_liveEntityCount--;
    }


}

