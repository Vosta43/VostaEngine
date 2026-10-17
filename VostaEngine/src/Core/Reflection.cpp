#include "vepch.h"
#include "Core/Reflection.h"

#include <cctype>

namespace ve {

    static float parseMetaFloat(const std::string& s, float fallback = 0.0f) {
        try {
            size_t idx = 0;
            float v = std::stof(s, &idx);
            return (idx == 0) ? fallback : v;
        } catch (...) {
            return fallback;
        }
    }

    void parseMeta(const std::string& meta, ReflectionProperty& rp) {
        rp.meta = meta;
        rp.uiType.clear();
        rp.text.clear();
        rp.minValue = 0.0f;
        rp.maxValue = 0.0f;
        rp.speed = 0.0f;
        rp.format.clear();
        rp.enableEdit = true;
        rp.options.clear();

        size_t pos = 0;
        while (pos < meta.size()) {
            while (pos < meta.size() && std::isspace((unsigned char)meta[pos])) ++pos;
            if (pos >= meta.size()) break;

            size_t comma = meta.find(',', pos);
            std::string token = meta.substr(pos,
                comma == std::string::npos ? std::string::npos : comma - pos);
            pos = (comma == std::string::npos) ? meta.size() : comma + 1;

            while (!token.empty() && std::isspace((unsigned char)token.back())) token.pop_back();
            if (token.empty()) continue;

            size_t eq = token.find('=');
            std::string key   = (eq == std::string::npos) ? token : token.substr(0, eq);
            std::string value = (eq == std::string::npos) ? "" : token.substr(eq + 1);

            while (!key.empty() && std::isspace((unsigned char)key.back())) key.pop_back();
            size_t vpos = 0;
            while (vpos < value.size() && std::isspace((unsigned char)value[vpos])) ++vpos;
            value = value.substr(vpos);

            if (key == "type")          rp.uiType = value;
            else if (key == "minValue") rp.minValue = parseMetaFloat(value);
            else if (key == "maxValue") rp.maxValue = parseMetaFloat(value);
            else if (key == "speed")    rp.speed = parseMetaFloat(value);
            else if (key == "format")   rp.format = value;
            else if (key == "text")     rp.text = value;
            else if (key == "readonly") rp.enableEdit = !(value == "true" || value == "1");
            else if (key == "options") {
                size_t s = 0;
                while (s <= value.size()) {
                    size_t bar = value.find('|', s);
                    std::string opt = value.substr(s,
                        bar == std::string::npos ? std::string::npos : bar - s);
                    if (!opt.empty()) rp.options.push_back(opt);
                    if (bar == std::string::npos) break;
                    s = bar + 1;
                }
            }
        }
    }


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