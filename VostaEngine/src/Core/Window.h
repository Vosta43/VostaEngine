#ifndef WINDOW_H
#define WINDOW_H

#include "Renderer/GraphicsContext.h"
#include "Core.h"

#include <string>

struct GLFWwindow;

namespace ve {

    class EventDispatcher;

    // Hidden keeps the OS cursor position valid (hover still works); Disabled
    // makes it virtual and unbounded -- the mode a first-person camera needs.
    enum class CursorMode {
        Normal,
        Hidden,
        Disabled
    };

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

        void setCursorMode(CursorMode mode);
        CursorMode getCursorMode() const;

        // Window chrome a host or game module may rename/rebrand at runtime. The
        // title is UTF-8. The icon path resolves like any asset (loose or packed);
        // false on a load failure, which leaves the current icon untouched.
        void setTitle(const std::string& title);
        bool setIcon(const std::string& imagePath);

        // Raw motion bypasses OS pointer acceleration and only takes effect while
        // the cursor is Disabled. Returns false if the platform cannot provide it.
        bool setRawMouseMotion(bool enabled);
        bool isRawMouseMotionEnabled() const;

        int getWidth() const;
        int getHeight() const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}

#endif