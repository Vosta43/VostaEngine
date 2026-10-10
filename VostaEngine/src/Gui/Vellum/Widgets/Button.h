#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Reply.h"
#include "Gui/Vellum/Core/Widget.h"

#include <functional>

namespace ve::vellum {

    // Clickable filled rect. Picks one of three colours (normal / hovered /
    // pressed) and fires `onClick` when the button is released inside its own
    // bounds. Capturing the pointer is what makes press-then-drag-out cancel,
    // the same contract as a Slate SButton.
    class VE_API Button : public Widget {
    public:
        explicit Button(const glm::vec2& size,
                        const glm::vec4& color = glm::vec4(0.23f, 0.25f, 0.29f, 1.0f));

        void setColor(const glm::vec4& color)        { m_color = color; }
        void setHoverColor(const glm::vec4& color)   { m_hoverColor = color; }
        void setPressedColor(const glm::vec4& color) { m_pressedColor = color; }

        void setOnClick(std::function<void()> fn) { m_onClick = std::move(fn); }

        void arrange(const Rect& slot) override;
        void paint(PaintContext& ctx) override;

        Reply onMouseDown(const glm::vec2& pos) override;
        Reply onMouseUp(const glm::vec2& pos) override;

    protected:
        glm::vec2 computeDesiredSize() const override;

    private:
        glm::vec4 m_color;
        glm::vec4 m_hoverColor;
        glm::vec4 m_pressedColor;
        glm::vec2 m_size{ 0.0f };
        std::function<void()> m_onClick;
    };

}
