#ifndef WINDOWCLOSEEVENT_H
#define WINDOWCLOSEEVENT_H

#include "Event.h"

namespace ve {

    class WindowCloseEvent : public Event {
    public:
        WindowCloseEvent() {
        }

        EVENT_CLASS_TYPE(WindowClose)
            EVENT_CLASS_CATEGORY(static_cast<int>(Category::Window))

            std::string toString() const override {
            return "WindowCloseEvent";
        }
    };

}
#endif