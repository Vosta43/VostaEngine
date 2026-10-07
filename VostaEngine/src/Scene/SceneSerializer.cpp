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
        JsonReader reader;
        if (!JsonReader::load(filepath, reader) || !reader.valid()) {
            VE_CORE_ERROR_PRINT("SceneSerializer: could not read scene from '%s'", filepath.c_str());
            return false;
        }
        return deserialize(reader);
    }

    void SceneSerializer::serialize(JsonWriter& w) {
        auto& registry = m_scene->getRegistry();
        auto entities = registry.each();

        w.set("version", kSceneVersion);
        w.beginArray("entities", entities.size());

        for (uint32_t id : entities) {
            const Entity entity = registry.getEntity(id);

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
        auto& registry = m_scene->getRegistry();

        const int version = r.getInt("version", 0);
        if (version != kSceneVersion) {
            VE_CORE_ERROR_PRINT("SceneSerializer: unsupported scene version %d (expected %d)",
                                version, kSceneVersion);
            return false;
        }

        const size_t entityCount = r.arraySize("entities");
        registry.clearAllEntity();

        for (size_t i = 0; i < entityCount; ++i) {
            const JsonReader components = r.at("entities", i).child("components");
            auto entity = registry.create();

            for (const auto& info : componentRegistry()) {
                if (!components.has(info.typeKey))
                    continue;
                info.deserialize(registry, entity, components.child(info.typeKey));
            }
        }

        return true;
    }

}
