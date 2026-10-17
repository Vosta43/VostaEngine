#pragma once

#include "Renderer/LayeredMaterial.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ve {

	// A generated layered-surface shader: the vertex source, the per-layer fragment
	// source, and the content samplers it declared (uniform name + texture + unit).
	// `ok == false` carries the reason in `error`; the caller then leaves the draw
	// to the pass's default shader instead of binding a broken one.
	struct LayeredBuildResult {
		std::string vertSrc;
		std::string fragSrc;
		std::vector<MaterialTextureBinding> textures;
		int contentSamplerCount = 0;
		bool ok = false;
		std::string error;
	};

	// Composes a LayeredMaterial's shader: one GLSL function per layer, each
	// evaluating that layer's referenced material (its node graph, or its fixed
	// PBR slots), blended by the shared terrain template. No GL calls — the caller
	// feeds the sources to Shader::create.
	class VE_API LayeredShaderBuilder {
	public:
		// Unit reserved for the painted control map. Content samplers are allocated
		// from 0 up, so keeping the control map at the top of the 16-slot budget
		// stops the two from colliding.
		static constexpr uint32_t kWeightsTextureUnit = 15;
		static constexpr int      kMaxContentSamplers = 15;

		// Globe content/macro tiling: repeats per cube face. Constants for now.
		static constexpr float kGlobeTiles      = 128.0f;
		static constexpr float kGlobeMacroTiles = 4.0f;

		static LayeredBuildResult build(const LayeredMaterial& mat);
	};

}
