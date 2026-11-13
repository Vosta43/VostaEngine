#ifndef EVENTDISPATCHER_H
#define EVENTDISPATCHER_H

#include "Event.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>
#include <unordered_map>

namespace ve {

    using EventCallback = std::function<void(Event&)>;

    class EventDispatcher {
    public:
        using SubscriptionId = uint64_t;

        EventDispatcher() { }

        // A host that keeps a per-view subscription (SceneView does) must be able
        // to drop it again: a destroyed view would otherwise leave a callback
        // pointing at freed memory.
        SubscriptionId subscribe(Event::Type type, EventCallback callback) {
            Listener listener;
            listener.id = ++m_nextId;
            listener.callback = std::move(callback);
            const SubscriptionId id = listener.id;
            m_listeners[type].push_back(std::move(listener));
            return id;
        }

        void unsubscribe(SubscriptionId id) {
            if (id == 0)
                return;

            for (auto& pair : m_listeners) {
                std::vector<Listener>& listeners = pair.second;
                for (Listener& listener : listeners) {
                    if (listener.id != id)
                        continue;

                    listener.removed = true;
                    // A removal that happens inside dispatch cannot erase: the
                    // dispatch loop owns the vector until it unwinds.
                    if (m_dispatchDepth == 0)
                        compact(listeners);
                    return;
                }
            }
        }

        void dispatch(Event& event) {
            auto it = m_listeners.find(event.getType());
            if (it == m_listeners.end())
                return;

            ++m_dispatchDepth;
            std::vector<Listener>& listeners = it->second;
            for (Listener& listener : listeners) {
                if (listener.removed)
                    continue;
                listener.callback(event);
                if (event.isHandled())
                    break;
            }
            --m_dispatchDepth;

            if (m_dispatchDepth == 0)
                compact(listeners);
        }

        void dispatch(Event&& event) {
            dispatch(event);
        }

        void clear() {
            m_listeners.clear();
        }

    private:
        struct Listener {
            SubscriptionId id = 0;
            EventCallback callback;
            bool removed = false;
        };

        static void compact(std::vector<Listener>& listeners) {
            listeners.erase(
                std::remove_if(listeners.begin(), listeners.end(),
                    [](const Listener& listener) { return listener.removed; }),
                listeners.end());
        }

        std::unordered_map<Event::Type, std::vector<Listener>> m_listeners;
        SubscriptionId m_nextId = 0;
        int m_dispatchDepth = 0;
    };

}

#endif
