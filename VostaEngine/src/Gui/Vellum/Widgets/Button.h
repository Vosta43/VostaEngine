#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Animated.h"
#include "Gui/Vellum/Core/Reply.h"
#include "Gui/Vellum/Core/Widget.h"

#include <functional>

namespace ve::vellum {

    // Clickable filled rect. Hover and press fade between three colours (normal /
    // hovered / pressed) and `onClick` fires when the button is released inside its
    // own bounds. Capturing the pointer is what makes press-then-drag-out cancel,
    // the same contract as a Slate SButton.
    class VE_API Button : public Widget {
    public:
        explicit Button(const glm::vec2& size,
                        const glm::vec4& color = glm::vec4(0.23f, 0.25f, 0.29f, 1.0f));

        // Resizes the button and drops any cached layout up the tree.
        void setSize(const glm::vec2& size);

        void setColor(const glm::vec4& color)        { m_color = color; }
        void setHoverColor(const glm::vec4& color)   { m_hoverColor = color; }
        void setPressedColor(const glm::vec4& color) { m_pressedColor = color; }

        void setOnClick(std::function<void()> fn) { m_onClick = std::move(fn); }

        // Fade speed of the hover/press transition, in e-folds per second.
        // <= 0 restores the instant colour switch.
        void setTransitionRate(float rate) {
            m_hoverAnim.rate = rate;
            m_pressAnim.rate = rate;
        }

        void arrange(const Rect& slot) override;
        void paint(PaintContext& ctx) override;
        void tick(float dt) override;

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
        AnimFloat m_hoverAnim;   // 0 = normal, 1 = hovered
        AnimFloat m_pressAnim;   // 0 = released, 1 = pressed
    };

}
