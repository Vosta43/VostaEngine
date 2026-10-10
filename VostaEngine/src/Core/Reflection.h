#pragma once

#include "Core.h"
#include "Log.h"

#include <cctype>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace ve {


	struct ReflectionProperty {
		std::string name;
		std::string typeName; // raw type : int/std::string/float/Handle(Customized val)
		void* offset; //deprecated
		std::function<void*(void*)> getter;
		
		bool enableEdit = true;
		// none/drag/input (Implement 3 only currently)
		// META FORMAT:
		// EXAMPLE: VEPROPERTY(TransformComponent,glm::mat4,transform,"transform","type=drag,minValue=5,maxValue=10")
		// EXAMPLE: VEPROPERTY(TransformComponent,glm::mat4,transform,"transform","type=drag")
		
		std::string meta;
		std::string uiType;
		std::string text;
		float minValue;
		float maxValue;
		float speed;   // custom drag step; 0 = derive from (max-min)*0.01
		std::string format; // DragFloat display/typing format, e.g. "%.6f"; empty = "%.3f"
		std::vector<std::string> options;
	};

	inline float parseMetaFloat(const std::string& s, float fallback = 0.0f) {
		try {
			size_t idx = 0;
			float v = std::stof(s, &idx);
			return (idx == 0) ? fallback : v;
		} catch (...) {
			return fallback;
		}
	}

	inline void parseMeta(const std::string& meta, ReflectionProperty& rp) {
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

	class VE_API ReflectionSystem {
	public:
		static void* get(const std::string& typeName, void* obj, const std::string& propName);
		static std::vector<ReflectionProperty> getProperties(const std::string& typeName);

		template<typename T>
		static void set(const std::string& typeName, void* obj, const std::string& propName, const T& value) {
			void* ptr = get(typeName, obj, propName);
			if (ptr) *static_cast<T*>(ptr) = value;
		}

		// Both return true only when the type was absent and this call inserted it.
		// An existing registration is never clobbered: a module that includes an
		// engine header must not replace the engine's own reflection (which lives
		// in the always-loaded engine DLL) with its own copy.
		static bool registerGetter(const std::string& typeName,
			std::function<void* (void*, const std::string&)> getter);

		static bool registerProperties(const std::string& typeName,
			std::function<std::vector<ReflectionProperty>& ()> propFunc);

		// Removes a type's getter and properties. Called from VEREGISTER's object
		// destructor so a module's own types leave the tables before its DLL is
		// freed -- otherwise the stored lambdas would point into unmapped code.
		static void unregisterType(const std::string& typeName);

		static std::vector<std::string> getAllRegisteredTypes();
	};

	template<typename T>
	struct TypeName {
		static const char* get() { return typeid(T).name(); }
	};



#define VEREGISTER(className) \
    static struct __globalReg_##className { \
        bool m_owned = false; \
        __globalReg_##className() { \
            const bool getter = ve::ReflectionSystem::registerGetter(#className, \
                [](void* obj, const std::string& name) -> void* { \
                    return className##Reflection::get(obj, name); \
                }); \
            const bool props = ve::ReflectionSystem::registerProperties(#className, \
                []() -> std::vector<ve::ReflectionProperty>& { \
                    return className##Reflection::getProperties(); \
                }); \
            m_owned = getter && props; \
        } \
        ~__globalReg_##className() { \
            if (m_owned) \
                ve::ReflectionSystem::unregisterType(#className); \
        } \
    } __globalReg_instance_##className;


#define VECLASS(className) \
	struct className##Reflection{ \
		static std::vector<ReflectionProperty>& getProperties(){ \
			static std::vector<ReflectionProperty> properties; \
			return properties; \
		} \
		static void* get(void* obj,const std::string& name){ \
			for(auto& p : getProperties()){ \
				if(p.name == name)return p.getter(obj);\
			} \
			return nullptr; \
		} \
		template<typename T> \
		static void set(void* obj, const std::string& name, const T& value) { \
			void* ptr = get(obj, name); \
			if (ptr) *static_cast<T*>(ptr) = value; \
		} \
		static void addProperty(const ReflectionProperty& rp) { \
			auto& props = getProperties(); \
			for (const auto& p : props) \
				if (p.name == rp.name) return; \
			props.push_back(rp); \
		} \
	}; \
	VEREGISTER(className) \
	class className;

#define VEPROPERTY(className, propertyType, propertyName, displayName, metaStr) \
    static inline struct autoRegister##propertyName { \
        autoRegister##propertyName() { \
            ReflectionProperty rp; \
            rp.name = displayName; \
			rp.typeName = #propertyType; \
            rp.meta = metaStr; \
            parseMeta(metaStr, rp); \
            rp.getter = [](void* obj) -> void* { \
                return &(static_cast<className*>(obj)->propertyName); \
            }; \
            className##Reflection::addProperty(rp); \
        } \
    } auto_##propertyName;

#define VESTRUCT(className) \
	struct className##Reflection{ \
		static std::vector<ReflectionProperty>& getProperties(){ \
			static std::vector<ReflectionProperty> properties; \
			return properties; \
		} \
		static void* get(void* obj,const std::string& name){ \
			for(auto& p : getProperties()){ \
				if(p.name == name)return p.getter(obj);\
			} \
			return nullptr; \
		} \
		template<typename T> \
		static void set(void* obj, const std::string& name, const T& value) { \
			void* ptr = get(obj, name); \
			if (ptr) *static_cast<T*>(ptr) = value; \
		} \
		static void addProperty(const ReflectionProperty& rp) { \
			auto& props = getProperties(); \
			for (const auto& p : props) \
				if (p.name == rp.name) return; \
			props.push_back(rp); \
		} \
	}; \
	VEREGISTER(className) \
	struct className;

	struct Method {
		std::string name;
		std::function<void*(void*,const std::vector<void*>&)> invoker;
	};

	// Helper: take one argument from args and cast it back to its real type
	template<typename T>
	T& unpackArg(const std::vector<void*>& args, size_t& index) {
		return *static_cast<T*>(args[index++]);
	}

	//Return value,belongs to class,and args
	template<typename Ret,typename Class,typename... Args>
	struct FunctionCaller {

		//Ret (Class::*func)(Args...)  EX: int Player::addDamage(int damage,float missChance)
		static void* call(void* obj,const std::vector<void*>& args,Ret (Class::*func)(Args...)) {
			auto* self = static_cast<Class*>(obj);
			size_t idx = 0;

			if constexpr (std::is_void_v<Ret>) {
				(self->*func)(unpackArg<Args>(args,idx)...);
				return nullptr;
			}
			else {
				Ret result = (self->*func)(unpackArg<Args>(args, idx)...);
				return new Ret(result);
			}

		}
	};
#define VEFUNCTION(className,funcType,funcName,...) \
	static inline struct autoReg##className##funcName{ \
		autoReg##className##funcName(){ \
			Method info;\
			info.name = #funcName; \
			info.invoker = [](void* obj,const std::vector<void*>& args) -> void*{ \
				FunctionCaller<funcType, className, __VA_ARGS__>::call(obj, args, &className::funcName); \
			}; \
			className##Reflection::addMethod(info); \
		} \
	}reg##className##funcName;
}






