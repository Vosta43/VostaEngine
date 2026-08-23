#ifndef MOUSESCROLLEDEVENT_H
#define MOUSESCROLLEDEVENT_H

#include "Event.h"

namespace ve {

    class MouseScrolledEvent : public Event {
    public:
        MouseScrolledEvent(float xOffset, float yOffset)
            : m_xOffset(xOffset), m_yOffset(yOffset) {
        }

        EVENT_CLASS_TYPE(MouseScrolled)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Mouse) |
                static_cast<int>(Category::Input))

        float getXOffset() const { return m_xOffset; }
        float getYOffset() const { return m_yOffset; }

        std::string toString() const override {
            return "MouseScrolledEvent: " + std::to_string(m_xOffset) +
                ", " + std::to_string(m_yOffset);
        }

    private:
        float m_xOffset;
        float m_yOffset;
    };

}
#endif