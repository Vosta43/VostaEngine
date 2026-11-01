#include "vepch.h"
#include "FontManager.h"

#include "FontAtlas.h"
#include "Core/Log.h"
#include "Gui/Vellum/VellumRenderer.h"

namespace ve::vellum {

	void FontManager::init(VellumRenderer* renderer, std::vector<std::string> defaultFaces) {
		m_renderer = renderer;
		m_defaultFaces = std::move(defaultFaces);
		m_default.reset();
	}

	void FontManager::init(VellumRenderer* renderer, std::string defaultTtfPath) {
		init(renderer, std::vector<std::string>{ std::move(defaultTtfPath) });
	}

	Font* FontManager::add(const std::string& name, std::vector<std::string> faces) {
		if (!m_renderer) {
			VE_CORE_ERROR_PRINT("Vellum: FontManager used before init()");
			return nullptr;
		}
		if (name.empty()) {
			VE_CORE_ERROR_PRINT("Vellum: a registered font needs a name");
			return nullptr;
		}
		if (const auto found = m_fonts.find(name); found != m_fonts.end())
			return found->second.get();

		Ref<Font> font = Font::load(*m_renderer, std::move(faces), name);
		if (!font)
			return nullptr;

		return m_fonts.emplace(name, std::move(font)).first->second.get();
	}

	Font* FontManager::find(const std::string& name) const {
		const auto found = m_fonts.find(name);
		return found != m_fonts.end() ? found->second.get() : nullptr;
	}

	Font* FontManager::defaultFont() {
		if (!m_renderer) {
			VE_CORE_ERROR_PRINT("Vellum: FontManager used before init()");
			return nullptr;
		}
		if (m_default)
			return m_default.get();

		if (m_defaultFaces.empty()) {
			VE_CORE_ERROR_PRINT("Vellum: FontManager has no face in its chain");
			return nullptr;
		}

		m_default = Font::load(*m_renderer, m_defaultFaces, "default");
		return m_default.get();
	}

	FontAtlas* FontManager::font(float pixelSize) {
		Font* def = defaultFont();
		return def ? def->at(pixelSize) : nullptr;
	}

}
