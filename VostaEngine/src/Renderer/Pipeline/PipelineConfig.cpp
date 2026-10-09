#include "vepch.h"
#include "PipelineConfig.h"

#include "Core/Json.h"
#include "Core/Log.h"

#include <algorithm>
#include <cctype>

namespace ve {

	namespace {

		std::string lower(const std::string& s) {
			std::string r = s;
			std::transform(r.begin(), r.end(), r.begin(),
				[](unsigned char c) { return (char)std::tolower(c); });
			return r;
		}

		TextureInternalFormat parseInternalFormat(const std::string& s) {
			if (s.empty()) return TextureInternalFormat::RGBA8;
			if (s == "RGBA8")    return TextureInternalFormat::RGBA8;
			if (s == "RGBA16F")  return TextureInternalFormat::RGBA16F;
			if (s == "R32I")     return TextureInternalFormat::R32I;
			VE_CORE_WARN_PRINT("PipelineConfig: unknown internalFormat '%s', using RGBA8", s.c_str());
			return TextureInternalFormat::RGBA8;
		}

		TextureDataFormat defaultFormat(TextureInternalFormat f) {
			return (f == TextureInternalFormat::R32I) ? TextureDataFormat::RED_INTEGER
			                                          : TextureDataFormat::RGBA;
		}

		TextureDataType defaultType(TextureInternalFormat f) {
			switch (f) {
				case TextureInternalFormat::RGBA16F: return TextureDataType::FLOAT;
				case TextureInternalFormat::R32I:    return TextureDataType::INT;
				default:                             return TextureDataType::UNSIGNED_BYTE;
			}
		}

		TextureDataFormat parseDataFormat(const std::string& s, TextureInternalFormat f) {
			if (s.empty()) return defaultFormat(f);
			if (s == "RGBA")        return TextureDataFormat::RGBA;
			if (s == "RED_INTEGER") return TextureDataFormat::RED_INTEGER;
			VE_CORE_WARN_PRINT("PipelineConfig: unknown format '%s', deriving from internalFormat", s.c_str());
			return defaultFormat(f);
		}

		TextureDataType parseDataType(const std::string& s, TextureInternalFormat f) {
			if (s.empty()) return defaultType(f);
			if (s == "UNSIGNED_BYTE") return TextureDataType::UNSIGNED_BYTE;
			if (s == "FLOAT")         return TextureDataType::FLOAT;
			if (s == "INT")           return TextureDataType::INT;
			VE_CORE_WARN_PRINT("PipelineConfig: unknown type '%s', deriving from internalFormat", s.c_str());
			return defaultType(f);
		}

		TextureFilter parseFilter(const std::string& s) {
			if (s.empty()) return TextureFilter::LINEAR;
			const std::string l = lower(s);
			if (l == "linear")  return TextureFilter::LINEAR;
			if (l == "nearest") return TextureFilter::NEAREST;
			VE_CORE_WARN_PRINT("PipelineConfig: unknown filter '%s', using linear", s.c_str());
			return TextureFilter::LINEAR;
		}

		BlendFactor parseBlend(const std::string& s, BlendFactor def) {
			if (s.empty()) return def;
			const std::string l = lower(s);
			if (l == "zero")                 return BlendFactor::Zero;
			if (l == "one")                  return BlendFactor::One;
			if (l == "src_alpha")            return BlendFactor::SrcAlpha;
			if (l == "one_minus_src_alpha")  return BlendFactor::OneMinusSrcAlpha;
			VE_CORE_WARN_PRINT("PipelineConfig: unknown blend factor '%s', using default", s.c_str());
			return def;
		}

		DepthFuncType parseDepthFunc(const std::string& s) {
			const std::string l = lower(s);
			if (l.empty())  return DepthFuncType::Less;
			if (l == "lequal") return DepthFuncType::Lequal;
			if (l == "less")   return DepthFuncType::Less;
			VE_CORE_WARN_PRINT("PipelineConfig: depthFunc '%s' unsupported, using less", s.c_str());
			return DepthFuncType::Less;
		}

		UniformKind parseUniformKind(const std::string& s) {
			const std::string l = lower(s);
			if (l.empty()) return UniformKind::Float;
			if (l == "float") return UniformKind::Float;
			if (l == "int")   return UniformKind::Int;
			if (l == "bool")  return UniformKind::Bool;
			if (l == "vec2")  return UniformKind::Vec2;
			if (l == "vec3")  return UniformKind::Vec3;
			if (l == "vec4")  return UniformKind::Vec4;
			VE_CORE_WARN_PRINT("PipelineConfig: unknown uniform kind '%s', using float", s.c_str());
			return UniformKind::Float;
		}

