#ifndef KEYTYPEDEVENT_H
#define KEYTYPEDEVENT_H

#include "Event.h"

namespace ve {

    class KeyTypedEvent : public Event {
    public:
        KeyTypedEvent(int keyCode)
            : m_keyCode(keyCode) {
        }

        EVENT_CLASS_TYPE(KeyTyped)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Keyboard) |
                static_cast<int>(Category::Input))

            int getKeyCode() const { return m_keyCode; }

        std::string toString() const override {
            return "KeyTypedEvent: " + std::to_string(m_keyCode);
        }

    private:
        int m_keyCode;
    };

}
#endif