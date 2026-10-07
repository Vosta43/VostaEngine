#ifndef WINDOW_H
#define WINDOW_H

#include "Renderer/GraphicsContext.h"
#include "Core.h"

struct GLFWwindow;

namespace ve {

    class EventDispatcher; 

    class VE_API Window {
    public:
        Window(int width, int height, const char* title, EventDispatcher* dispatcher = nullptr);
        ~Window();

        Window(Window&&) noexcept;
        Window& operator=(Window&&) noexcept;

        GLFWwindow* getNativeWindow() const;

        void init();
        void processEvents();
        void swapBuffers();
        void setVSync(bool enabled);
        bool shouldClose() const;

        int getWidth() const;
        int getHeight() const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}

#endif