		SettingsApply parseApply(const std::string& s) {
			const std::string l = lower(s);
			if (l.empty() || l == "uniform") return SettingsApply::Uniform;
			if (l == "resource") return SettingsApply::Resource;
			if (l == "pass")     return SettingsApply::Pass;
			if (l == "variant")  return SettingsApply::Variant;
			VE_CORE_WARN_PRINT("PipelineConfig: unknown settings apply '%s', using uniform", s.c_str());
			return SettingsApply::Uniform;
		}

		FboAttachmentDef parseAttachment(const JsonReader& el) {
			FboAttachmentDef a;
			a.slot = el.getInt("slot", 0);
			a.internalFormat = parseInternalFormat(el.getString("internalFormat"));
			a.format = parseDataFormat(el.getString("format"), a.internalFormat);
			a.type = parseDataType(el.getString("type"), a.internalFormat);
			a.minFilter = parseFilter(el.getString("minFilter"));
			a.magFilter = parseFilter(el.getString("magFilter"));
			return a;
		}

		FboDef parseFbo(const JsonReader& el) {
			FboDef f;
			f.name = el.getString("name");
			f.widthScale = el.getFloat("widthScale", 1.0f);
			f.heightScale = el.getFloat("heightScale", 1.0f);
			f.width = (uint32_t)std::max(0, el.getInt("width", 0));
			f.height = (uint32_t)std::max(0, el.getInt("height", 0));
			f.hasDepthStencil = el.getBool("depth", false);
			f.depthCompare = el.getBool("depthCompare", false);

			const size_t n = el.arraySize("attachments");
			f.attachments.reserve(n);
			for (size_t i = 0; i < n; ++i)
				f.attachments.push_back(parseAttachment(el.at("attachments", i)));
			return f;
		}

		PassInputDef parseInput(const JsonReader& el) {
			PassInputDef in;
			in.uniform = el.getString("uniform");
			in.unit = el.getInt("unit", 0);
			in.source = el.getString("source");
			in.type = el.getString("type");
			in.attachment = el.getInt("attachment", 0);
			in.depth = el.getBool("depth", false);
			return in;
		}

		PassStateDef parseState(const JsonReader& el) {
			PassStateDef s;
			s.blend = el.getBool("blend", false);
			s.src = parseBlend(el.getString("src"), BlendFactor::SrcAlpha);
			s.dst = parseBlend(el.getString("dst"), BlendFactor::OneMinusSrcAlpha);
			s.depthTest = el.getBool("depthTest", false);
			s.depthWrite = el.getBool("depthWrite", false);
			s.depthFunc = parseDepthFunc(el.getString("depthFunc"));

			// Subkeys Vosta's RenderCommand has no way to express. Parse-and-ignore.
			const char* unsupported[] = { "cull", "polygon_offset", "line_width", "topology", "colorWrite" };
			for (const char* k : unsupported)
				if (el.has(k))
					VE_CORE_WARN_PRINT("PipelineConfig: pass state '%s' is unsupported in Vosta, ignored", k);
			return s;
		}

		PassDef parsePass(const JsonReader& el) {
			PassDef p;
			p.name = el.getString("name");
			p.type = el.getString("type");
			p.target = el.getString("target");
			p.shader = el.getString("shader");
			p.index = el.getInt("index", -1);

			const size_t nIn = el.arraySize("inputs");
			p.inputs.reserve(nIn);
			for (size_t i = 0; i < nIn; ++i)
				p.inputs.push_back(parseInput(el.at("inputs", i)));

			if (el.has("clear")) {
				const JsonReader c = el.child("clear");
				p.clear.enabled = true;
				p.clear.color[0] = c.getFloat("r", 0.0f);
				p.clear.color[1] = c.getFloat("g", 0.0f);
				p.clear.color[2] = c.getFloat("b", 0.0f);
				p.clear.color[3] = c.getFloat("a", 1.0f);
				p.clear.depth = c.getBool("depth", false);
			}

			if (el.has("state")) {
				p.state = parseState(el.child("state"));
				p.hasState = true;
			}

			if (el.has("when")) {
				const JsonReader w = el.child("when");
				p.when.provider = w.getString("from");
				p.when.negate = w.getBool("not", false);
				p.hasCondition = true;
			}

			const size_t nHist = el.arraySize("history");
			for (size_t i = 0; i < nHist; ++i)
				p.history.push_back(el.at("history", i).asString());

			const size_t nU = el.arraySize("uniforms");
			for (size_t i = 0; i < nU; ++i) {
				const JsonReader u = el.at("uniforms", i);
				UniformValueDef uv;
				uv.uniform = u.getString("uniform");
				uv.kind = parseUniformKind(u.getString("kind"));
				uv.v[0] = u.getFloat("x", 0.0f);
				uv.v[1] = u.getFloat("y", 0.0f);
				uv.v[2] = u.getFloat("z", 0.0f);
				uv.v[3] = u.getFloat("w", 0.0f);
				p.uniforms.push_back(uv);
			}
			return p;
		}

