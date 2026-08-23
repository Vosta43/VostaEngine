#ifndef KEYRELEASEDEVENT_H
#define KEYRELEASEDEVENT_H

#include "Event.h"

namespace ve {

    class KeyReleasedEvent : public Event {
    public:
        KeyReleasedEvent(int keyCode)
            : m_keyCode(keyCode) {
        }

        EVENT_CLASS_TYPE(KeyReleased)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Keyboard) |
                static_cast<int>(Category::Input))

            int getKeyCode() const { return m_keyCode; }

        std::string toString() const override {
            return "KeyReleasedEvent: " + std::to_string(m_keyCode);
        }

    private:
        int m_keyCode;
    };

}
#endif