#include "vepch.h"
#include "DeltaTime.h"

namespace ve {

    DeltaTime::DeltaTime()
        : m_startTime(Clock::now())
        , m_lastFrameTime(Clock::now())
        , m_deltaTime(0.0f) {
    }

    void DeltaTime::update() {
        auto now = Clock::now();
        m_deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
        m_lastFrameTime = now;
    }

    float DeltaTime::getDeltaTime() const {
        return m_deltaTime;
    }

    float DeltaTime::getCurrentTime() const {
        return std::chrono::duration<float>(Clock::now() - m_startTime).count();
    }

}
