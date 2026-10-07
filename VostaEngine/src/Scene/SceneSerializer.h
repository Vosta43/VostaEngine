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

        bool saveToFile(const std::string& filepath);
        bool loadFromFile(const std::string& filepath);

	private:
		Ref<Scene> m_scene;
	};

}
