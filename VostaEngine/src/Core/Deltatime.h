#pragma once

#include "Core.h"

namespace ve {

    class VE_API DeltaTime {
    public:
        static DeltaTime& get() {
            static DeltaTime instance;
            return instance;
        }

        void update();
        float getDeltaTime() const;
        float getCurrentTime() const;

    private:
        DeltaTime();

        float m_lastFrameTime;
        float m_deltaTime;
    };

}