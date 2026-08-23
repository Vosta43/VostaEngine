#ifndef MOUSEBUTTONPRESSEDEVENT_H
#define MOUSEBUTTONPRESSEDEVENT_H

#include "Event.h"

namespace ve {

    class MouseButtonPressedEvent : public Event {
    public:
        MouseButtonPressedEvent(int button)
            : m_button(button) {
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Mouse) |
                static_cast<int>(Category::MouseButton) |
                static_cast<int>(Category::Input))

        int getButton() const { return m_button; }

        std::string toString() const override {
            return "MouseButtonPressedEvent: " + std::to_string(m_button);
        }

    private:
        int m_button;
    };

}
#endif