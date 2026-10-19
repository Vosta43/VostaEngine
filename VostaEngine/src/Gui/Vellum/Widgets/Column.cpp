#include "vepch.h"
#include "Column.h"

namespace ve::vellum {

    void Column::setSpacing(float spacing) {
        if (spacing == m_spacing)
            return;
        m_spacing = spacing;
        invalidateDesiredSize();
    }

    glm::vec2 Column::computeDesiredSize() const {
        glm::vec2 size{ 0.0f };
        for (const auto& child : m_children) {
            const glm::vec2 childSize = child->desiredSize();
            size.x = glm::max(size.x, childSize.x);
            size.y += childSize.y;
        }
        if (m_children.size() > 1)
            size.y += m_spacing * static_cast<float>(m_children.size() - 1);
        return size;
    }

    void Column::arrange(const Rect& slot) {
        m_rect = slot;

        float y = slot.pos.y;
        for (const auto& child : m_children) {
            const float height = child->desiredSize().y;
            child->arrange({ { slot.pos.x, y }, { slot.size.x, height } });
            y += height + m_spacing;
        }
    }

}
