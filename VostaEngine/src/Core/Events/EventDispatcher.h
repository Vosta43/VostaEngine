#ifndef EVENTDISPATCHER_H
#define EVENTDISPATCHER_H

#include "Event.h"
#include <functional>
#include <vector>
#include <unordered_map>

namespace ve {

    using EventCallback = std::function<void(Event&)>;

    class EventDispatcher {
    public:
        //EventDispatcher(Event& event)
        //    : m_Event(event) {
        //}
        EventDispatcher() { }
        void subscribe(Event::Type type, EventCallback callback) {
            m_listeners[type].push_back(callback);
        }

        void dispatch(Event& event) {
            auto it = m_listeners.find(event.getType());
            if (it != m_listeners.end()) {
                for (auto& callback : it->second) {
                    callback(event);
                    if (event.isHandled())
                        break;
                }
            }
        }

        void dispatch(Event&& event) {
            dispatch(event);  
        }

        void clear() {
            m_listeners.clear();
        }

    private:
        std::unordered_map<Event::Type, std::vector<EventCallback>> m_listeners;
        //Event& m_Event;
    };

}

#endif