#ifndef WINDOWRESIZEEVENT_H
#define WINDOWRESIZEEVENT_H

#include "Event.h"

namespace ve {

    class WindowResizeEvent : public Event {
    public:
        WindowResizeEvent(int width, int height)
            : m_width(width), m_height(height) {
        }

        EVENT_CLASS_TYPE(WindowResize)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Window))

            int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }

        std::string toString() const override {
            return "WindowResizeEvent: " + std::to_string(m_width) +
                ", " + std::to_string(m_height);
        }

    private:
        int m_width;
        int m_height;
    };

}
#endif