		SettingDef parseSetting(const JsonReader& el) {
			SettingDef s;
			s.name = el.getString("name");
			s.apply = parseApply(el.getString("apply"));
			s.uniform = el.getString("uniform");
			s.target = el.getString("target");
			s.kind = parseUniformKind(el.getString("kind"));
			s.value[0] = el.getFloat("value", 0.0f);
			s.boolValue = el.getBool("value", false);
			s.minValue = el.getFloat("min", 0.0f);
			s.maxValue = el.getFloat("max", 1.0f);

			const size_t n = el.arraySize("values");
			for (size_t i = 0; i < n; ++i)
				s.discrete.push_back(el.at("values", i).asFloat());

			if (s.apply == SettingsApply::Variant)
				VE_CORE_WARN_PRINT("PipelineConfig: setting '%s' is a variant; not implemented in Vosta", s.name.c_str());
			return s;
		}

	} // namespace

	bool PipelineConfig::load(const std::string& absPath) {
		JsonReader root;
		if (!JsonReader::load(absPath, root) || !root.valid()) {
			VE_CORE_WARN_PRINT("PipelineConfig: failed to load '%s'", absPath.c_str());
			return false;
		}

		shaders.clear();
		framebuffers.clear();
		passes.clear();
		settings.clear();

		const size_t nS = root.arraySize("shaders");
		for (size_t i = 0; i < nS; ++i) {
			const JsonReader el = root.at("shaders", i);
			ShaderDef s;
			s.name = el.getString("name");
			s.path = el.getString("path");
			shaders.push_back(std::move(s));
		}

		const size_t nF = root.arraySize("framebuffers");
		for (size_t i = 0; i < nF; ++i)
			framebuffers.push_back(parseFbo(root.at("framebuffers", i)));

		const size_t nP = root.arraySize("passes");
		for (size_t i = 0; i < nP; ++i)
			passes.push_back(parsePass(root.at("passes", i)));

		const size_t nSet = root.arraySize("settings");
		for (size_t i = 0; i < nSet; ++i)
			settings.push_back(parseSetting(root.at("settings", i)));

		if (passes.empty()) {
			VE_CORE_WARN_PRINT("PipelineConfig: '%s' declares no passes", absPath.c_str());
			return false;
		}
		return true;
	}

