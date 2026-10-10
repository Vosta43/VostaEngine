#include "vepch.h"
#include "FontManager.h"

#include "FontAtlas.h"
#include "Core/Log.h"

namespace ve::vellum {

	void FontManager::init(VellumRenderer* renderer, std::string defaultTtfPath) {
		m_renderer = renderer;
		m_defaultPath = std::move(defaultTtfPath);
	}

	FontAtlas* FontManager::font(float pixelSize) {
		if (!m_renderer) {
			VE_CORE_ERROR_PRINT("Vellum: FontManager used before init()");
			return nullptr;
		}

		const auto key = std::make_pair(m_defaultPath, pixelSize);
		const auto found = m_cache.find(key);
		if (found != m_cache.end())
			return found->second.get();

		Ref<FontAtlas> atlas = FontAtlas::create(m_defaultPath, pixelSize, m_nextId, *m_renderer);
		if (!atlas)
			return nullptr;

		++m_nextId;
		return m_cache.emplace(key, std::move(atlas)).first->second.get();
	}

}
