#include "vepch.h"
#include "Network/RepLayout.h"

#include "Core/Types.h"

#include <algorithm>
#include <cstring>

namespace ve {

	namespace {

		template<typename T>
		void rawWrite(BitWriter& w, const void* value) {
			w.writeBytes(value, (uint32_t)sizeof(T));
		}

		template<typename T>
		void rawRead(BitReader& r, void* value) {
			r.readBytes(value, (uint32_t)sizeof(T));
		}

		template<typename T>
		bool rawCompare(const void* a, const void* b) {
			return std::memcmp(a, b, sizeof(T)) == 0;
		}

		template<typename T>
		RepTypeOps rawOps() {
			return RepTypeOps{ (uint32_t)sizeof(T), &rawWrite<T>, &rawRead<T>, &rawCompare<T>, nullptr };
		}

		template<typename T>
		void lerpInterp(void* dst, const void* a, const void* b, float alpha) {
			using Scalar = typename T::value_type;
			const T& va = *static_cast<const T*>(a);
			const T& vb = *static_cast<const T*>(b);
			*static_cast<T*>(dst) = va + (vb - va) * (Scalar)alpha;
		}

		void floatInterp(void* dst, const void* a, const void* b, float alpha) {
			const float va = *static_cast<const float*>(a);
			const float vb = *static_cast<const float*>(b);
			*static_cast<float*>(dst) = va + (vb - va) * alpha;
		}

		// q and -q are the same orientation, so the dot product picks the shorter
		// arc before the spherical blend.
		void quatInterp(void* dst, const void* a, const void* b, float alpha) {
			const glm::quat& qa = *static_cast<const glm::quat*>(a);
			glm::quat qb = *static_cast<const glm::quat*>(b);
			if (glm::dot(qa, qb) < 0.0f)
				qb = -qb;
			*static_cast<glm::quat*>(dst) = glm::slerp(qa, qb, alpha);
		}

		template<typename T>
		RepTypeOps lerpOps() {
			RepTypeOps ops = rawOps<T>();
			ops.interp = &lerpInterp<T>;
			return ops;
		}

		RepTypeOps floatLerpOps() {
			RepTypeOps ops = rawOps<float>();
			ops.interp = &floatInterp;
			return ops;
		}

		void boolWrite(BitWriter& w, const void* value) {
			w.writeBit(*static_cast<const bool*>(value));
		}

		void boolRead(BitReader& r, void* value) {
			*static_cast<bool*>(value) = r.readBit();
		}

		bool boolCompare(const void* a, const void* b) {
			return *static_cast<const bool*>(a) == *static_cast<const bool*>(b);
		}

		void registerBuiltinTypes() {
			RepLayoutRegistry::registerType("bool", { (uint32_t)sizeof(bool), &boolWrite, &boolRead, &boolCompare, nullptr });
			RepLayoutRegistry::registerType("int", rawOps<int32_t>());
			RepLayoutRegistry::registerType("uint32", rawOps<uint32_t>());
			RepLayoutRegistry::registerType("uint32_t", rawOps<uint32_t>());
			RepLayoutRegistry::registerType("float", floatLerpOps());
			RepLayoutRegistry::registerType("ve::Vec3", lerpOps<Vec3>());
			RepLayoutRegistry::registerType("glm::vec2", lerpOps<glm::vec2>());
			RepLayoutRegistry::registerType("glm::vec3", lerpOps<glm::vec3>());
			RepLayoutRegistry::registerType("glm::dvec3", lerpOps<glm::dvec3>());
			RepLayoutRegistry::registerType("glm::vec4", lerpOps<glm::vec4>());
			RepLayoutRegistry::registerType("glm::ivec2", rawOps<glm::ivec2>());
			RepLayoutRegistry::registerType("glm::quat", { (uint32_t)sizeof(glm::quat), &rawWrite<glm::quat>, &rawRead<glm::quat>, &rawCompare<glm::quat>, &quatInterp });
		}

	}

	RepLayoutRegistry& RepLayoutRegistry::get() {
		static RepLayoutRegistry instance;
		return instance;
	}

