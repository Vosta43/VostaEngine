#include "vepch.h"
#include "Core/Json.h"
#include "Core/Log.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <gtc/type_ptr.hpp>

namespace ve {

	// ── JsonWriter ──────────────────────────────────────────────────────────

	struct JsonWriter::Impl {
		nlohmann::json root = nlohmann::json::object();
		// Open containers, innermost last. The root object is always stack[0].
		std::vector<nlohmann::json*> stack;
	};

	JsonWriter::JsonWriter()
		: m_impl(std::make_unique<Impl>()) {
		m_impl->stack.push_back(&m_impl->root);
	}

	JsonWriter::~JsonWriter() = default;
	JsonWriter::JsonWriter(JsonWriter&&) noexcept = default;
	JsonWriter& JsonWriter::operator=(JsonWriter&&) noexcept = default;

	void JsonWriter::beginObject(const char* key) {
		nlohmann::json& cur = *m_impl->stack.back();

		if (key) {
			cur[key] = nlohmann::json::object();
			m_impl->stack.push_back(&cur[key]);
		}
		else {
			// No key: the enclosing container is an array and this appends an element.
			cur.push_back(nlohmann::json::object());
			m_impl->stack.push_back(&cur.back());
		}
	}

	void JsonWriter::beginArray(const char* key, size_t count) {
		nlohmann::json& cur = *m_impl->stack.back();
		cur[key] = nlohmann::json::array();
		cur[key].get_ref<nlohmann::json::array_t&>().reserve(count);
		m_impl->stack.push_back(&cur[key]);
	}

	void JsonWriter::end() {
		if (m_impl->stack.size() > 1)
			m_impl->stack.pop_back();
	}

	void JsonWriter::set(const char* key, float v)              { (*m_impl->stack.back())[key] = v; }
	void JsonWriter::set(const char* key, int v)                { (*m_impl->stack.back())[key] = v; }
	void JsonWriter::set(const char* key, bool v)               { (*m_impl->stack.back())[key] = v; }
	void JsonWriter::set(const char* key, const char* v)        { (*m_impl->stack.back())[key] = v ? v : ""; }
	void JsonWriter::set(const char* key, const std::string& v) { (*m_impl->stack.back())[key] = v; }

	void JsonWriter::set(const char* key, const glm::vec2& v) {
		(*m_impl->stack.back())[key] = nlohmann::json::array({ v.x, v.y });
	}

	void JsonWriter::set(const char* key, const glm::vec3& v) {
		(*m_impl->stack.back())[key] = nlohmann::json::array({ v.x, v.y, v.z });
	}

	void JsonWriter::set(const char* key, const glm::vec4& v) {
		(*m_impl->stack.back())[key] = nlohmann::json::array({ v.x, v.y, v.z, v.w });
	}

	void JsonWriter::set(const char* key, const glm::mat4& v) {
		// Column-major, matching glm::value_ptr order so make_mat4 reads it back.
		const float* m = glm::value_ptr(v);
		nlohmann::json arr = nlohmann::json::array();
		for (int i = 0; i < 16; ++i)
			arr.push_back(m[i]);
		(*m_impl->stack.back())[key] = std::move(arr);
	}

	void JsonWriter::setRaw(const char* key, const std::string& rawJson) {
		// Non-throwing parse: a malformed blob degrades to an empty object, which
		// callers (e.g. an OpenAI tool schema) can still accept.
		nlohmann::json parsed = nlohmann::json::parse(rawJson, nullptr, false);
		if (parsed.is_discarded())
			parsed = nlohmann::json::object();

		nlohmann::json& cur = *m_impl->stack.back();
		if (key)
			cur[key] = std::move(parsed);
		else
			cur.push_back(std::move(parsed));
	}

	std::string JsonWriter::str() const {
		// "replace" instead of the default "strict": a non-UTF-8 byte in a name or
		// path must not make dump() throw.
		return m_impl->root.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
	}

