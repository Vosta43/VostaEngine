#pragma once

#include "Core.h"

#include <chrono>

namespace ve {

    class VE_API DeltaTime {
    public:
        using Clock = std::chrono::steady_clock;

        static DeltaTime& get() {
            static DeltaTime instance;
            return instance;
        }

        void update();
        float getDeltaTime() const;
        float getCurrentTime() const;

    private:
        DeltaTime();

        std::chrono::time_point<Clock> m_startTime;
        std::chrono::time_point<Clock> m_lastFrameTime;
        float m_deltaTime;
    };

}
