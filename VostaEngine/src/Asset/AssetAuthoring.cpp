#include "vepch.h"
#include "Asset/AssetAuthoring.h"

#include "Asset/AssetLibrary.h"
#include "Asset/Utils.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"
#include "Graph/NodeGraph.h"
#include "Noise/NoiseGraph.h"
#include "Noise/NoiseGraphResource.h"
#include "Noise/NoiseNodes.h"
#include "Renderer/LayeredMaterial.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialLayer.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/MaterialNodes.h"
#include "Renderer/SingleMaterial.h"
#include "Renderer/Texture.h"
#include "Scene/Archive.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ve {
	namespace {

		// ── small JSON helpers ─────────────────────────────────────────────

		std::string jsonEscape(const std::string& in) {
			std::string out;
			out.reserve(in.size() + 8);
			for (char c : in) {
				switch (c) {
				case '"':  out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n";  break;
				case '\r': out += "\\r";  break;
				case '\t': out += "\\t";  break;
				default:   out += c;      break;
				}
			}
			return out;
		}

		std::string errorJson(const std::string& message) {
			return "{\"error\":\"" + jsonEscape(message) + "\"}";
		}

		std::string okJson(const std::string& path, const char* kind) {
			return "{\"ok\":true,\"path\":\"" + jsonEscape(path) + "\",\"kind\":\"" + kind + "\"}";
		}

		// Report a write to the asset library when it landed, then pass the reply
		// through. Every writer goes through here, so a view that shows assets learns
		// about a new file without the writer knowing the view exists.
		std::string announceWrite(std::string reply) {
			if (reply.rfind("{\"ok\":true", 0) == 0)
				AssetLibrary::get().notifyChanged();
			return reply;
		}

		// Append a string element to the array currently open on the writer. set()
		// takes a key and would read null as an empty std::string, so go through
		// setRaw, whose null key means "append to the enclosing array".
		void pushString(JsonWriter& w, const std::string& s) {
			w.setRaw(nullptr, "\"" + jsonEscape(s) + "\"");
		}

		// ── paths ──────────────────────────────────────────────────────────

		std::string withVeassetExt(const std::string& path) {
			if (path.size() >= 8 && path.compare(path.size() - 8, 8, ".veasset") == 0)
				return path;
			return path + ".veasset";
		}

		bool isUnder(const std::string& child, const std::string& parent) {
			const std::filesystem::path c = std::filesystem::path(child).lexically_normal();
			const std::filesystem::path p = std::filesystem::path(parent).lexically_normal();
			auto ci = c.begin();
			for (auto pi = p.begin(); pi != p.end(); ++pi, ++ci) {
				if (ci == c.end() || *ci != *pi)
					return false;
			}
			return true;
		}

		// The model may write "content/materials/x" as readily as "materials/x";
		// both mean the same place, so drop the redundant leading folder.
		std::string stripContentPrefix(const std::string& in) {
			const char* prefixes[] = { "content/", "content\\" };
			for (const char* prefix : prefixes) {
				const size_t n = std::strlen(prefix);
				if (in.size() > n && in.compare(0, n, prefix) == 0)
					return in.substr(n);
			}
			return in;
		}

		// Turn whatever the model passed into an absolute file path. Relative paths
		// are read against the project's content folder, except the engine-root
		// form list_assets emits (which already names the content folder). On
		// failure `error` is left non-empty and the return value is unused.
		std::string resolveAssetPath(const std::string& in, bool forWrite, std::string& error) {
			error.clear();
			if (in.empty()) {
				error = "missing 'path'";
				return {};
			}

			if (std::filesystem::path(in).is_absolute())
				return withVeassetExt(in);

			const std::string contentRoot = toProjectAbsolute("content");
			if (contentRoot.empty()) {
				error = "no project is open, so a content-relative path cannot be resolved; "
				        "open a project or pass an absolute path";
				return {};
			}

			const std::string rel = stripContentPrefix(in);
			const std::string fromContent = (std::filesystem::path(contentRoot) / rel).string();
			const std::string fromRoot = toAbsolute(in);

			if (forWrite)
				return withVeassetExt(isUnder(fromRoot, contentRoot) ? fromRoot : fromContent);

			std::error_code ec;
			if (std::filesystem::exists(fromRoot, ec)) return withVeassetExt(fromRoot);
			if (std::filesystem::exists(fromContent, ec)) return withVeassetExt(fromContent);
			return withVeassetExt(fromRoot);   // not found: report it at the path as given
		}

		std::string texturePath(AssetHandle h) {
			return h.isValid() ? ResourceManager::getPath<Texture2D>(h) : std::string();
		}

		// Resolve a texture reference and register it. An empty reference is a
		// no-op so a map can be left unset.
		bool storeTexture(const JsonReader& src, const char* key, AssetHandle& out, std::string& error) {
			error.clear();
			const std::string p = src.getString(key, "");
			if (p.empty())
				return true;

			std::string pathError;
			const std::string abs = resolveAssetPath(p, false, pathError);
			if (!pathError.empty()) { error = pathError; return false; }

			out = ResourceManager::store<Texture2D>(abs);
			if (!out.isValid()) {
				error = "texture '" + p + "' could not be loaded";
				return false;
			}
			return true;
		}

		// ── node-type catalogue ────────────────────────────────────────────

		struct ParamSpec { const char* name; const char* kind; };
		struct NodeSpec {
			const char* name;
			const char* inputs;    // comma-separated, in pin-index order
			const char* outputs;
			std::vector<ParamSpec> params;
		};

		// Index == MaterialNodeTag. Order must stay in step with the enum.
		const std::vector<NodeSpec>& materialSpecs() {
			static const std::vector<NodeSpec> specs = {
				{ "ConstantFloat",     "",              "Float",            { {"value","float"} } },
				{ "Constant2Vector",   "",              "Vec2",             { {"value","vec2"} } },
				{ "Constant3Vector",   "",              "Vec3",             { {"value","vec3"} } },
				{ "Constant4Vector",   "",              "Vec4",             { {"value","vec4"} } },
				{ "TextureCoordinate", "",              "UV",               { {"uvScale","vec2"} } },
				{ "TextureSampler",    "UVs",           "RGBA, R, G, B, A", { {"texture","string (a texture asset path)"} } },
				{ "Multiply",          "A, B",          "Result",           {} },
				{ "Add",               "A, B",          "Result",           {} },
				{ "Subtract",          "A, B",          "Result",           {} },
				{ "Divide",            "A, B",          "Result",           {} },
				{ "Lerp",              "A, B, T",       "Result",           {} },
				{ "Clamp",             "Value, Min, Max", "Result",         {} },
				{ "MaterialOutput",    "Base Color, Metallic, Roughness, Ambient Occlusion, Normal, Emissive, Opacity Mask",
				                       "",              {} },
			};
			return specs;
		}

		// Index == NoiseNodeTag.
		const std::vector<NodeSpec>& noiseSpecs() {
			static const std::vector<NodeSpec> specs = {
				{ "NoiseUnit", "",        "Out",    { {"settings","object (see noise_settings_fields)"} } },
				{ "Add",       "A, B",    "Result", {} },
				{ "Subtract",  "A, B",    "Result", {} },
				{ "Multiply",  "A, B",    "Result", {} },
				{ "Divide",    "A, B",    "Result", {} },
				{ "Output",    "Value",   "",       { {"outputMin","float"}, {"outputMax","float"} } },
				{ "Constant",  "",        "Out",    { {"value","float"} } },
				{ "Blend",     "A, B, T", "Result", {} },
				{ "Min",       "A, B",    "Result", {} },
				{ "Max",       "A, B",    "Result", {} },
				{ "Invert",    "Value",   "Result", {} },
				{ "Clamp",     "Value",   "Result", { {"minValue","float"}, {"maxValue","float"} } },
				{ "Remap",     "Value",   "Result", { {"inMin","float"}, {"inMax","float"}, {"outMin","float"}, {"outMax","float"}, {"clamp","bool"} } },
				{ "Threshold", "Value",   "Result", { {"threshold","float"}, {"falloff","float"} } },
				{ "Abs",       "Value",   "Result", {} },
				{ "Erf",       "Value",   "Result", { {"divisor","float"} } },
				{ "Trunc",     "Value",   "Result", {} },
				{ "Spline",    "Value",   "Result", { {"points","array of {x,y}"} } },
			};
			return specs;
		}

		int tagOfName(const std::vector<NodeSpec>& specs, const std::string& name) {
			for (size_t i = 0; i < specs.size(); ++i)
				if (name == specs[i].name) return static_cast<int>(i);
			return -1;
		}

		const NodeRegistry& registryFor(const std::string& kind) {
			return (kind == "noise") ? getNoiseNodeRegistry() : getMaterialNodeRegistry();
		}
		const std::vector<NodeSpec>& specsFor(const std::string& kind) {
			return (kind == "noise") ? noiseSpecs() : materialSpecs();
		}

		// ── NoiseSettings JSON ─────────────────────────────────────────────

		const char* kNoiseTypeNames[]        = { "OpenSimplex2", "OpenSimplex2S", "Cellular", "Perlin", "ValueCubic", "Value" };
		const char* kFractalNames[]          = { "None", "Fbm", "Ridged", "PingPong" };
		const char* kCellularDistanceNames[] = { "Euclidean", "EuclideanSq", "Manhattan", "Hybrid" };
		const char* kCellularReturnNames[]   = { "CellValue", "Distance", "Distance2", "Distance2Add", "Distance2Sub", "Distance2Mul", "Distance2Div" };
		const char* kDomainWarpNames[]       = { "None", "OpenSimplex2", "OpenSimplex2Reduced", "BasicGrid" };
		const char* kBlendNames[]            = { "WeightBlend", "AlphaBlend", "HeightBlend" };

		int clampIndex(int v, int count) {
			return (v < 0) ? 0 : (v >= count ? count - 1 : v);
		}

		// Accepts an enum as its name or its integer, so a model can use either.
		int enumFromJson(const JsonReader& r, const char* key, const char* const* names, int count, int def) {
			if (!r.has(key))
				return def;
			const std::string s = r.getString(key, "");
			if (!s.empty()) {
				for (int i = 0; i < count; ++i)
					if (s == names[i]) return i;
			}
			return r.getInt(key, def);
		}

		void readNoiseSettings(const JsonReader& r, NoiseSettings& s) {
			s.type = (NoiseSettings::Type)clampIndex(
				enumFromJson(r, "type", kNoiseTypeNames, 6, (int)s.type), 6);
			s.seed = r.getInt("seed", s.seed);
			s.frequency = r.getFloat("frequency", s.frequency);

			s.fractal = (NoiseSettings::Fractal)clampIndex(
				enumFromJson(r, "fractal", kFractalNames, 4, (int)s.fractal), 4);
			s.octaves = r.getInt("octaves", s.octaves);
			s.lacunarity = r.getFloat("lacunarity", s.lacunarity);
			s.gain = r.getFloat("gain", s.gain);
			s.weightedStrength = r.getFloat("weightedStrength", s.weightedStrength);
			s.pingPongStrength = r.getFloat("pingPongStrength", s.pingPongStrength);

			s.cellularDistance = (NoiseSettings::CellularDistance)clampIndex(
				enumFromJson(r, "cellularDistance", kCellularDistanceNames, 4, (int)s.cellularDistance), 4);
			s.cellularReturn = (NoiseSettings::CellularReturn)clampIndex(
				enumFromJson(r, "cellularReturn", kCellularReturnNames, 7, (int)s.cellularReturn), 7);
			s.cellularJitter = r.getFloat("cellularJitter", s.cellularJitter);

			s.domainWarp = (NoiseSettings::DomainWarp)clampIndex(
				enumFromJson(r, "domainWarp", kDomainWarpNames, 4, (int)s.domainWarp), 4);
			s.domainWarpAmp = r.getFloat("domainWarpAmp", s.domainWarpAmp);

			s.outputMin = r.getFloat("outputMin", s.outputMin);
			s.outputMax = r.getFloat("outputMax", s.outputMax);
		}

		void writeNoiseSettings(JsonWriter& w, const NoiseSettings& s) {
			w.beginObject("settings");
			w.set("type", std::string(kNoiseTypeNames[clampIndex((int)s.type, 6)]));
			w.set("seed", s.seed);
			w.set("frequency", s.frequency);
			w.set("fractal", std::string(kFractalNames[clampIndex((int)s.fractal, 4)]));
			w.set("octaves", s.octaves);
			w.set("lacunarity", s.lacunarity);
			w.set("gain", s.gain);
			w.set("weightedStrength", s.weightedStrength);
			w.set("pingPongStrength", s.pingPongStrength);
			w.set("cellularDistance", std::string(kCellularDistanceNames[clampIndex((int)s.cellularDistance, 4)]));
			w.set("cellularReturn", std::string(kCellularReturnNames[clampIndex((int)s.cellularReturn, 7)]));
			w.set("cellularJitter", s.cellularJitter);
			w.set("domainWarp", std::string(kDomainWarpNames[clampIndex((int)s.domainWarp, 4)]));
			w.set("domainWarpAmp", s.domainWarpAmp);
			w.set("outputMin", s.outputMin);
			w.set("outputMax", s.outputMax);
			w.end();
		}

		// ── per-node params ────────────────────────────────────────────────

		void readMaterialParams(int tag, GraphNode& node, const JsonReader& p) {
			switch (tag) {
			case (int)MaterialNodeTag::ConstantFloat: {
				auto& n = static_cast<ConstantFloatNode&>(node);
				n.value = p.getFloat("value", n.value);
				break; }
			case (int)MaterialNodeTag::Constant2Vector: {
				auto& n = static_cast<Constant2VectorNode&>(node);
				n.value = p.getVec2("value", n.value);
				break; }
			case (int)MaterialNodeTag::Constant3Vector: {
				auto& n = static_cast<Constant3VectorNode&>(node);
				n.value = p.getVec3("value", n.value);
				break; }
			case (int)MaterialNodeTag::Constant4Vector: {
				auto& n = static_cast<Constant4VectorNode&>(node);
				n.value = p.getVec4("value", n.value);
				break; }
			case (int)MaterialNodeTag::TextureCoordinate: {
				auto& n = static_cast<TextureCoordinateNode&>(node);
				n.uvScale = p.getVec2("uvScale", n.uvScale);
				break; }
			case (int)MaterialNodeTag::TextureSampler: {
				auto& n = static_cast<TextureSamplerNode&>(node);
				std::string ignored;
				storeTexture(p, "texture", n.textureHandle, ignored);
				break; }
			default: break;
			}
		}

		void writeMaterialParams(int tag, const GraphNode& node, JsonWriter& w) {
			switch (tag) {
			case (int)MaterialNodeTag::ConstantFloat:
				w.beginObject("params");
				w.set("value", static_cast<const ConstantFloatNode&>(node).value);
				w.end(); break;
			case (int)MaterialNodeTag::Constant2Vector:
				w.beginObject("params");
				w.set("value", static_cast<const Constant2VectorNode&>(node).value);
				w.end(); break;
			case (int)MaterialNodeTag::Constant3Vector:
				w.beginObject("params");
				w.set("value", static_cast<const Constant3VectorNode&>(node).value);
				w.end(); break;
			case (int)MaterialNodeTag::Constant4Vector:
				w.beginObject("params");
				w.set("value", static_cast<const Constant4VectorNode&>(node).value);
				w.end(); break;
			case (int)MaterialNodeTag::TextureCoordinate:
				w.beginObject("params");
				w.set("uvScale", static_cast<const TextureCoordinateNode&>(node).uvScale);
				w.end(); break;
			case (int)MaterialNodeTag::TextureSampler:
				w.beginObject("params");
				w.set("texture", texturePath(static_cast<const TextureSamplerNode&>(node).textureHandle));
				w.end(); break;
			default: break;
			}
		}

		void readNoiseParams(int tag, GraphNode& node, const JsonReader& p) {
			switch (tag) {
			case (int)NoiseNodeTag::NoiseUnit: {
				auto& n = static_cast<NoiseUnitNode&>(node);
				if (p.has("settings"))
					readNoiseSettings(p.child("settings"), n.settings);
				break; }
			case (int)NoiseNodeTag::Output: {
				auto& n = static_cast<NoiseOutputNode&>(node);
				n.outputMin = p.getFloat("outputMin", n.outputMin);
				n.outputMax = p.getFloat("outputMax", n.outputMax);
				break; }
			case (int)NoiseNodeTag::Constant: {
				auto& n = static_cast<NoiseConstantNode&>(node);
				n.value = p.getFloat("value", n.value);
				break; }
			case (int)NoiseNodeTag::Clamp: {
				auto& n = static_cast<NoiseClampNode&>(node);
				n.minValue = p.getFloat("minValue", n.minValue);
				n.maxValue = p.getFloat("maxValue", n.maxValue);
				break; }
			case (int)NoiseNodeTag::Remap: {
				auto& n = static_cast<NoiseRemapNode&>(node);
				n.inMin = p.getFloat("inMin", n.inMin);
				n.inMax = p.getFloat("inMax", n.inMax);
				n.outMin = p.getFloat("outMin", n.outMin);
				n.outMax = p.getFloat("outMax", n.outMax);
				n.clamp = p.getBool("clamp", n.clamp);
				break; }
			case (int)NoiseNodeTag::Threshold: {
				auto& n = static_cast<NoiseThresholdNode&>(node);
				n.threshold = p.getFloat("threshold", n.threshold);
				n.falloff = p.getFloat("falloff", n.falloff);
				break; }
			case (int)NoiseNodeTag::Erf: {
				auto& n = static_cast<NoiseErfNode&>(node);
				n.divisor = p.getFloat("divisor", n.divisor);
				break; }
			case (int)NoiseNodeTag::Spline: {
				auto& n = static_cast<NoiseSplineNode&>(node);
				const size_t count = p.arraySize("points");
				if (count > 0) {
					std::vector<glm::vec2> points;
					points.reserve(count);
					for (size_t i = 0; i < count; ++i) {
						JsonReader e = p.at("points", i);
						points.push_back(glm::vec2(e.getFloat("x", 0.0f), e.getFloat("y", 0.0f)));
					}
					n.points = std::move(points);
				}
				break; }
			default: break;
			}
		}

		void writeNoiseParams(int tag, const GraphNode& node, JsonWriter& w) {
			switch (tag) {
			case (int)NoiseNodeTag::NoiseUnit:
				writeNoiseSettings(w, static_cast<const NoiseUnitNode&>(node).settings); break;
			case (int)NoiseNodeTag::Output: {
				auto& n = static_cast<const NoiseOutputNode&>(node);
				w.beginObject("params");
				w.set("outputMin", n.outputMin);
				w.set("outputMax", n.outputMax);
				w.end(); break; }
			case (int)NoiseNodeTag::Constant:
				w.beginObject("params");
				w.set("value", static_cast<const NoiseConstantNode&>(node).value);
				w.end(); break;
			case (int)NoiseNodeTag::Clamp: {
				auto& n = static_cast<const NoiseClampNode&>(node);
				w.beginObject("params");
				w.set("minValue", n.minValue);
				w.set("maxValue", n.maxValue);
				w.end(); break; }
			case (int)NoiseNodeTag::Remap: {
				auto& n = static_cast<const NoiseRemapNode&>(node);
				w.beginObject("params");
				w.set("inMin", n.inMin);
				w.set("inMax", n.inMax);
				w.set("outMin", n.outMin);
				w.set("outMax", n.outMax);
				w.set("clamp", n.clamp);
				w.end(); break; }
			case (int)NoiseNodeTag::Threshold: {
				auto& n = static_cast<const NoiseThresholdNode&>(node);
				w.beginObject("params");
				w.set("threshold", n.threshold);
				w.set("falloff", n.falloff);
				w.end(); break; }
			case (int)NoiseNodeTag::Erf:
				w.beginObject("params");
				w.set("divisor", static_cast<const NoiseErfNode&>(node).divisor);
				w.end(); break;
			case (int)NoiseNodeTag::Spline: {
				auto& n = static_cast<const NoiseSplineNode&>(node);
				w.beginObject("params");
				w.beginArray("points", n.points.size());
				for (const glm::vec2& pt : n.points) {
					w.beginObject();
					w.set("x", pt.x);
					w.set("y", pt.y);
					w.end();
				}
				w.end();
				w.end(); break; }
			default: break;
			}
		}

		// ── graph <-> JSON ─────────────────────────────────────────────────

		// `src` is the object holding "nodes" and "links"; `out` is filled from
		// scratch. Returns false with the reason in `error` on the first malformed
		// node or link, so a bad graph never half-applies.
		bool graphFromJson(const std::string& kind, const JsonReader& src, NodeGraph& out, std::string& error) {
			const bool isNoise = (kind == "noise");
			const NodeRegistry& registry = registryFor(kind);
			const std::vector<NodeSpec>& specs = specsFor(kind);

			std::unordered_map<int, GraphNode*> byId;
			uint32_t maxId = 0;

			const size_t nodeCount = src.arraySize("nodes");
			for (size_t i = 0; i < nodeCount; ++i) {
				JsonReader n = src.at("nodes", i);
				const std::string type = n.getString("type", "");
				const int tag = tagOfName(specs, type);
				if (tag < 0) {
					error = "node " + std::to_string(i) + ": unknown type '" + type +
					        "' (call list_asset_node_types for the names)";
					return false;
				}

				Ref<GraphNode> node = registry.create((uint8_t)tag);
				if (!node) {
					error = "node " + std::to_string(i) + ": type '" + type + "' is not constructible";
					return false;
				}

				int id = n.getInt("id", (int)maxId + 1);
				if (id <= 0)
					id = (int)maxId + 1;
				node->id = (uint32_t)id;
				node->m_pos = glm::vec2(n.getFloat("x", 0.0f), n.getFloat("y", 0.0f));

				const std::string displayName = n.getString("displayName", "");
				if (!displayName.empty())
					node->m_displayName = displayName;

				if (isNoise) readNoiseParams(tag, *node, n.child("params"));
				else         readMaterialParams(tag, *node, n.child("params"));

				out.assignPinIds(node.get());
				byId[id] = node.get();
				out.nodes.push_back(node);
				if ((uint32_t)id > maxId) maxId = (uint32_t)id;
			}
			out.m_nextNodeId = maxId + 1;

			const size_t linkCount = src.arraySize("links");
			for (size_t i = 0; i < linkCount; ++i) {
				JsonReader l = src.at("links", i);
				const int fromId = l.getInt("fromNode", -1);
				const int toId = l.getInt("toNode", -1);

				auto fromIt = byId.find(fromId);
				auto toIt = byId.find(toId);
				if (fromIt == byId.end() || toIt == byId.end()) {
					error = "link " + std::to_string(i) +
					        ": fromNode/toNode must name a node id defined in this graph";
					return false;
				}
				GraphNode* from = fromIt->second;
				GraphNode* to = toIt->second;

				const int fromPin = l.getInt("fromPin", 0);
				const int toPin = l.getInt("toPin", 0);
				if (fromPin < 0 || fromPin >= (int)from->m_outputPins.size()) {
					error = "link " + std::to_string(i) + ": fromPin " + std::to_string(fromPin) +
					        " is out of range (node " + std::to_string(fromId) + " has " +
					        std::to_string(from->m_outputPins.size()) + " outputs)";
					return false;
				}
				if (toPin < 0 || toPin >= (int)to->m_inputPins.size()) {
					error = "link " + std::to_string(i) + ": toPin " + std::to_string(toPin) +
					        " is out of range (node " + std::to_string(toId) + " has " +
					        std::to_string(to->m_inputPins.size()) + " inputs)";
					return false;
				}

				// Output pins are indexed past the inputs on the same node (see
				// NodeGraph::assignPinIds), which is what a link stores.
				PinId start{ from->id, (uint32_t)from->m_inputPins.size() + (uint32_t)fromPin };
				PinId end{ to->id, (uint32_t)toPin };

				const char* linkError = out.addLink(start, end);
				if (linkError) {
					error = "link " + std::to_string(i) + ": " + linkError;
					return false;
				}
			}
			return true;
		}

		void graphToJson(const std::string& kind, const NodeGraph& graph, JsonWriter& w) {
			const bool isNoise = (kind == "noise");
			const NodeRegistry& registry = registryFor(kind);
			const std::vector<NodeSpec>& specs = specsFor(kind);

			w.beginArray("nodes", graph.nodes.size());
			for (const Ref<GraphNode>& node : graph.nodes) {
				const uint8_t tag = registry.tagOf(*node);
				const char* name = (tag < specs.size()) ? specs[tag].name : "Unknown";

				w.beginObject();
				w.set("id", (int)node->id);
				w.set("type", std::string(name));
				w.set("x", node->m_pos.x);
				w.set("y", node->m_pos.y);
				if (!node->m_displayName.empty())
					w.set("displayName", node->m_displayName);
				if (isNoise) writeNoiseParams(tag, *node, w);
				else         writeMaterialParams(tag, *node, w);
				w.end();
			}
			w.end();

			w.beginArray("links", graph.links.size());
			for (const GraphLink& link : graph.links) {
				const GraphNode* from = graph.findNode(link.startPin.id);
				const int fromPin = from
					? (int)link.startPin.pinIndex - (int)from->m_inputPins.size()
					: (int)link.startPin.pinIndex;

				w.beginObject();
				w.set("fromNode", (int)link.startPin.id);
				w.set("fromPin", fromPin);
				w.set("toNode", (int)link.endPin.id);
				w.set("toPin", (int)link.endPin.pinIndex);
				w.end();
			}
			w.end();
		}

		// ── asset readers ──────────────────────────────────────────────────

		std::string readNoise(const std::string& file, const std::string& asGiven) {
			TextArchive ar(file, ArchiveMode::read);
			if (!ar.isGood())
				return errorJson("cannot open '" + file + "' for reading");

			std::string token;
			int32_t version = 0;
			ar >> token;
			ar >> version;
			if (token != "noise" || version != 2)
				return errorJson("'" + file + "' is not a noise asset (token '" + token +
				                 "', version " + std::to_string(version) + ")");

			NodeGraph graph;
			deserializeNoiseGraph(graph, ar);

			JsonWriter w;
			w.set("path", asGiven);
			w.set("kind", std::string("noise"));
			w.beginObject("graph");
			graphToJson("noise", graph, w);
			w.end();
			return w.str();
		}

		std::string readMaterialLayer(const std::string& file, const std::string& asGiven) {
			Ref<MaterialLayerAsset> asset = MaterialLayerAsset::create(file);
			if (!asset)
				return errorJson("'" + file + "' could not be loaded as a material_layer asset");

			const MaterialLayer& l = asset->layer;
			JsonWriter w;
			w.set("path", asGiven);
			w.set("kind", std::string("material_layer"));
			w.set("name", l.name);
			w.set("albedoMap", texturePath(l.albedoMap));
			w.set("normalMap", texturePath(l.normalMap));
			w.set("roughnessMap", texturePath(l.roughnessMap));
			w.set("heightMap", texturePath(l.heightMap));
			w.set("tiling", l.tiling);
			w.set("useSlope", l.useSlope);
			w.set("slopeMin", l.slopeMin);
			w.set("slopeMax", l.slopeMax);
			w.set("useHeight", l.useHeight);
			w.set("heightMin", l.heightMin);
			w.set("heightMax", l.heightMax);
			w.set("noiseStrength", l.noiseStrength);
			w.set("noiseScale", l.noiseScale);
			w.set("blend", std::string(kBlendNames[clampIndex((int)l.blend, 3)]));
			return w.str();
		}

		std::string readMaterialKind(const std::string& file, const std::string& asGiven,
		                             const std::string& token) {
			Ref<Material> material = Material::create(file);
			if (!material)
				return errorJson("'" + file + "' could not be loaded as a material");

			JsonWriter w;
			w.set("path", asGiven);

			if (token == "layered_material") {
				auto* layered = dynamic_cast<LayeredMaterial*>(material.get());
				if (!layered)
					return errorJson("'" + file + "' declares 'layered_material' but did not load as one");

				w.set("kind", std::string("layered_material"));
				w.set("name", layered->name);
				w.beginArray("layers", layered->layers.size());
				for (AssetHandle h : layered->layers)
					pushString(w, h.isValid() ? ResourceManager::getPath<MaterialLayerAsset>(h) : std::string());
				w.end();
				w.set("heightScale", layered->heightScale);
				w.set("macroStrength", layered->macroStrength);
				w.set("macroScale", layered->macroScale);
			}
			else {
				auto* single = dynamic_cast<SingleMaterial*>(material.get());
				if (!single)
					return errorJson("'" + file + "' declares 'material' but did not load as a single-surface material");

				w.set("kind", std::string("material"));
				w.set("name", single->name);
				w.set("albedoColor", single->albedoColor);
				w.set("metallic", single->metallic);
				w.set("roughness", single->roughness);
				w.set("ao", single->ao);
				w.set("emissiveColor", single->emissiveColor);
				w.beginObject("textures");
				w.set("albedo", texturePath(single->albedoMapHandle));
				w.set("normal", texturePath(single->normalMapHandle));
				w.set("metallic", texturePath(single->metallicMapHandle));
				w.set("roughness", texturePath(single->roughnessMapHandle));
				w.set("ao", texturePath(single->aoMapHandle));
				w.set("emissive", texturePath(single->emissiveMapHandle));
				w.end();
				w.beginObject("graph");
				graphToJson("material", single->graph, w);
				w.end();
			}

			return w.str();
		}

		// ── asset writers ──────────────────────────────────────────────────

		std::string writeMaterial(const std::string& file, const std::string& asGiven, const JsonReader& data) {
			auto material = CreateRef<SingleMaterial>();
			material->name = toRelative(file);

			std::string error;
			if (!storeTexture(data.child("textures"), "albedo", material->albedoMapHandle, error) ||
			    !storeTexture(data.child("textures"), "normal", material->normalMapHandle, error) ||
			    !storeTexture(data.child("textures"), "metallic", material->metallicMapHandle, error) ||
			    !storeTexture(data.child("textures"), "roughness", material->roughnessMapHandle, error) ||
			    !storeTexture(data.child("textures"), "ao", material->aoMapHandle, error) ||
			    !storeTexture(data.child("textures"), "emissive", material->emissiveMapHandle, error))
				return errorJson(error);

			material->albedoColor = data.getVec3("albedoColor", material->albedoColor);
			material->metallic = data.getFloat("metallic", material->metallic);
			material->roughness = data.getFloat("roughness", material->roughness);
			material->ao = data.getFloat("ao", material->ao);
			material->emissiveColor = data.getVec3("emissiveColor", material->emissiveColor);

			if (data.has("graph")) {
				std::string graphError;
				if (!graphFromJson("material", data.child("graph"), material->graph, graphError))
					return errorJson("graph: " + graphError);
			}
			// A material with no output node draws through the PBR fallback, but a
			// totally empty graph is not a state the editor ever produces; give it
			// the same lone output node a fresh material gets.
			if (material->graph.nodes.empty())
				material->graph.addNode(CreateRef<MaterialOutputNode>());

			TextArchive ar(file, ArchiveMode::write);
			if (!ar.isGood())
				return errorJson("cannot open '" + file + "' for writing");
			material->serialize(ar);
			return okJson(asGiven, "material");
		}

		std::string writeNoise(const std::string& file, const std::string& asGiven, const JsonReader& data) {
			if (!data.has("graph"))
				return errorJson("a noise asset needs a 'graph' object");

			NodeGraph graph;
			std::string graphError;
			if (!graphFromJson("noise", data.child("graph"), graph, graphError))
				return errorJson("graph: " + graphError);

			TextArchive ar(file, ArchiveMode::write);
			if (!ar.isGood())
				return errorJson("cannot open '" + file + "' for writing");
			// Header matches NoiseGraphResource's reader: token, then version.
			ar << std::string("noise");
			ar << (int32_t)2;
			serializeNoiseGraph(graph, ar);
			// An overwrite must refresh the already-loaded resource so terrain keyed on
			// its content picks the new graph up.
			NoiseGraphResource::refreshFromFile(file);
			return okJson(asGiven, "noise");
		}

		std::string writeMaterialLayer(const std::string& file, const std::string& asGiven, const JsonReader& data) {
			// Accept the fields either nested under "layer" or at the top level.
			JsonReader src = data.has("layer") ? data.child("layer") : data;

			auto asset = CreateRef<MaterialLayerAsset>();
			MaterialLayer& l = asset->layer;

			std::string error;
			if (!storeTexture(src, "albedoMap", l.albedoMap, error) ||
			    !storeTexture(src, "normalMap", l.normalMap, error) ||
			    !storeTexture(src, "roughnessMap", l.roughnessMap, error) ||
			    !storeTexture(src, "heightMap", l.heightMap, error))
				return errorJson(error);

			l.name = src.getString("name", l.name);
			l.tiling = src.getFloat("tiling", l.tiling);
			l.useSlope = src.getBool("useSlope", l.useSlope);
			l.slopeMin = src.getFloat("slopeMin", l.slopeMin);
			l.slopeMax = src.getFloat("slopeMax", l.slopeMax);
			l.useHeight = src.getBool("useHeight", l.useHeight);
			l.heightMin = src.getFloat("heightMin", l.heightMin);
			l.heightMax = src.getFloat("heightMax", l.heightMax);
			l.noiseStrength = src.getFloat("noiseStrength", l.noiseStrength);
			l.noiseScale = src.getFloat("noiseScale", l.noiseScale);

			const std::string blend = src.getString("blend", "");
			if (!blend.empty()) {
				int index = 0;
				for (int i = 0; i < 3; ++i)
					if (blend == kBlendNames[i]) { index = i; break; }
				l.blend = (MaterialLayerBlend)index;
			}
			else {
				l.blend = (MaterialLayerBlend)clampIndex(src.getInt("blend", (int)l.blend), 3);
			}

			TextArchive ar(file, ArchiveMode::write);
			if (!ar.isGood())
				return errorJson("cannot open '" + file + "' for writing");
			asset->serialize(ar);
			return okJson(asGiven, "material_layer");
		}

		std::string writeLayeredMaterial(const std::string& file, const std::string& asGiven, const JsonReader& data) {
			auto material = CreateRef<LayeredMaterial>();
			material->name = toRelative(file);

			const size_t count = data.arraySize("layers");
			for (size_t i = 0; i < count && i < (size_t)kMaxMaterialLayers; ++i) {
				const std::string p = data.at("layers", i).asString("");
				if (p.empty()) {
					material->layers.push_back(INVALID_ASSET_HANDLE);
					continue;
				}

				std::string pathError;
				const std::string abs = resolveAssetPath(p, false, pathError);
				if (!pathError.empty())
					return errorJson("layer " + std::to_string(i) + ": " + pathError);

				AssetHandle h = ResourceManager::store<MaterialLayerAsset>(abs);
				if (!h.isValid())
					return errorJson("layer " + std::to_string(i) + ": '" + p +
					                 "' is not a material_layer asset (call list_assets for the ones that exist)");
				material->layers.push_back(h);
			}
			if (count > (size_t)kMaxMaterialLayers)
				return errorJson("a layered material holds at most " + std::to_string(kMaxMaterialLayers) +
				                 " layers, got " + std::to_string(count));

			material->heightScale = data.getFloat("heightScale", material->heightScale);
			material->macroStrength = data.getFloat("macroStrength", material->macroStrength);
			material->macroScale = data.getFloat("macroScale", material->macroScale);

			TextArchive ar(file, ArchiveMode::write);
			if (!ar.isGood())
				return errorJson("cannot open '" + file + "' for writing");
			material->serialize(ar);
			return okJson(asGiven, "layered_material");
		}

		// ── node-type catalogue emission ───────────────────────────────────

		void writeCatalogue(JsonWriter& w, const std::string& kind) {
			const std::vector<NodeSpec>& specs = specsFor(kind);

			w.beginObject(kind.c_str());
			w.set("graph_format", std::string(
				"{\"nodes\":[{\"id\":int,\"type\":name,\"x\":float,\"y\":float,\"params\":{...}}],"
				"\"links\":[{\"fromNode\":id,\"fromPin\":outputIndex,\"toNode\":id,\"toPin\":inputIndex}]}"));
			if (kind == "noise") {
				w.set("noise_settings_fields", std::string(
					"type(OpenSimplex2|OpenSimplex2S|Cellular|Perlin|ValueCubic|Value), seed(int), frequency(float), "
					"fractal(None|Fbm|Ridged|PingPong), octaves(int), lacunarity(float), gain(float), "
					"weightedStrength(float), pingPongStrength(float), "
					"cellularDistance(Euclidean|EuclideanSq|Manhattan|Hybrid), "
					"cellularReturn(CellValue|Distance|Distance2|Distance2Add|Distance2Sub|Distance2Mul|Distance2Div), "
					"cellularJitter(float), domainWarp(None|OpenSimplex2|OpenSimplex2Reduced|BasicGrid), "
					"domainWarpAmp(float), outputMin(float), outputMax(float)"));
			}

			w.beginArray("node_types", specs.size());
			for (const NodeSpec& spec : specs) {
				w.beginObject();
				w.set("name", spec.name);
				w.set("inputs", std::string(spec.inputs));
				w.set("outputs", std::string(spec.outputs));
				w.beginArray("params", spec.params.size());
				for (const ParamSpec& param : spec.params) {
					w.beginObject();
					w.set("name", param.name);
					w.set("kind", param.kind);
					w.end();
				}
				w.end();
				w.end();
			}
			w.end();
			w.end();
		}

	} // namespace

	std::string assetNodeTypesJson(const std::string& kind) {
		JsonWriter w;
		if (kind == "material" || kind == "noise") {
			writeCatalogue(w, kind);
			return w.str();
		}
		if (kind.empty()) {
			writeCatalogue(w, "material");
			writeCatalogue(w, "noise");
			return w.str();
		}
		return errorJson("unknown kind '" + kind + "' (expected 'material' or 'noise')");
	}

	std::string readAssetJson(const std::string& path) {
		std::string error;
		const std::string file = resolveAssetPath(path, false, error);
		if (!error.empty())
			return errorJson(error);

		std::error_code ec;
		if (!std::filesystem::exists(file, ec))
			return errorJson("no asset at '" + file + "' (call list_assets for the paths that exist)");

		const std::string token = utils::peekAssetToken(file);
		if (token.empty())
			return errorJson("'" + file + "' is not a readable .veasset");

		if (token == "noise")          return readNoise(file, path);
		if (token == "material_layer") return readMaterialLayer(file, path);
		if (token == "material" || token == "layered_material") return readMaterialKind(file, path, token);

		return errorJson("'" + file + "' is a '" + token + "' asset; readable kinds are "
		                 "material, layered_material, material_layer and noise");
	}

	std::string writeAssetJson(const std::string& path, const JsonReader& data, bool overwrite) {
		if (!data.valid())
			return errorJson("'data' must be an object carrying the asset body");

		const std::string kind = data.getString("kind", "");
		if (kind.empty())
			return errorJson("'data' must carry a 'kind' (material, noise, material_layer or layered_material)");

		std::string error;
		const std::string file = resolveAssetPath(path, true, error);
		if (!error.empty())
			return errorJson(error);

		std::error_code ec;
		if (std::filesystem::exists(file, ec) && !overwrite)
			return errorJson("'" + file + "' already exists; use write_asset to overwrite it");

		if (kind == "material")         return announceWrite(writeMaterial(file, path, data));
		if (kind == "noise")            return announceWrite(writeNoise(file, path, data));
		if (kind == "material_layer")   return announceWrite(writeMaterialLayer(file, path, data));
		if (kind == "layered_material") return announceWrite(writeLayeredMaterial(file, path, data));

		return errorJson("unknown kind '" + kind +
		                 "' (expected material, noise, material_layer or layered_material)");
	}

}
