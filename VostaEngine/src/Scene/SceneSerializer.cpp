#include "vepch.h"
#include "SceneSerializer.h"
#include "Components.h"

namespace ve {

    bool SceneSerializer::saveToFile(const std::string& filepath) {
        TextArchive ar(filepath, ArchiveMode::write);
        if (!ar.isGood()) return false;
        serialize(ar);
        return true;
    }

    bool SceneSerializer::loadFromFile(const std::string& filepath) {
        TextArchive ar(filepath, ArchiveMode::read);
        if (!ar.isGood()) return false;
        deserialize(ar);
        return true;
    }

    void SceneSerializer::serialize(Archive& ar) {
        auto& registry = m_scene->getRegistry();
        auto entities = registry.each();

        ar << "version" << 1;
        ar << "entity_count" << (int32_t)entities.size();

        for (uint32_t id : entities) {
            ar << "{";

            if (registry.has<NameComponent>(id)) {
                ar << "NameComponent";
                ar << "{";
                registry.get<NameComponent>(id).serialize(ar);
                ar << "}";
            }

            if (registry.has<TransformComponent>(id)) {
                ar << "TransformComponent";
                ar << "{";
                registry.get<TransformComponent>(id).serialize(ar);
                ar << "}";
            }

            if (registry.has<SpriteRendererComponent>(id)) {
                ar << "SpriteRendererComponent";
                ar << "{";
                registry.get<SpriteRendererComponent>(id).serialize(ar);
                ar << "}";
            }

            if (registry.has<StaticMeshComponent>(id)) {
                ar << "StaticMeshComponent";
                ar << "{";
                registry.get<StaticMeshComponent>(id).serialize(ar);
                ar << "}";
            }

            if (registry.has<SkyBoxComponent>(id)) {
                ar << "SkyBoxComponent";
                ar << "{";
                registry.get<SkyBoxComponent>(id).serialize(ar);
                ar << "}";
            }

            if (registry.has<LightComponent>(id)) {
                ar << "LightComponent";
                ar << "{";
                registry.get<LightComponent>(id).serialize(ar);
                ar << "}";
            }

            if (registry.has<TerrainComponent>(id)) {
                ar << "TerrainComponent";
                ar << "{";
                registry.get<TerrainComponent>(id).serialize(ar);
                ar << "}";
            }

            if (registry.has<AtmosphereComponent>(id)) {
                ar << "AtmosphereComponent";
                ar << "{";
                registry.get<AtmosphereComponent>(id).serialize(ar);
                ar << "}";
            }

            ar << "}";
        }
    }

    void SceneSerializer::deserialize(Archive& ar) {
        auto& registry = m_scene->getRegistry();

        std::string key;
        int32_t version = 0;

        ar >> key;
        if (key != "version") return;
        ar >> version;

        ar >> key;
        if (key != "entity_count") return;

        int32_t entityCount = 0;
        ar >> entityCount;

        registry.clearAllEntity();

        for (int32_t i = 0; i < entityCount; ++i) {
            std::string openBrace;
            ar >> openBrace;
            if (openBrace != "{") return;

            auto entity = registry.create();

            while (true) {
                std::string componentType;
                ar >> componentType;

                if (componentType == "}") break;

                std::string componentOpen;
                ar >> componentOpen;
                if (componentOpen != "{") return;

                if (componentType == "NameComponent") {
                    auto& comp = registry.emplace<NameComponent>(entity);
                    comp.deserialize(ar);
                }
                else if (componentType == "TransformComponent") {
                    auto& comp = registry.emplace<TransformComponent>(entity);
                    comp.deserialize(ar);
                }
                else if (componentType == "SpriteRendererComponent") {
                    auto& comp = registry.emplace<SpriteRendererComponent>(entity);
                    comp.deserialize(ar);
                }
                else if (componentType == "StaticMeshComponent") {
                    auto& comp = registry.emplace<StaticMeshComponent>(entity);
                    comp.deserialize(ar);
                }
                else if (componentType == "SkyBoxComponent") {
                    auto& comp = registry.emplace<SkyBoxComponent>(entity);
                    comp.deserialize(ar);
                }
                else if (componentType == "LightComponent") {
                    auto& comp = registry.emplace<LightComponent>(entity);
                    comp.deserialize(ar);
                }
                else if (componentType == "TerrainComponent") {
                    auto& comp = registry.emplace<TerrainComponent>(entity);
                    comp.deserialize(ar);
                }
                else if (componentType == "AtmosphereComponent") {
                    auto& comp = registry.emplace<AtmosphereComponent>(entity);
                    comp.deserialize(ar);
                }

                std::string componentClose;
                ar >> componentClose;
                if (componentClose != "}") return;
            }
        }
    }

}