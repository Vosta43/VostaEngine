#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Animated.h"
#include "Gui/Vellum/Core/DrawList.h"
#include "Gui/Vellum/Core/Reply.h"
#include "Gui/Vellum/Core/Widget.h"

#include <functional>

namespace ve::vellum {

    // Appearance of one button, held by value on the widget. A state left with a
    // zero texture reuses the normal state's art, so only states that need
    // distinct art set one. Tints multiply the texture.
    struct ButtonStyle {
        TextureId normalTexture  = 0;
        TextureId hoveredTexture = 0;
        TextureId pressedTexture = 0;

        glm::vec4 normalTint { 1.0f };
        glm::vec4 hoveredTint{ 1.0f };
        glm::vec4 pressedTint{ 1.0f };

        // One base colour: hover/pressed tints derive from it (the old look).
        static ButtonStyle fromColor(const glm::vec4& base) {
            ButtonStyle style;
            style.normalTint  = base;
            style.hoveredTint = glm::min(base * 1.35f, glm::vec4(1.0f, 1.0f, 1.0f, base.a));
            style.pressedTint = glm::vec4(base.r * 0.7f, base.g * 0.7f, base.b * 0.7f, base.a);
            return style;
        }

        // One texture for every state: hover keeps the tint, press darkens 15%
        // so a single-image button still reacts.
        static ButtonStyle fromTexture(TextureId texture, const glm::vec4& tint = glm::vec4(1.0f)) {
            ButtonStyle style;
            style.normalTexture = texture;
            style.normalTint  = tint;
            style.hoveredTint = tint;
            style.pressedTint = glm::vec4(tint.r * 0.85f, tint.g * 0.85f, tint.b * 0.85f, tint.a);
            return style;
        }
    };

    // Clickable rect drawn from a per-state texture + tint. Hover and press fade
    // their tints; the texture snaps with the discrete state, so a one-art style
    // animates while three-art styles swap instantly. Capturing the pointer is
    // what makes press-then-drag-out cancel, the same contract as a Slate SButton.
    class VE_API Button : public Widget {
    public:
        enum class State { Normal, Hovered, Pressed };

        explicit Button(const glm::vec2& size,
                        const glm::vec4& color = glm::vec4(0.23f, 0.25f, 0.29f, 1.0f));

        // Resizes the button and drops any cached layout up the tree.
        void setSize(const glm::vec2& size);

        // Whole appearance, copied. Editing the source afterwards does not reach
        // this button.
        void setStyle(const ButtonStyle& style) { m_style = style; }
        const ButtonStyle& style() const { return m_style; }

        // Per-state art; 0 reuses the normal state's texture.
        void setTexture(State state, TextureId texture);

        // Colour-only sugar over the style's tints.
        void setColor(const glm::vec4& color)        { m_style.normalTint  = color; }
        void setHoverColor(const glm::vec4& color)   { m_style.hoveredTint = color; }
        void setPressedColor(const glm::vec4& color) { m_style.pressedTint = color; }

        void setOnClick(std::function<void()> fn) { m_onClick = std::move(fn); }

        // Fade speed of the hover/press transition, in e-folds per second.
        // <= 0 restores the instant switch.
        void setTransitionRate(float rate) {
            m_hoverAnim.rate = rate;
            m_pressAnim.rate = rate;
        }

        void arrange(const Rect& slot) override;
        void onPaint(PaintContext& ctx) override;
        void tick(float dt) override;

        Reply onMouseDown(const glm::vec2& pos) override;
        Reply onMouseUp(const glm::vec2& pos) override;

    protected:
        glm::vec2 computeDesiredSize() const override;

    private:
        ButtonStyle m_style;
        glm::vec2 m_size{ 0.0f };
        std::function<void()> m_onClick;
        AnimFloat m_hoverAnim;   // 0 = normal, 1 = hovered
        AnimFloat m_pressAnim;   // 0 = released, 1 = pressed
    };

}
