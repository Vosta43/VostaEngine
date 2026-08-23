#ifndef KEYPRESSEDEVENT_H
#define KEYPRESSEDEVENT_H

#include "Event.h"

namespace ve {

    class KeyPressedEvent : public Event {
    public:
        KeyPressedEvent(int keyCode, int repeatCount = 0)
            : m_keyCode(keyCode), m_repeatCount(repeatCount) {
        }

        EVENT_CLASS_TYPE(KeyPressed)
        EVENT_CLASS_CATEGORY(static_cast<int>(Category::Keyboard) |
                static_cast<int>(Category::Input))

        int getKeyCode() const { return m_keyCode; }
        int getRepeatCount() const { return m_repeatCount; }

        std::string toString() const override {
            return "KeyPressedEvent: " + std::to_string(m_keyCode) +
                " (" + std::to_string(m_repeatCount) + " repeats)";
        }

    private:
        int m_keyCode;
        int m_repeatCount;
    };

}
#endif