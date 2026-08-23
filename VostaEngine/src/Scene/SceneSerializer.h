#pragma once

#include "Scene.h"
#include <unordered_map>
#include <functional>
#include <string>
#include <typeindex>

#include "Archive.h"

namespace ve {
	
	class VE_API SceneSerializer {
	public:
		SceneSerializer(const Ref<Scene>& scene):
            m_scene(scene) {};

        void serialize(Archive& ar);
        void deserialize(Archive& ar);

        bool saveToFile(const std::string& filepath);
        bool loadFromFile(const std::string& filepath);

        template<typename T>
        void registerComponent(const std::string& typeName) {

            m_saveFuncs[typeName] = [](const void* comp, Archive& ar) {
                ((const T*)comp)->Serialize(ar);
                };

            m_loadFuncs[typeName] = [](void* comp, Archive& ar) {
                ((T*)comp)->Deserialize(ar);
                };

            m_createFuncs[typeName] = []() -> void* {
                return new T();
                };
        }

	private:
		Ref<Scene> m_scene;

		std::unordered_map<std::string, std::function<void(const void*, Archive&)>> m_saveFuncs;
		std::unordered_map<std::string, std::function<void(void*, Archive&)>> m_loadFuncs;
		std::unordered_map<std::string, std::function<void* ()>> m_createFuncs;
	};

}