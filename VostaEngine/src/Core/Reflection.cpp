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

    bool ReflectionSystem::registerGetter(const std::string& typeName,
        std::function<void* (void*, const std::string&)> getter) {
        auto& map = getGetters();
        if (map.find(typeName) != map.end())
            return false;
        map.emplace(typeName, std::move(getter));
        return true;
    }

    bool ReflectionSystem::registerProperties(const std::string& typeName,
        std::function<std::vector<ReflectionProperty>& ()> propFunc) {
        auto& map = getPropertyFuncs();
        if (map.find(typeName) != map.end())
            return false;
        map.emplace(typeName, std::move(propFunc));
        return true;
    }

    void ReflectionSystem::unregisterType(const std::string& typeName) {
        getGetters().erase(typeName);
        getPropertyFuncs().erase(typeName);
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