	bool JsonWriter::writeToFile(const std::string& path) const {
		try {
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			if (!out.is_open()) {
				VE_CORE_ERROR_PRINT("JsonWriter::writeToFile: cannot open '%s'", path.c_str());
				return false;
			}
			out << str();
			out.flush();
			if (!out.good()) {
				VE_CORE_ERROR_PRINT("JsonWriter::writeToFile: write failed for '%s'", path.c_str());
				return false;
			}
			return true;
		}
		catch (const std::exception& e) {
			VE_CORE_ERROR_PRINT("JsonWriter::writeToFile: %s ('%s')", e.what(), path.c_str());
			return false;
		}
	}

	// ── JsonReader ──────────────────────────────────────────────────────────

	struct JsonReader::Impl {
		std::shared_ptr<const nlohmann::json> doc;
		// Current view. Null means "absent": every getter falls back to its default.
		const nlohmann::json* node = nullptr;
	};

	namespace {

		// Member lookup on a pure object-view. Returns null for a missing key, a
		// non-object view or a null view, so callers never have to look at the
		// node themselves.
		const nlohmann::json* findMember(const nlohmann::json* node, const char* key) {
			if (!node || !key || !node->is_object())
				return nullptr;
			auto it = node->find(key);
			return (it != node->end()) ? &(*it) : nullptr;
		}

		// Reads exactly N numbers out of a JSON array. False on any mismatch, so
		// the caller can fall back to its default.
		template<size_t N>
		bool readFloats(const nlohmann::json* v, float (&out)[N]) {
			if (!v || !v->is_array() || v->size() != N)
				return false;
			for (size_t i = 0; i < N; ++i) {
				const nlohmann::json& e = (*v)[i];
				if (!e.is_number())
					return false;
				out[i] = e.get<float>();
			}
			return true;
		}

	} // namespace

	bool JsonReader::load(const std::string& path, JsonReader& out) {
		out.m_impl.reset();
		try {
			std::ifstream in(path, std::ios::binary);
			if (!in.is_open()) {
				VE_CORE_ERROR_PRINT("JsonReader::load: cannot open '%s'", path.c_str());
				return false;
			}

			// allow_exceptions = false: a malformed document yields `discarded`
			// rather than throwing out of the parser.
			nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
			if (doc.is_discarded()) {
				VE_CORE_ERROR_PRINT("JsonReader::load: '%s' is not valid JSON", path.c_str());
				return false;
			}

			out.m_impl = std::make_shared<Impl>();
			out.m_impl->doc = std::make_shared<const nlohmann::json>(std::move(doc));
			out.m_impl->node = out.m_impl->doc.get();
			return true;
		}
		catch (const std::exception& e) {
			VE_CORE_ERROR_PRINT("JsonReader::load: %s ('%s')", e.what(), path.c_str());
			out.m_impl.reset();
			return false;
		}
	}

	bool JsonReader::parse(const std::string& text, JsonReader& out) {
		out.m_impl.reset();
		try {
			// allow_exceptions = false: a malformed document yields `discarded`
			// rather than throwing out of the parser.
			nlohmann::json doc = nlohmann::json::parse(text, nullptr, false);
			if (doc.is_discarded()) {
				VE_CORE_ERROR_PRINT("JsonReader::parse: not valid JSON (%zu bytes)", text.size());
				return false;
			}

			out.m_impl = std::make_shared<Impl>();
			out.m_impl->doc = std::make_shared<const nlohmann::json>(std::move(doc));
			out.m_impl->node = out.m_impl->doc.get();
			return true;
		}
		catch (const std::exception& e) {
			VE_CORE_ERROR_PRINT("JsonReader::parse: %s", e.what());
			out.m_impl.reset();
			return false;
		}
	}

	bool JsonReader::valid() const {
		return m_impl && m_impl->node && m_impl->node->is_object();
	}

	bool JsonReader::has(const char* key) const {
		return findMember(m_impl ? m_impl->node : nullptr, key) != nullptr;
	}

	float JsonReader::getFloat(const char* key, float def) const {
		const nlohmann::json* v = findMember(m_impl ? m_impl->node : nullptr, key);
		return (v && v->is_number()) ? v->get<float>() : def;
	}

	int JsonReader::getInt(const char* key, int def) const {
		const nlohmann::json* v = findMember(m_impl ? m_impl->node : nullptr, key);
		if (!v)
			return def;
		if (v->is_number_integer())
			return v->get<int>();
		if (v->is_number_float())
			return static_cast<int>(v->get<double>());
		return def;
	}

