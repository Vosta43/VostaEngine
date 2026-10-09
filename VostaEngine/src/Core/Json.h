#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include <glm.hpp>

#include "Core/Core.h"

namespace ve {

	// Asymmetric JSON facade. Only Json.cpp sees the underlying library; nothing
	// else in the engine includes it. Write with JsonWriter (build up), read with
	// JsonReader (typed getters that always take a default).
	//
	// Every getter on JsonReader is total: a missing key, a wrong-typed value or a
	// malformed document yields the caller's default instead of failing. Callers
	// that need to distinguish "absent" from "present" use has().

	class VE_API JsonWriter {
	public:
		JsonWriter();
		~JsonWriter();

		JsonWriter(JsonWriter&&) noexcept;
		JsonWriter& operator=(JsonWriter&&) noexcept;

		// Begin a named object/array. beginObject() with no key appends an element
		// to the enclosing array. `count` is a reserve hint for beginArray.
		void beginObject(const char* key = nullptr);
		void beginArray(const char* key, size_t count);
		void end();

		void set(const char* key, float v);
		void set(const char* key, int v);
		void set(const char* key, bool v);
		void set(const char* key, const char* v);
		void set(const char* key, const std::string& v);
		void set(const char* key, const glm::vec2& v);
		void set(const char* key, const glm::vec3& v);
		void set(const char* key, const glm::vec4& v);
		void set(const char* key, const glm::mat4& v);

		// Embed an already-serialised JSON value at `key`. For opaque blobs built
		// outside this writer (a tool's JSON-Schema string). Invalid text becomes {}
		// rather than throwing or corrupting the document.
		void setRaw(const char* key, const std::string& rawJson);

		bool writeToFile(const std::string& path) const;
		std::string str() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	class VE_API JsonReader {
	public:
		// False when the file is missing or is not valid JSON. On failure `out`
		// stays invalid and every getter returns its default.
		static bool load(const std::string& path, JsonReader& out);

		// Counterpart to load() for JSON that is already in memory, e.g. tool
		// arguments handed over as a string. Same contract: on failure `out`
		// stays invalid and every getter returns its default.
		static bool parse(const std::string& text, JsonReader& out);

		bool valid() const;
		bool has(const char* key) const;

		float       getFloat(const char* key, float def = 0.0f) const;
		int         getInt(const char* key, int def = 0) const;
		bool        getBool(const char* key, bool def = false) const;
		std::string getString(const char* key, const std::string& def = std::string()) const;
		glm::vec2   getVec2(const char* key, const glm::vec2& def = glm::vec2(0.0f)) const;
		glm::vec3   getVec3(const char* key, const glm::vec3& def = glm::vec3(0.0f)) const;
		glm::vec4   getVec4(const char* key, const glm::vec4& def = glm::vec4(0.0f)) const;
		glm::mat4   getMat4(const char* key, const glm::mat4& def = glm::mat4(1.0f)) const;

		size_t arraySize(const char* key) const;
		JsonReader child(const char* key) const;
		JsonReader at(const char* key, size_t index) const;

		// Element count of THIS node when it is an array (0 otherwise). Lets a
		// caller walk a bare array -- pass order, a history pair, a setting's
		// discrete values -- whose elements are scalars, not objects.
		size_t size() const;
		// Element `index` of THIS node as a view. Invalid (all defaults) when this
		// node is not an array or the index is out of range.
		JsonReader atIndex(size_t index) const;
		// Scalars read off THIS node rather than a member of it. Total, like the
		// rest: a wrong-typed or absent node yields the default.
		float       asFloat(float def = 0.0f) const;
		int         asInt(int def = 0) const;
		bool        asBool(bool def = false) const;
		std::string asString(const std::string& def = std::string()) const;

	private:
		struct Impl;
		std::shared_ptr<Impl> m_impl;
	};

}
