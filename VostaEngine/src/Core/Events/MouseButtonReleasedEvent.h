#ifndef MOUSEBUTTONRELEASEDEVENT_H
#define MOUSEBUTTONRELEASEDEVENT_H

#include "Event.h"

namespace ve {

    class MouseButtonReleasedEvent : public Event {
    public:
        MouseButtonReleasedEvent(int button)
            : m_button(button) {
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Mouse) |
                static_cast<int>(Category::MouseButton) |
                static_cast<int>(Category::Input))

            int getButton() const { return m_button; }

        std::string toString() const override {
            return "MouseButtonReleasedEvent: " + std::to_string(m_button);
        }

    private:
        int m_button;
    };

}
#endif