	bool JsonReader::getBool(const char* key, bool def) const {
		const nlohmann::json* v = findMember(m_impl ? m_impl->node : nullptr, key);
		return (v && v->is_boolean()) ? v->get<bool>() : def;
	}

	std::string JsonReader::getString(const char* key, const std::string& def) const {
		const nlohmann::json* v = findMember(m_impl ? m_impl->node : nullptr, key);
		return (v && v->is_string()) ? v->get<std::string>() : def;
	}

	glm::vec2 JsonReader::getVec2(const char* key, const glm::vec2& def) const {
		float v[2];
		if (!readFloats(findMember(m_impl ? m_impl->node : nullptr, key), v))
			return def;
		return glm::vec2(v[0], v[1]);
	}

	glm::vec3 JsonReader::getVec3(const char* key, const glm::vec3& def) const {
		float v[3];
		if (!readFloats(findMember(m_impl ? m_impl->node : nullptr, key), v))
			return def;
		return glm::vec3(v[0], v[1], v[2]);
	}

	glm::vec4 JsonReader::getVec4(const char* key, const glm::vec4& def) const {
		float v[4];
		if (!readFloats(findMember(m_impl ? m_impl->node : nullptr, key), v))
			return def;
		return glm::vec4(v[0], v[1], v[2], v[3]);
	}

	glm::mat4 JsonReader::getMat4(const char* key, const glm::mat4& def) const {
		float v[16];
		if (!readFloats(findMember(m_impl ? m_impl->node : nullptr, key), v))
			return def;
		return glm::make_mat4(v);
	}

	size_t JsonReader::arraySize(const char* key) const {
		const nlohmann::json* v = findMember(m_impl ? m_impl->node : nullptr, key);
		return (v && v->is_array()) ? v->size() : 0;
	}

	JsonReader JsonReader::child(const char* key) const {
		JsonReader out;
		const nlohmann::json* v = findMember(m_impl ? m_impl->node : nullptr, key);
		if (v) {
			out.m_impl = std::make_shared<Impl>();
			out.m_impl->doc = m_impl->doc;
			out.m_impl->node = v;
		}
		return out;
	}

	JsonReader JsonReader::at(const char* key, size_t index) const {
		JsonReader out;
		const nlohmann::json* arr = findMember(m_impl ? m_impl->node : nullptr, key);
		if (arr && arr->is_array() && index < arr->size()) {
			out.m_impl = std::make_shared<Impl>();
			out.m_impl->doc = m_impl->doc;
			out.m_impl->node = &(*arr)[index];
		}
		return out;
	}

	size_t JsonReader::size() const {
		const nlohmann::json* n = m_impl ? m_impl->node : nullptr;
		return (n && n->is_array()) ? n->size() : 0;
	}

	JsonReader JsonReader::atIndex(size_t index) const {
		JsonReader out;
		const nlohmann::json* n = m_impl ? m_impl->node : nullptr;
		if (n && n->is_array() && index < n->size()) {
			out.m_impl = std::make_shared<Impl>();
			out.m_impl->doc = m_impl->doc;
			out.m_impl->node = &(*n)[index];
		}
		return out;
	}

	float JsonReader::asFloat(float def) const {
		const nlohmann::json* n = m_impl ? m_impl->node : nullptr;
		return (n && n->is_number()) ? n->get<float>() : def;
	}

	int JsonReader::asInt(int def) const {
		const nlohmann::json* n = m_impl ? m_impl->node : nullptr;
		if (!n)
			return def;
		if (n->is_number_integer())
			return n->get<int>();
		if (n->is_number_float())
			return static_cast<int>(n->get<double>());
		return def;
	}

	bool JsonReader::asBool(bool def) const {
		const nlohmann::json* n = m_impl ? m_impl->node : nullptr;
		return (n && n->is_boolean()) ? n->get<bool>() : def;
	}

	std::string JsonReader::asString(const std::string& def) const {
		const nlohmann::json* n = m_impl ? m_impl->node : nullptr;
		return (n && n->is_string()) ? n->get<std::string>() : def;
	}

}
