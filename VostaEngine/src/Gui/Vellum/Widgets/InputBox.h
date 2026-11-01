#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"
#include "Gui/Vellum/Core/Widget.h"

#include <glm.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace ve::vellum {

	class FontAtlas;

	// Single-line UTF-8 text field. Click to focus and place the caret; Enter
	// submits. No selection, clipboard, or IME composition yet.
	class VE_API InputBox : public Widget {
	public:
		// Left keeps the caret-anchored editing every existing user expects; Center
		// is for fields whose text reads as a caption (the old pixel-art boxes).
		enum class TextAlign { Left, Center };

		InputBox(FontAtlas* font, const glm::vec2& size);

		void setFont(FontAtlas* font);
		void setSize(const glm::vec2& size);

		// Host-side write; does not fire onTextChanged.
		void setText(std::string text);
		const std::string& text() const { return m_text; }

		void setTextColor(const glm::vec4& color) { m_textColor = color; }
		void setTextAlign(TextAlign align) { m_textAlign = align; }

		// Shown while the field is empty; zero-width text disables it.
		void setPlaceholder(std::string text,
		                    const glm::vec4& color = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)) {
			m_placeholder = std::move(text);
			m_placeholderColor = color;
		}

		// Replaces the border/fill art with one texture stretched over the rect.
		// The focus accent becomes a tint multiplier so the pixel art keeps its
		// look instead of gaining a coloured border.
		void setBackgroundTexture(TextureId texture) { m_bgTexture = texture; }
		void setBackgroundTint(const glm::vec4& tint) { m_bgTint = tint; }
		void setFocusTintScale(float scale) { m_focusTintScale = scale; }

		void setOnTextChanged(std::function<void(const std::string&)> fn) { m_onTextChanged = std::move(fn); }
		void setOnSubmit(std::function<void(const std::string&)> fn) { m_onSubmit = std::move(fn); }

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;

		Reply onMouseDown(const glm::vec2& pos) override;

		bool onKeyPressed(int key, int repeat) override;
		bool onTextInput(uint32_t codepoint) override;

		void onFocus() override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		Rect innerRect() const;       // inside the border
		Rect contentRect() const;     // inner rect inset by horizontal padding
		float textOffsetX(const std::string& text) const;  // Center-alignment inset
		void updateCaretMetrics();    // recomputes the caret offset and scroll
		void placeCaretAt(float contentX);
		void notifyTextChanged();

		FontAtlas*  m_font = nullptr;
		glm::vec2   m_size{ 0.0f };
		std::string m_text;
		std::size_t m_caret = 0;      // byte offset, always on a codepoint boundary

		// Caret offset from the content's left edge, in pixels (scroll excluded).
		float m_caretContentX = 0.0f;
		float m_scrollX = 0.0f;

		glm::vec4 m_bgColor{ 0.10f, 0.11f, 0.13f, 1.0f };
		glm::vec4 m_borderColor{ 0.30f, 0.32f, 0.36f, 1.0f };
		glm::vec4 m_hoverBorderColor{ 0.42f, 0.45f, 0.50f, 1.0f };
		glm::vec4 m_focusBorderColor{ 0.24f, 0.50f, 0.85f, 1.0f };
		glm::vec4 m_textColor{ 0.92f, 0.93f, 0.95f, 1.0f };
		glm::vec4 m_caretColor{ 0.92f, 0.93f, 0.95f, 1.0f };

		TextAlign   m_textAlign = TextAlign::Left;
		std::string m_placeholder;
		glm::vec4   m_placeholderColor{ 0.6f, 0.6f, 0.6f, 1.0f };

		TextureId m_bgTexture = 0;
		glm::vec4 m_bgTint{ 1.0f };
		float     m_focusTintScale = 1.1f;

		std::function<void(const std::string&)> m_onTextChanged;
		std::function<void(const std::string&)> m_onSubmit;
	};

}
