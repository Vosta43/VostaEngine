#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"

namespace ve::vellum {

    class VE_API Column : public Widget {
    public:
        void setSpacing(float spacing);
        float spacing() const { return m_spacing; }

        void arrange(const Rect& slot) override;

    protected:
        glm::vec2 computeDesiredSize() const override;

    private:
        float m_spacing = 0.0f;
    };

}
