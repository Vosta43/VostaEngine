#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"   // TextureId

#include <map>
#include <string>
#include <utility>

namespace ve::vellum {

	class FontAtlas;
	class VellumRenderer;

	// Owns the font atlases and hands out one per (face, pixel size), creating
	// them on demand. Ids come from the renderer's allocator, so an atlas and a
	// widget texture never collide.
	class VE_API FontManager {
	public:
		// `renderer` must outlive the manager. `defaultTtfPath` is the absolute
		// path backing font(size).
		void init(VellumRenderer* renderer, std::string defaultTtfPath);

		// The default face at `pixelSize`; created and uploaded on first request.
		// Returns null if the face could not be read.
		FontAtlas* font(float pixelSize);

		// The default face at the reference size (32px).
		FontAtlas* defaultFont() { return font(kDefaultSize); }

		static constexpr float kDefaultSize = 32.0f;

	private:
		VellumRenderer* m_renderer = nullptr;
		std::string     m_defaultPath;

		std::map<std::pair<std::string, float>, Ref<FontAtlas>> m_cache;
	};

}
