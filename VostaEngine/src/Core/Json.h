#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <glm.hpp>
#include <gtc/quaternion.hpp>

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

		// Appends a scalar element to the enclosing array -- the key-less
		// counterpart of beginObject(). The enclosing container must be an array.
		void push(const std::string& v);

		void set(const char* key, float v);
		void set(const char* key, int v);
		void set(const char* key, bool v);
		void set(const char* key, const char* v);
		void set(const char* key, const std::string& v);
		void set(const char* key, const glm::vec2& v);
		void set(const char* key, const glm::vec3& v);
		void set(const char* key, const glm::vec4& v);
		void set(const char* key, const glm::mat4& v);
		// Written as doubles so an authoritative world position round-trips exactly.
		void set(const char* key, const glm::dvec3& v);
		void set(const char* key, const glm::quat& q);

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
		glm::dvec3  getVec3d(const char* key, const glm::dvec3& def = glm::dvec3(0.0)) const;
		glm::quat   getQuat(const char* key, const glm::quat& def = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) const;

		// The subtree at `key` re-serialised to JSON text, or `def` when absent.
		// Lets a caller carry an opaque sub-object (a widget's properties) around
		// as text without knowing its shape.
		std::string rawJson(const char* key, const std::string& def = std::string()) const;

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

	// Generic document tree, for tools that edit JSON whose shape they do not
	// know (the editor's JSON panel). Unlike JsonReader, any root type is valid,
	// key order is preserved (insertion order), and integers stay integers.
	// Nodes are handles: they address a position from the root, so a structural
	// edit elsewhere can never leave one dangling -- it resolves to the node now
	// at that position, or to an invalid node when the position is gone.

	enum class JsonKind : uint8_t { Null, Bool, Integer, Float, String, Object, Array };

	class JsonDocument;

	class VE_API JsonNode {
	public:
		JsonNode();

		bool     valid() const;
		JsonKind kind() const;

		// Children of an object or array, in document order. An object child's
		// key comes from keyAt(); an array child's key is empty.
		size_t      childCount() const;
		std::string keyAt(size_t index) const;
		JsonNode    childAt(size_t index) const;

		// Opaque identity for this position, e.g. $["window"][0]. Stable across
		// frames while the shape above it does not change; used as a UI id.
		std::string path() const;

		std::string asString(const std::string& def = std::string()) const;
		int64_t     asInt64(int64_t def = 0) const;
		double      asDouble(double def = 0.0) const;
		bool        asBool(bool def = false) const;
		// Compact JSON text of this node. For a value too large for int64, the
		// only lossless reading.
		std::string asRawText() const;

		bool setString(const std::string& v);
		bool setInteger(int64_t v);
		bool setFloat(double v);   // false for NaN/Inf
		bool setBool(bool v);
		bool setNull();
		// Replaces the value with the empty value of `kind` ({}, [], "", 0, ...).
		bool setKind(JsonKind kind);

		bool appendMember(const std::string& key, JsonKind kind); // unique key
		bool appendElement(JsonKind kind);
		bool removeChild(size_t index);
		bool renameKey(size_t index, const std::string& newKey);  // keeps position
		bool moveChild(size_t from, size_t to);                   // keeps in-bounds

	private:
		friend class JsonDocument;
		struct Impl;
		std::shared_ptr<Impl> m_impl;
	};

	class VE_API JsonDocument {
	public:
		JsonDocument();
		~JsonDocument();

		JsonDocument(const JsonDocument&) = delete;
		JsonDocument& operator=(const JsonDocument&) = delete;

		// A missing file or a parse error leaves this document invalid and, when
		// `error` is given, puts the parser's line/column message there. An empty
		// or whitespace-only file loads as an empty object, not as an error.
		bool load(const std::string& path, std::string* error = nullptr);
		bool parseText(const std::string& text, std::string* error = nullptr);
		// Writes the loaded path with 2-space indentation; clears dirty().
		bool save(std::string* error = nullptr);

		bool valid() const;
		bool dirty() const;
		const std::string& path() const;
		JsonNode root() const;

	private:
		friend class JsonNode;
		struct Impl;
		std::shared_ptr<Impl> m_impl;
	};

}
