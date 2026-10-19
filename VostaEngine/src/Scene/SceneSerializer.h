#pragma once

#include "Scene.h"
#include "Core/Json.h"

#include <string>

namespace ve {

	class VE_API SceneSerializer {
	public:
		SceneSerializer(const Ref<Scene>& scene):
            m_scene(scene) {};

        void serialize(JsonWriter& w);
        bool deserialize(const JsonReader& r);

        // Incremental load. beginLoad parses the file and resets the entity
        // cursor; loadChunk commits up to maxEntities more and returns true
        // once the scene is complete.
        bool beginLoad(const std::string& filepath);
        bool loadChunk(size_t maxEntities);
        size_t entityCount() const { return m_total; }
        size_t loadedCount() const { return m_cursor; }
        const Ref<Scene>& scene() const { return m_scene; }

        bool saveToFile(const std::string& filepath);
        bool loadFromFile(const std::string& filepath);

	private:
        bool beginDeserialize(const JsonReader& r);

		Ref<Scene> m_scene;
        JsonReader m_reader;
        size_t m_cursor = 0;
        size_t m_total = 0;
	};

}
