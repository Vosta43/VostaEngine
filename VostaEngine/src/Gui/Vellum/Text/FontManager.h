#pragma once

#include "Core/Core.h"
#include "Font.h"
#include "Gui/Vellum/Core/DrawList.h"   // TextureId

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ve::vellum {

	class FontAtlas;
	class VellumRenderer;

	// Owns the engine's named fonts and hands out per-size handles. Face bytes and
	// glyphs are shared through the renderer: a size adds metrics, not a copy of
	// the font file or a second texture.
	class VE_API FontManager {
	public:
		// `renderer` must outlive the manager. `defaultFaces` is the default chain,
		// primary first; codepoints are looked up through it in order.
		void init(VellumRenderer* renderer, std::vector<std::string> defaultFaces);

		// Single-face convenience.
		void init(VellumRenderer* renderer, std::string defaultTtfPath);

		// Registers `name` as a face chain. A taken name returns the existing font
		// unchanged, so atlases already handed out never dangle. Null when the chain
		// has no readable face.
		Font* add(const std::string& name, std::vector<std::string> faces);

		// Null when `name` was never registered.
		Font* find(const std::string& name) const;

		// The chain passed to init(), or null before init() / when it did not load.
		Font* defaultFont();

		// The default chain at `pixelSize`; created and uploaded on first request.
		FontAtlas* font(float pixelSize);

		static constexpr float kDefaultSize = Font::kReferenceSize;

	private:
		VellumRenderer* m_renderer = nullptr;
		std::vector<std::string> m_defaultFaces;
		Ref<Font> m_default;
		std::map<std::string, Ref<Font>> m_fonts;
	};

}
