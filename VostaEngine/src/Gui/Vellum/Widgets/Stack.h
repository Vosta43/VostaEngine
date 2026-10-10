#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"

#include <cstddef>
#include <vector>

namespace ve::vellum {

    // Absolute-positioning container. Each child keeps the size it measured — a
    // Stack never stretches its children, so overlapping is allowed and z-order is
    // child order. Placement is `anchor` first, `offset` second:
    //
    //   pos = slot.pos + (slot.size - childSize) * anchor + offset
    //
    // The anchor is normalised over the leftover space, so (0.5, 0.5) centres the
    // child, (1, 1) pins it to the bottom-right, and the default (0, 0) reproduces
    // plain top-left placement.
    class VE_API Stack : public Widget {
    public:
        // `offset` is in pixels, relative to this widget's top-left corner.
        // `anchor` is normalised 0..1 over the space left after the child's size.
        void add(const Ref<Widget>& child,
                 const glm::vec2& offset = glm::vec2(0.0f),
                 const glm::vec2& anchor = glm::vec2(0.0f));

        void arrange(const Rect& slot) override;

    protected:
        glm::vec2 computeDesiredSize() const override;

    private:
        glm::vec2 offsetFor(std::size_t index) const;
        glm::vec2 anchorFor(std::size_t index) const;

        std::vector<glm::vec2> m_offsets;  // parallel to children()
        std::vector<glm::vec2> m_anchors;  // parallel to children()
    };

}
