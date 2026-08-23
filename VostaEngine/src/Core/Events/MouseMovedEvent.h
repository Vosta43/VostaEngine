#ifndef MOUSEMOVEDEVENT_H
#define MOUSEMOVEDEVENT_H

#include "Event.h"

namespace ve {

    class MouseMovedEvent : public Event {
    public:
        MouseMovedEvent(float x, float y)
            : m_x(x), m_y(y) {
        }

        EVENT_CLASS_TYPE(MouseMoved)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Mouse) |
                static_cast<int>(Category::Input))

        float getX() const { return m_x; }
        float getY() const { return m_y; }

        std::string toString() const override {
            return "MouseMovedEvent: " + std::to_string(m_x) +
                ", " + std::to_string(m_y);
        }

    private:
        float m_x;
        float m_y;
    };

}
#endif