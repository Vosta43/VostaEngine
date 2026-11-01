#include "vepch.h"
#include "Core/Json.h"
#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>

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

	void JsonWriter::set(const char* key, const glm::dvec3& v) {
		(*m_impl->stack.back())[key] = nlohmann::json::array({ v.x, v.y, v.z });
	}

	void JsonWriter::set(const char* key, const glm::quat& q) {
		(*m_impl->stack.back())[key] = nlohmann::json::array({ q.x, q.y, q.z, q.w });
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

		// Same contract as readFloats, but reads through double so an authoritative
		// world position keeps its full precision.
		template<size_t N>
		bool readDoubles(const nlohmann::json* v, double(&out)[N]) {
			if (!v || !v->is_array() || v->size() != N)
				return false;
			for (size_t i = 0; i < N; ++i) {
				const nlohmann::json& e = (*v)[i];
				if (!e.is_number())
					return false;
				out[i] = e.get<double>();
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

	glm::dvec3 JsonReader::getVec3d(const char* key, const glm::dvec3& def) const {
		double v[3];
		if (!readDoubles(findMember(m_impl ? m_impl->node : nullptr, key), v))
			return def;
		return glm::dvec3(v[0], v[1], v[2]);
	}

	glm::quat JsonReader::getQuat(const char* key, const glm::quat& def) const {
		double v[4];
		if (!readDoubles(findMember(m_impl ? m_impl->node : nullptr, key), v))
			return def;
		return glm::quat((float)v[3], (float)v[0], (float)v[1], (float)v[2]); // stored x,y,z,w
	}

	std::string JsonReader::rawJson(const char* key, const std::string& def) const {
		const nlohmann::json* v = findMember(m_impl ? m_impl->node : nullptr, key);
		if (!v)
			return def;
		return v->dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
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

	// ── JsonNode / JsonDocument ─────────────────────────────────────────────

	using OrderedJson = nlohmann::ordered_json;

	struct JsonDocument::Impl {
		OrderedJson root = OrderedJson::object();
		std::string path;
		bool     dirty = false;
		bool     valid = false;
		// Bumped by every mutation; node caches resolve against it.
		uint64_t revision = 1;
	};

	namespace {

		// One hop from a container to a child: either an object key or an array
		// index. A node is the path from the root, never a pointer, so a sibling
		// edit that reallocates cannot leave it dangling.
		struct PathStep {
			bool        isIndex = false;
			std::string key;
			size_t      index = 0;
		};

		OrderedJson emptyValue(JsonKind kind) {
			switch (kind) {
			case JsonKind::Bool:    return false;
			case JsonKind::Integer: return static_cast<int64_t>(0);
			case JsonKind::Float:   return 0.0;
			case JsonKind::String:  return std::string();
			case JsonKind::Object:  return OrderedJson::object();
			case JsonKind::Array:   return OrderedJson::array();
			default:                return nullptr;
			}
		}

		std::string escapedKey(const std::string& key) {
			std::string out;
			out.reserve(key.size());
			for (char c : key) {
				if (c == '"' || c == '\\')
					out.push_back('\\');
				out.push_back(c);
			}
			return out;
		}

		bool isBlank(const std::string& text) {
			for (unsigned char c : text)
				if (!std::isspace(c))
					return false;
			return true;
		}

		JsonKind kindOf(const OrderedJson& v) {
			switch (v.type()) {
			case OrderedJson::value_t::boolean:        return JsonKind::Bool;
			case OrderedJson::value_t::number_integer: return JsonKind::Integer;
			case OrderedJson::value_t::number_unsigned:return JsonKind::Integer;
			case OrderedJson::value_t::number_float:   return JsonKind::Float;
			case OrderedJson::value_t::string:         return JsonKind::String;
			case OrderedJson::value_t::array:          return JsonKind::Array;
			case OrderedJson::value_t::object:         return JsonKind::Object;
			default:                                   return JsonKind::Null;
			}
		}

	} // namespace

	struct JsonNode::Impl {
		std::shared_ptr<JsonDocument::Impl> doc;
		std::vector<PathStep> steps;

		mutable const OrderedJson* cache = nullptr;
		mutable uint64_t cacheRevision = 0;

		const OrderedJson* resolve() const {
			if (!doc)
				return nullptr;
			if (cache && cacheRevision == doc->revision)
				return cache;

			const OrderedJson* node = &doc->root;
			for (const PathStep& step : steps) {
				if (step.isIndex) {
					if (!node->is_array() || step.index >= node->size())
						return nullptr;
					node = &(*node)[step.index];
				}
				else {
					if (!node->is_object())
						return nullptr;
					auto it = node->find(step.key);
					if (it == node->end())
						return nullptr;
					node = &(*it);
				}
			}

			cache = node;
			cacheRevision = doc->revision;
			return node;
		}

		// Safe because the resolved pointer always comes from the document's
		// non-const root.
		OrderedJson* resolveMut() {
			return const_cast<OrderedJson*>(resolve());
		}

		void bump() {
			doc->dirty = true;
			++doc->revision;
			cache = nullptr;
			cacheRevision = 0;
		}
	};

	JsonNode::JsonNode() = default;

	bool JsonNode::valid() const {
		return m_impl && m_impl->doc && m_impl->resolve() != nullptr;
	}

	JsonKind JsonNode::kind() const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		return node ? kindOf(*node) : JsonKind::Null;
	}

	size_t JsonNode::childCount() const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		if (!node || !(node->is_object() || node->is_array()))
			return 0;
		return node->size();
	}

	std::string JsonNode::keyAt(size_t index) const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		if (!node || !node->is_object() || index >= node->size())
			return std::string();
		auto it = node->begin();
		std::advance(it, static_cast<ptrdiff_t>(index));
		return it.key();
	}

	JsonNode JsonNode::childAt(size_t index) const {
		JsonNode out;
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		if (!node || index >= node->size())
			return out;

		PathStep step;
		if (node->is_array()) {
			step.isIndex = true;
			step.index = index;
		}
		else if (node->is_object()) {
			step.key = keyAt(index);
		}
		else {
			return out;
		}

		out.m_impl = std::make_shared<Impl>();
		out.m_impl->doc = m_impl->doc;
		out.m_impl->steps = m_impl->steps;
		out.m_impl->steps.push_back(step);
		return out;
	}

	std::string JsonNode::path() const {
		std::string out = "$";
		if (!m_impl)
			return out;
		for (const PathStep& step : m_impl->steps) {
			if (step.isIndex) {
				out += '[';
				out += std::to_string(step.index);
				out += ']';
			}
			else {
				out += "[\"";
				out += escapedKey(step.key);
				out += "\"]";
			}
		}
		return out;
	}

	std::string JsonNode::asString(const std::string& def) const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		return (node && node->is_string()) ? node->get<std::string>() : def;
	}

	int64_t JsonNode::asInt64(int64_t def) const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		if (!node)
			return def;
		if (node->is_number_integer())
			return node->get<int64_t>();
		// Above INT64_MAX the value has no int64 reading; the caller falls back
		// to asRawText() rather than being handed a truncated number.
		if (node->is_number_unsigned()) {
			const uint64_t u = node->get<uint64_t>();
			if (u > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
				return def;
			return static_cast<int64_t>(u);
		}
		if (node->is_number_float())
			return static_cast<int64_t>(node->get<double>());
		return def;
	}

	double JsonNode::asDouble(double def) const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		return (node && node->is_number()) ? node->get<double>() : def;
	}

	bool JsonNode::asBool(bool def) const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		return (node && node->is_boolean()) ? node->get<bool>() : def;
	}

	std::string JsonNode::asRawText() const {
		const OrderedJson* node = m_impl ? m_impl->resolve() : nullptr;
		if (!node)
			return std::string();
		try {
			return node->dump(-1, ' ', false, OrderedJson::error_handler_t::replace);
		}
		catch (const std::exception&) {
			return std::string();
		}
	}

	bool JsonNode::setString(const std::string& v) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node)
			return false;
		*node = v;
		m_impl->bump();
		return true;
	}

	bool JsonNode::setInteger(int64_t v) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node)
			return false;
		*node = v;
		m_impl->bump();
		return true;
	}

	bool JsonNode::setFloat(double v) {
		if (!std::isfinite(v))
			return false;
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node)
			return false;
		*node = v;
		m_impl->bump();
		return true;
	}

	bool JsonNode::setBool(bool v) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node)
			return false;
		*node = v;
		m_impl->bump();
		return true;
	}

	bool JsonNode::setNull() {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node)
			return false;
		*node = nullptr;
		m_impl->bump();
		return true;
	}

	bool JsonNode::setKind(JsonKind kind) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node)
			return false;
		*node = emptyValue(kind);
		m_impl->bump();
		return true;
	}

	bool JsonNode::appendMember(const std::string& key, JsonKind kind) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node || !node->is_object() || node->contains(key))
			return false;
		// ordered_json objects append in insertion order.
		(*node)[key] = emptyValue(kind);
		m_impl->bump();
		return true;
	}

	bool JsonNode::appendElement(JsonKind kind) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node || !node->is_array())
			return false;
		node->push_back(emptyValue(kind));
		m_impl->bump();
		return true;
	}

	bool JsonNode::removeChild(size_t index) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node || index >= node->size())
			return false;

		if (node->is_array()) {
			auto& arr = node->get_ref<OrderedJson::array_t&>();
			arr.erase(arr.begin() + static_cast<ptrdiff_t>(index));
		}
		else if (node->is_object()) {
			auto& obj = node->get_ref<OrderedJson::object_t&>();
			obj.erase(std::next(obj.begin(), static_cast<ptrdiff_t>(index)));
		}
		else {
			return false;
		}

		m_impl->bump();
		return true;
	}

	bool JsonNode::renameKey(size_t index, const std::string& newKey) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node || !node->is_object() || index >= node->size())
			return false;

		auto& obj = node->get_ref<OrderedJson::object_t&>();
		auto target = std::next(obj.begin(), static_cast<ptrdiff_t>(index));
		if (target->first == newKey)
			return true;

		// The key is taken by another member; refuse rather than overwrite.
		auto existing = obj.find(newKey);
		if (existing != obj.end())
			return false;

		// ordered_json keys are const, so the pair cannot be moved in place:
		// rebuild the object with the renamed pair at the same position.
		OrderedJson rebuilt = OrderedJson::object();
		auto& out = rebuilt.get_ref<OrderedJson::object_t&>();
		out.reserve(obj.size());
		for (size_t i = 0; i < obj.size(); ++i) {
			auto cur = std::next(obj.begin(), static_cast<ptrdiff_t>(i));
			out.push_back({ i == index ? newKey : cur->first, cur->second });
		}

		*node = std::move(rebuilt);
		m_impl->bump();
		return true;
	}

	bool JsonNode::moveChild(size_t from, size_t to) {
		OrderedJson* node = m_impl ? m_impl->resolveMut() : nullptr;
		if (!node || from >= node->size() || to >= node->size())
			return false;
		if (from == to)
			return true;

		if (node->is_array()) {
			auto& arr = node->get_ref<OrderedJson::array_t&>();
			if (from < to)
				std::rotate(arr.begin() + static_cast<ptrdiff_t>(from),
							arr.begin() + static_cast<ptrdiff_t>(from + 1),
							arr.begin() + static_cast<ptrdiff_t>(to + 1));
			else
				std::rotate(arr.begin() + static_cast<ptrdiff_t>(to),
							arr.begin() + static_cast<ptrdiff_t>(from),
							arr.begin() + static_cast<ptrdiff_t>(from + 1));
		}
		else if (node->is_object()) {
			auto& obj = node->get_ref<OrderedJson::object_t&>();
			const auto moved = *std::next(obj.begin(), static_cast<ptrdiff_t>(from));

			// Rebuild in the target order; keys are const so the pair cannot be
			// moved in place. Insertion order is the document order.
			OrderedJson rebuilt = OrderedJson::object();
			auto& out = rebuilt.get_ref<OrderedJson::object_t&>();
			out.reserve(obj.size());
			for (size_t i = 0; i < obj.size(); ++i) {
				if (i == from)
					continue;
				if (from > to && i == to)
					out.push_back(moved);
				out.push_back(*std::next(obj.begin(), static_cast<ptrdiff_t>(i)));
				if (from < to && i == to)
					out.push_back(moved);
			}

			*node = std::move(rebuilt);
		}
		else {
			return false;
		}

		m_impl->bump();
		return true;
	}

	JsonDocument::JsonDocument()
		: m_impl(std::make_shared<Impl>()) {
	}

	JsonDocument::~JsonDocument() = default;

	bool JsonDocument::load(const std::string& path, std::string* error) {
		// Drop whatever was loaded before: after a failure this document must be
		// invalid, not a stale copy of the previous file.
		m_impl = std::make_shared<Impl>();

		std::ifstream in(path, std::ios::binary);
		if (!in.is_open()) {
			if (error)
				*error = "cannot open " + path;
			return false;
		}

		const std::string text((std::istreambuf_iterator<char>(in)),
							   std::istreambuf_iterator<char>());
		if (!parseText(text, error))
			return false;

		m_impl->path = path;
		return true;
	}

	bool JsonDocument::parseText(const std::string& text, std::string* error) {
		m_impl = std::make_shared<Impl>();

		// The parser rejects a byte-order mark; strip it so a UTF-8 BOM file
		// opens instead of failing with a confusing line-1 error.
		std::string body = text;
		if (body.size() >= 3
			&& static_cast<unsigned char>(body[0]) == 0xEF
			&& static_cast<unsigned char>(body[1]) == 0xBB
			&& static_cast<unsigned char>(body[2]) == 0xBF)
			body.erase(0, 3);

		// A new, empty file is an empty object, not a parse error.
		if (isBlank(body)) {
			m_impl->valid = true;
			return true;
		}

		try {
			// ordered_json: insertion order is the document's order, and the
			// number's integer/float flavour survives a round trip.
			m_impl->root = OrderedJson::parse(body);
		}
		catch (const std::exception& e) {
			// parse_error's message carries "at line L, column C".
			if (error)
				*error = e.what();
			m_impl->valid = false;
			return false;
		}

		m_impl->valid = true;
		m_impl->dirty = false;
		return true;
	}

	bool JsonDocument::save(std::string* error) {
		if (!m_impl || !m_impl->valid) {
			if (error)
				*error = "no valid document to save";
			return false;
		}
		if (m_impl->path.empty()) {
			if (error)
				*error = "document has no path";
			return false;
		}

		try {
			const std::string text = m_impl->root.dump(2, ' ', false,
				OrderedJson::error_handler_t::replace);

			std::ofstream out(m_impl->path, std::ios::binary | std::ios::trunc);
			if (!out.is_open()) {
				if (error)
					*error = "cannot open " + m_impl->path + " for writing";
				return false;
			}
			out << text;
			out.flush();
			if (!out.good()) {
				if (error)
					*error = "write failed for " + m_impl->path;
				return false;
			}
		}
		catch (const std::exception& e) {
			if (error)
				*error = e.what();
			return false;
		}

		m_impl->dirty = false;
		return true;
	}

	bool JsonDocument::valid() const {
		return m_impl && m_impl->valid;
	}

	bool JsonDocument::dirty() const {
		return m_impl && m_impl->dirty;
	}

	const std::string& JsonDocument::path() const {
		static const std::string kEmpty;
		return m_impl ? m_impl->path : kEmpty;
	}

	JsonNode JsonDocument::root() const {
		JsonNode out;
		if (!m_impl || !m_impl->valid)
			return out;
		out.m_impl = std::make_shared<JsonNode::Impl>();
		out.m_impl->doc = m_impl;
		return out;
	}

}
