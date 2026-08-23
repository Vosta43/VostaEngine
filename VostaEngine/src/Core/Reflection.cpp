#include "vepch.h"
#include "Core/Reflection.h"

namespace ve {

    static std::unordered_map<std::string, std::function<void* (void*, const std::string&)>>& getGetters() {
        static std::unordered_map<std::string, std::function<void* (void*, const std::string&)>> map;
        return map;
    }

    static std::unordered_map<std::string, std::function<std::vector<ReflectionProperty>& ()>>& getPropertyFuncs() {
        static std::unordered_map<std::string, std::function<std::vector<ReflectionProperty>& ()>> map;
        return map;
    }

    void ReflectionSystem::registerGetter(const std::string& typeName,
        std::function<void* (void*, const std::string&)> getter) {
        getGetters()[typeName] = std::move(getter);
    }

    void ReflectionSystem::registerProperties(const std::string& typeName,
        std::function<std::vector<ReflectionProperty>& ()> propFunc) {
        getPropertyFuncs()[typeName] = std::move(propFunc);
    }

    void* ReflectionSystem::get(const std::string& typeName, void* obj, const std::string& propName) {
        auto& getters = getGetters();
        auto it = getters.find(typeName);
        if (it != getters.end()) {
            return it->second(obj, propName);
        }
        return nullptr;
    }

    std::vector<std::string> ReflectionSystem::getAllRegisteredTypes() {
        std::vector<std::string> result;
        for (auto& pair : getGetters()) {
            result.push_back(pair.first);
        }
        return result;
    }

    std::vector<ReflectionProperty> ReflectionSystem::getProperties(const std::string& typeName) {
        auto& propFuncs = getPropertyFuncs();
        auto it = propFuncs.find(typeName);
        if (it != propFuncs.end()) {
            return it->second();
        }
        return {};
    }

}