	uint32_t RepLayoutRegistry::stableClassId(const std::string& className) {
		uint32_t hash = 2166136261u;
		for (unsigned char c : className) {
			hash ^= c;
			hash *= 16777619u;
		}
		return hash;
	}

	void RepLayoutRegistry::registerType(const std::string& typeName, const RepTypeOps& ops) {
		get().m_types.try_emplace(typeName, ops);
	}

	const RepTypeOps* RepLayoutRegistry::findType(const std::string& typeName) const {
		const auto it = m_types.find(typeName);
		return it == m_types.end() ? nullptr : &it->second;
	}

	bool RepLayoutRegistry::buildFromReflection(std::vector<std::string>& errors) {
		errors.clear();
		if (m_frozen) {
			errors.push_back("rep layouts are already frozen");
			return false;
		}

		registerBuiltinTypes();
		m_layouts.clear();
		m_byId.clear();

		for (const std::string& className : ReflectionSystem::getAllRegisteredTypes()) {
			std::vector<RepField> fields;

			for (const ReflectionProperty& p : ReflectionSystem::getProperties(className)) {
				if (!p.replicated) continue;

				const std::string fieldName = p.fieldName.empty() ? p.name : p.fieldName;
				const auto ops = m_types.find(p.typeName);
				if (ops == m_types.end()) {
					std::string message = className + "." + fieldName + ": no replication ops for '" + p.typeName + "'";
					if (p.typeName.rfind("std::", 0) == 0)
						message += " (variable-length types are not supported yet)";
					errors.push_back(std::move(message));
					continue;
				}
				if (ops->second.size == 0 || !ops->second.write || !ops->second.read || !ops->second.compare) {
					errors.push_back(className + "." + fieldName + ": incomplete replication ops for '" + p.typeName + "'");
					continue;
				}
				if (!p.getter) {
					errors.push_back(className + "." + fieldName + ": property has no getter");
					continue;
				}

				RepField field;
				field.fieldName = fieldName;
				field.typeName = p.typeName;
				field.getter = p.getter;
				field.size = ops->second.size;
				field.write = ops->second.write;
				field.read = ops->second.read;
				field.compare = ops->second.compare;
				field.interp = ops->second.interp;
				fields.push_back(std::move(field));
			}

			if (fields.empty()) continue;

			std::sort(fields.begin(), fields.end(),
				[](const RepField& a, const RepField& b) { return a.fieldName < b.fieldName; });

			for (size_t i = 1; i < fields.size(); ++i) {
				if (fields[i].fieldName == fields[i - 1].fieldName)
					errors.push_back(className + ": duplicate replicated field '" + fields[i].fieldName + "'");
			}

			RepLayout layout;
			layout.className = className;
			layout.classId = stableClassId(className);
			layout.fields = std::move(fields);

			const uint32_t classId = layout.classId;
			auto [it, inserted] = m_layouts.emplace(className, std::move(layout));
			if (!inserted) continue;

			const auto existing = m_byId.find(classId);
			if (existing != m_byId.end()) {
				errors.push_back("class id collision: '" + className + "' and '" + existing->second->className + "'");
				continue;
			}
			m_byId.emplace(classId, &it->second);
		}

		if (!errors.empty()) {
			m_layouts.clear();
			m_byId.clear();
			return false;
		}

		m_frozen = true;
		return true;
	}

	bool RepLayoutRegistry::rebuild(std::vector<std::string>& errors) {
		reset();
		return buildFromReflection(errors);
	}

	void RepLayoutRegistry::reset() {
		m_layouts.clear();
		m_byId.clear();
		m_frozen = false;
	}

	const RepLayout* RepLayoutRegistry::find(const std::string& className) const {
		const auto it = m_layouts.find(className);
		return it == m_layouts.end() ? nullptr : &it->second;
	}

	const RepLayout* RepLayoutRegistry::find(uint32_t classId) const {
		const auto it = m_byId.find(classId);
		return it == m_byId.end() ? nullptr : it->second;
	}

}