	PipelineConfig PipelineConfig::makeDefault() {
		PipelineConfig c;

		c.shaders = {
			{ "gbuffer",           "SandBox/assets/shaders/gbuffer.glsl" },
			{ "terrain_layers",    "SandBox/assets/shaders/terrain_layers.glsl" },
			{ "volume_cloud_pass", "SandBox/assets/shaders/volume_cloud_pass.glsl" },
			{ "cloud_taa",         "SandBox/assets/shaders/cloud_taa.glsl" },
			{ "pbrlighting",       "SandBox/assets/shaders/pbrlighting.glsl" },
			{ "taa",               "SandBox/assets/shaders/taa.glsl" },
			{ "postprocess",       "SandBox/assets/shaders/postprocess.glsl" },
			{ "screen",            "SandBox/assets/shaders/screen.glsl" },
		};

		auto color = [](int slot, TextureInternalFormat fmt) {
			FboAttachmentDef a;
			a.slot = slot;
			a.internalFormat = fmt;
			a.format = defaultFormat(fmt);
			a.type = defaultType(fmt);
			return a;
		};

		auto fbo = [](const char* name, float wScale, float hScale, bool depth,
		              std::vector<FboAttachmentDef> atts) {
			FboDef f;
			f.name = name;
			f.widthScale = wScale;
			f.heightScale = hScale;
			f.hasDepthStencil = depth;
			f.attachments = std::move(atts);
			return f;
		};

		c.framebuffers = {
			fbo("gbuffer", 1.0f, 1.0f, true, {
				color(0, TextureInternalFormat::RGBA8),
				color(1, TextureInternalFormat::RGBA16F),
				color(2, TextureInternalFormat::RGBA8),
				color(3, TextureInternalFormat::RGBA8),
			}),
			fbo("hdr", 1.0f, 1.0f, false, { color(0, TextureInternalFormat::RGBA16F) }),
			fbo("cloud", 0.25f, 0.25f, false, { color(0, TextureInternalFormat::RGBA16F) }),
			fbo("cloudTaaA", 0.25f, 0.25f, false, { color(0, TextureInternalFormat::RGBA16F) }),
			fbo("cloudTaaB", 0.25f, 0.25f, false, { color(0, TextureInternalFormat::RGBA16F) }),
			fbo("taaA", 1.0f, 1.0f, false, { color(0, TextureInternalFormat::RGBA16F) }),
			fbo("taaB", 1.0f, 1.0f, false, { color(0, TextureInternalFormat::RGBA16F) }),
			fbo("post", 1.0f, 1.0f, false, { color(0, TextureInternalFormat::RGBA8) }),
		};

		auto pass = [](const char* name, const char* type, const char* target, const char* shader) {
			PassDef p;
			p.name = name;
			p.type = type;
			p.target = target;
			p.shader = shader;
			return p;
		};

		PassDef gbuffer = pass("gbuffer", "gbuffer", "gbuffer", "gbuffer");

		// cloud/cloudTaa run unconditionally: with no atmosphere they clear the
		// cloud buffer to transparent and consumers composite nothing, matching
		// the pre-data-driven behaviour. (A `when` gate would skip them and leave
		// stale cloud textures bound on the units consumers sample.)
		PassDef cloud = pass("cloud", "cloud", "cloud", "volume_cloud_pass");

		PassDef cloudTaa = pass("cloudTaa", "cloudtaa", "cloudTaaA", "cloud_taa");
		cloudTaa.history = { "cloudTaaA", "cloudTaaB" };
		cloudTaa.inputs = {
			{ "u_CloudColor", 0, "cloud", "2d", 0, false },
			{ "u_History",    1, "@history", "2d", 0, false },
		};

		PassDef hdr = pass("hdr", "hdr", "hdr", "pbrlighting");
		hdr.inputs = {
			{ "u_AlbedoMap",        0, "gbuffer",  "2d", 0, false },
			{ "u_NormalMap",        1, "gbuffer",  "2d", 1, false },
			{ "u_MaterialMap",      2, "gbuffer",  "2d", 2, false },
			{ "u_DepthMap",         3, "gbuffer",  "2d", 0, true  },
			{ "u_CloudTex",         4, "cloudTaa", "2d", 0, false },
			{ "u_SkyboxMap",        5, "@skybox",       "cube", 0, false },
			{ "u_IrradianceMap",    6, "@irradiance",   "cube", 0, false },
			{ "u_PrefilteredEnvMap",7, "@prefilteredEnv","cube", 0, false },
			{ "u_BRDFLUT",          8, "@brdfLUT",      "2d",   0, false },
		};

		PassDef taa = pass("taa", "taa", "taaA", "taa");
		taa.history = { "taaA", "taaB" };
		taa.inputs = {
			{ "u_HDRColor", 0, "@previous", "2d", 0, false },
			{ "u_DepthMap", 1, "gbuffer",   "2d", 0, true  },
			{ "u_History",  2, "@history",  "2d", 0, false },
			{ "u_CloudTex", 3, "cloudTaa",  "2d", 0, false },
		};

		PassDef post = pass("post", "post", "post", "postprocess");
		post.inputs = {
			{ "u_HDRColor", 0, "@previous", "2d", 0, false },
		};

		PassDef present = pass("present", "present", "@default", "screen");
		present.inputs = {
			{ "u_ScreenTexture", 0, "@previous", "2d", 0, false },
			{ "u_DepthMap",      1, "gbuffer",   "2d", 0, true  },
		};

		c.passes = { gbuffer, cloud, cloudTaa, hdr, taa, post, present };
		return c;
	}

}
