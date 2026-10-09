#pragma once

#include "Core/Core.h"
#include "Renderer/FrameBuffer.h"

#include <string>
#include <vector>

namespace ve {

	// Data description of the render pipeline. The pipeline is a list of passes
	// wired to named framebuffers; nothing here references a live GL object. The
	// JSON asset in SandBox/assets/pipelines is loaded into this; makeDefault()
	// reproduces the hardcoded pipeline that predates the data-driven build, so a
	// missing/broken asset degrades to today's behaviour.

	struct ShaderDef {
		std::string name;   // == file stem; ShaderLibrary keys by stem
		std::string path;
	};

	struct FboAttachmentDef {
		int                slot = 0;   // color attachment index
		TextureInternalFormat internalFormat = TextureInternalFormat::RGBA8;
		TextureDataFormat     format = TextureDataFormat::RGBA;
		TextureDataType       type = TextureDataType::UNSIGNED_BYTE;
		TextureFilter         minFilter = TextureFilter::LINEAR;
		TextureFilter         magFilter = TextureFilter::LINEAR;
	};

	struct FboDef {
		std::string name;
		// Size: absolute width/height win when > 0, otherwise the scale multiplies
		// the render size. Resolved by the registry at build/resize time.
		float    widthScale = 1.0f;
		float    heightScale = 1.0f;
		uint32_t width = 0;
		uint32_t height = 0;
		bool     hasDepthStencil = false;
		// Put the depth attachment in compare mode (sampler2DShadow). Shadow maps.
		bool     depthCompare = false;
		std::vector<FboAttachmentDef> attachments;
	};

	// A declared input: bind `source` to `uniform` on the pass's shader at `unit`.
	// `source` is either an @-sentinel (see PassBinding) or a name resolved
	// against the pass-output table / FBO registry.
	struct PassInputDef {
		std::string uniform;
		int         unit = 0;
		std::string source;
		std::string type;         // "2d" | "cube" | "3d"; empty = "2d"
		int         attachment = 0;  // color attachment of the source FBO
		bool        depth = false;   // bind the source FBO's depth texture instead
	};

	struct ClearDef {
		bool  enabled = false;
		float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		bool  depth = false;
	};

	enum class BlendFactor { Zero, One, SrcAlpha, OneMinusSrcAlpha };
	enum class DepthFuncType { Less, Lequal };

	struct PassStateDef {
		bool          blend = false;
		BlendFactor   src = BlendFactor::SrcAlpha;
		BlendFactor   dst = BlendFactor::OneMinusSrcAlpha;
		bool          depthTest = false;
		bool          depthWrite = false;
		DepthFuncType depthFunc = DepthFuncType::Less;
	};

	struct ConditionDef {
		std::string provider;   // engine condition flag; unknown = false
		bool        negate = false;
	};

	enum class UniformKind { Float, Int, Bool, Vec2, Vec3, Vec4 };

	// A uniform pushed by the pass every frame (not a setting the user edits).
	struct UniformValueDef {
		std::string uniform;
		UniformKind kind = UniformKind::Float;
		float       v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		bool        vs[4] = { false, false, false, false };  // for Bool/int
	};

	enum class SettingsApply { Uniform, Resource, Pass, Variant };

	struct SettingDef {
		std::string   name;
		SettingsApply apply = SettingsApply::Uniform;
		std::string   uniform;      // Uniform kind: shader uniform
		std::string   target;       // Resource: fbo name; Pass: pass name
		UniformKind   kind = UniformKind::Float;
		float         value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		bool          boolValue = false;
		float         minValue = 0.0f;
		float         maxValue = 1.0f;
		std::vector<float> discrete;   // Variant: allowed values
	};

	struct PassDef {
		std::string name;
		std::string type;       // dispatch key for createPassByType
		std::string target;     // FBO name written by this pass, or "@default"
		std::string shader;     // shader name from the shaders list
		int         index = -1; // pass-defined integer (currently the shadow cascade)
		std::vector<PassInputDef>    inputs;
		std::vector<UniformValueDef> uniforms;
		std::vector<std::string>     history;   // ping-pong pair of FBO names
		ClearDef     clear;
		PassStateDef state;
		bool         hasState = false;
		ConditionDef when;
		bool         hasCondition = false;
	};

	// The whole pipeline as data. Pass execution order == passes order.
	struct PipelineConfig {
		std::vector<ShaderDef>  shaders;
		std::vector<FboDef>     framebuffers;
		std::vector<PassDef>    passes;
		std::vector<SettingDef> settings;

		// False when the file is missing or malformed; callers fall back to
		// makeDefault(). Warns on any unrecognised key/value it had to default.
		bool load(const std::string& absPath);

		// The hardcoded pipeline as data — the pre-JSON baseline.
		static PipelineConfig makeDefault();
	};

}
