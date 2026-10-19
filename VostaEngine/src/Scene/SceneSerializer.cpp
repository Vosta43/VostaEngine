#include "vepch.h"
#include "SceneSerializer.h"
#include "Components.h"
#include "Core/ComponentRegistry.h"

namespace ve {

    // Bumped whenever the emitted layout changes incompatibly. Files carrying
    // any other value are rejected rather than half-loaded.
    static constexpr int kSceneVersion = 2;

    bool SceneSerializer::saveToFile(const std::string& filepath) {
        JsonWriter w;
        serialize(w);
        if (!w.writeToFile(filepath)) {
            VE_CORE_ERROR_PRINT("SceneSerializer: could not write scene to '%s'", filepath.c_str());
            return false;
        }
        return true;
    }

    bool SceneSerializer::loadFromFile(const std::string& filepath) {
        if (!beginLoad(filepath))
            return false;
        loadChunk(m_total);
        return true;
    }

    bool SceneSerializer::beginLoad(const std::string& filepath) {
        if (!JsonReader::load(filepath, m_reader) || !m_reader.valid()) {
            VE_CORE_ERROR_PRINT("SceneSerializer: could not read scene from '%s'", filepath.c_str());
            return false;
        }
        return beginDeserialize(m_reader);
    }

    bool SceneSerializer::loadChunk(size_t maxEntities) {
        auto& registry = m_scene->getRegistry();

        size_t end = m_cursor + maxEntities;
        if (end > m_total)
            end = m_total;

        for (; m_cursor < end; ++m_cursor) {
            const JsonReader components = m_reader.at("entities", m_cursor).child("components");
            auto entity = registry.create();

            for (const auto& info : componentRegistry()) {
                if (!components.has(info.typeKey))
                    continue;
                info.deserialize(registry, entity, components.child(info.typeKey));
            }
        }
        return m_cursor >= m_total;
    }

    bool SceneSerializer::beginDeserialize(const JsonReader& r) {
        const int version = r.getInt("version", 0);
        if (version != kSceneVersion) {
            VE_CORE_ERROR_PRINT("SceneSerializer: unsupported scene version %d (expected %d)",
                                version, kSceneVersion);
            return false;
        }

        m_total = r.arraySize("entities");
        m_cursor = 0;
        m_scene->getRegistry().clearAllEntity();
        return true;
    }

    void SceneSerializer::serialize(JsonWriter& w) {
        auto& registry = m_scene->getRegistry();
        auto entities = registry.each();

        w.set("version", kSceneVersion);
        w.beginArray("entities", entities.size());

        for (uint32_t id : entities) {
            const Entity entity = registry.getEntity(id);

            // A streamed terrain tile exists because a camera asked for it, so it
            // is runtime state, not scene state. Dropping it here covers all three
            // consumers at once: save, load, and the play session's clone, which
            // round-trips through this same serializer.
            if (registry.has<TerrainComponent>(id) && registry.get<TerrainComponent>(id).bStreamed)
                continue;

            w.beginObject();
            w.beginObject("components");

            // The registry is the single source of truth for what a component
            // looks like in the file; adding a VECOMPONENT is all it takes for
            // a new type to be saved and loaded.
            for (const auto& info : componentRegistry()) {
                if (!info.present(registry, entity))
                    continue;
                w.beginObject(info.typeKey);
                info.serialize(registry, entity, w);
                w.end();
            }

            w.end();
            w.end();
        }

        w.end();
    }

    bool SceneSerializer::deserialize(const JsonReader& r) {
        m_reader = r;
        if (!beginDeserialize(r))
            return false;
        loadChunk(m_total);
        return true;
    }

}
