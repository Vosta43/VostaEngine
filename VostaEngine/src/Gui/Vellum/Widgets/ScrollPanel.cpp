#include "vepch.h"
#include "ScrollPanel.h"

#include "Gui/Vellum/Core/PaintContext.h"

#include <algorithm>

namespace ve::vellum {

    namespace {
        constexpr float kScrollPixelsPerNotch = 48.0f;
    }

    ScrollPanel::ScrollPanel(const glm::vec2& size)
        : m_size(size) {}

    void ScrollPanel::setSize(const glm::vec2& size) {
        m_size = size;
        invalidateDesiredSize();
    }

    void ScrollPanel::setScrollOffset(float offset) {
        m_scrollY = std::max(0.0f, offset);
    }

    glm::vec2 ScrollPanel::computeDesiredSize() const {
        glm::vec2 content{ 0.0f };
        if (!m_children.empty())
            content = m_children.front()->desiredSize();

        return {
            m_size.x > 0.0f ? m_size.x : content.x,
            m_size.y > 0.0f ? m_size.y : content.y
        };
    }

    void ScrollPanel::arrange(const Rect& slot) {
        m_rect = slot;
        layoutContent();
    }

    void ScrollPanel::layoutContent() {
        if (m_children.empty())
            return;

        Widget& content = *m_children.front();
        const float contentHeight = std::max(m_rect.size.y, content.desiredSize().y);
        m_maxScroll = std::max(0.0f, contentHeight - m_rect.size.y);
        m_scrollY = std::clamp(m_scrollY, 0.0f, m_maxScroll);

        content.arrange({
            { m_rect.pos.x, m_rect.pos.y - m_scrollY },
            { m_rect.size.x, contentHeight }
        });
    }

    void ScrollPanel::paint(PaintContext& ctx) {
        ctx.pushClip(m_rect);
        if (m_background.a > 0.0f)
            ctx.fillRect(m_rect, m_background);
        Widget::paint(ctx);
        ctx.popClip();
    }

    Reply ScrollPanel::onMouseWheel(float delta) {
        const float next = std::clamp(m_scrollY - delta * kScrollPixelsPerNotch,
                                      0.0f, m_maxScroll);
        if (next == m_scrollY)
            return Reply::unhandled();

        m_scrollY = next;
        layoutContent();
        return Reply::handle();
    }

}
