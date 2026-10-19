#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Reply.h"
#include "Gui/Vellum/Core/Widget.h"

namespace ve::vellum {

    class VE_API ScrollPanel : public Widget {
    public:
        explicit ScrollPanel(const glm::vec2& size = glm::vec2(0.0f));

        void setSize(const glm::vec2& size);
        void setBackgroundColor(const glm::vec4& color) { m_background = color; }

        void setScrollOffset(float offset);
        float scrollOffset() const { return m_scrollY; }
        float maxScrollOffset() const { return m_maxScroll; }

        void arrange(const Rect& slot) override;
        void paint(PaintContext& ctx) override;

        Reply onMouseWheel(float delta) override;
        bool canConsumeWheel() const override { return m_maxScroll > 0.0f; }

    protected:
        glm::vec2 computeDesiredSize() const override;

    private:
        void layoutContent();

        glm::vec2 m_size{ 0.0f };
        glm::vec4 m_background{ 0.0f };
        float     m_scrollY = 0.0f;
        float     m_maxScroll = 0.0f;
    };

}
