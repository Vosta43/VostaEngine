#include "vepch.h"
#include "Window.h"
#include "Log.h"
#include "Asset/TextureImporter.h"
#include "Core/AssetConfig.h"
#include "Events/EventDispatcher.h"
#include "Events/KeyPressedEvent.h"
#include "Events/KeyReleasedEvent.h"
#include "Events/KeyTypedEvent.h"
#include "Events/MouseMovedEvent.h"
#include "Events/MouseScrolledEvent.h"
#include "Events/MouseButtonPressedEvent.h"
#include "Events/MouseButtonReleasedEvent.h"
#include "Events/WindowResizeEvent.h"
#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>

namespace ve {

    class Window::Impl {
    public:
        Impl(int width, int height, const char* title, EventDispatcher* dispatcher)
            : m_width(width)
            , m_height(height)
            , m_window(nullptr)
            , m_dispatcher(dispatcher) {

            glfwInit();

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

            m_context = new OpenGLContext(m_window);
            m_context->init();

            setupCallbacks();
        }

        Impl() {
            if (m_context) {
                delete m_context;
                m_context = nullptr;
            }
            if (m_window) {
                glfwDestroyWindow(m_window);
                m_window = nullptr;
            }
            glfwTerminate();
        }

        void setupCallbacks() {
            glfwSetWindowUserPointer(m_window, this);

            glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                auto* impl = static_cast<Impl*>(glfwGetWindowUserPointer(window));
                if (impl && impl->m_dispatcher) {
                    if (action == GLFW_PRESS) {
                        impl->m_dispatcher->dispatch(KeyPressedEvent(key, 0));
                    }
                    else if (action == GLFW_REPEAT) {
                        impl->m_dispatcher->dispatch(KeyPressedEvent(key, 1));
                    }
                    else if (action == GLFW_RELEASE) {
                        impl->m_dispatcher->dispatch(KeyReleasedEvent(key));
                    }
                }
                });

            // Must be installed before ImGui initialises: its backend chains only to
            // callbacks that were already set when it installed its own.
            glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) {
                auto* impl = static_cast<Impl*>(glfwGetWindowUserPointer(window));
                if (impl && impl->m_dispatcher) {
                    impl->m_dispatcher->dispatch(KeyTypedEvent(static_cast<int>(codepoint)));
                }
                });

            glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
                auto* impl = static_cast<Impl*>(glfwGetWindowUserPointer(window));
                if (impl && impl->m_dispatcher) {
                    impl->m_dispatcher->dispatch(MouseMovedEvent(static_cast<float>(xpos),
                        static_cast<float>(ypos)));
                }
                });

            glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
                auto* impl = static_cast<Impl*>(glfwGetWindowUserPointer(window));
                if (impl && impl->m_dispatcher) {
                    impl->m_dispatcher->dispatch(MouseScrolledEvent(static_cast<float>(xoffset),
                        static_cast<float>(yoffset)));
                }
                });

            glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
                auto* impl = static_cast<Impl*>(glfwGetWindowUserPointer(window));
                if (impl && impl->m_dispatcher) {
                    if (action == GLFW_PRESS) {
                        impl->m_dispatcher->dispatch(MouseButtonPressedEvent(button));
                    }
                    else if (action == GLFW_RELEASE) {
                        impl->m_dispatcher->dispatch(MouseButtonReleasedEvent(button));
                    }
                }
                });

            glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
                auto* impl = static_cast<Impl*>(glfwGetWindowUserPointer(window));
                impl->m_width = width;
                impl->m_height = height;
                if (impl->m_dispatcher) {
                    impl->m_dispatcher->dispatch(WindowResizeEvent(width, height));
                }
                });
        }

        GLFWwindow* getNativeWindow() const {
            return m_window;
        }

        void init() {
        }

        void processEvents() {
            glfwPollEvents();
        }

        void swapBuffers() {
            glfwSwapBuffers(m_window);
        }

        void setVSync(bool enabled) {
            glfwSwapInterval(enabled ? 1 : 0);
        }

        bool shouldClose() const {
            return glfwWindowShouldClose(m_window);
        }

        int getWidth() const {
            return m_width;
        }

        int getHeight() const {
            return m_height;
        }

        void setCursorMode(CursorMode mode) {
            m_cursorMode = mode;
            glfwSetInputMode(m_window, GLFW_CURSOR,
                mode == CursorMode::Normal ? GLFW_CURSOR_NORMAL
                : mode == CursorMode::Hidden ? GLFW_CURSOR_HIDDEN
                : GLFW_CURSOR_DISABLED);
            applyRawMouseMotion();
        }

        CursorMode getCursorMode() const {
            return m_cursorMode;
        }

        void setTitle(const std::string& title) {
            glfwSetWindowTitle(m_window, title.c_str());
        }

        void setIcon(uint32_t width, uint32_t height, const void* rgba) {
            GLFWimage image{ static_cast<int>(width), static_cast<int>(height),
                             const_cast<unsigned char*>(static_cast<const unsigned char*>(rgba)) };
            glfwSetWindowIcon(m_window, 1, &image);
        }

        bool setRawMouseMotion(bool enabled) {
            if (enabled && !glfwRawMouseMotionSupported()) {
                VE_CORE_WARN_PRINT("%s", "Window: raw mouse motion is not supported on this platform");
                return false;
            }
            m_rawMouseMotionWanted = enabled;
            applyRawMouseMotion();
            return true;
        }

        bool isRawMouseMotionEnabled() const {
            return glfwGetInputMode(m_window, GLFW_RAW_MOUSE_MOTION) == GLFW_TRUE;
        }

    private:
        // Raw motion is inert unless the cursor is Disabled; keep the request
        // remembered so entering Disabled later turns it on without a second call.
        void applyRawMouseMotion() {
            const bool enabled = m_rawMouseMotionWanted
                && m_cursorMode == CursorMode::Disabled
                && glfwRawMouseMotionSupported();
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, enabled ? GLFW_TRUE : GLFW_FALSE);
        }

        int m_width;
        int m_height;
        GLFWwindow* m_window;
        GraphicsContext* m_context;
        EventDispatcher* m_dispatcher;
        CursorMode m_cursorMode = CursorMode::Normal;
        bool m_rawMouseMotionWanted = false;
    };

    Window::Window(int width, int height, const char* title, EventDispatcher* dispatcher)
        : m_impl(std::make_unique<Impl>(width, height, title, dispatcher)) {
    }

    Window::~Window() = default;

    Window::Window(Window&&) noexcept = default;
    Window& Window::operator=(Window&&) noexcept = default;

    GLFWwindow* Window::getNativeWindow() const {
        return m_impl->getNativeWindow();
    }

    void Window::init() {
        m_impl->init();
    }

    void Window::processEvents() {
        m_impl->processEvents();
    }

    void Window::swapBuffers() {
        m_impl->swapBuffers();
    }

    void Window::setVSync(bool enabled) {
        m_impl->setVSync(enabled);
    }

    bool Window::shouldClose() const {
        return m_impl->shouldClose();
    }

    int Window::getWidth() const {
        return m_impl->getWidth();
    }

    int Window::getHeight() const {
        return m_impl->getHeight();
    }

    void Window::setCursorMode(CursorMode mode) {
        m_impl->setCursorMode(mode);
    }

    CursorMode Window::getCursorMode() const {
        return m_impl->getCursorMode();
    }

    void Window::setTitle(const std::string& title) {
        m_impl->setTitle(title);
    }

    bool Window::setIcon(const std::string& imagePath) {
        if (imagePath.empty())
            return false;

        Ref<TextureResource> image = TextureImporter::importFromFile(toAbsolute(imagePath));
        if (!image || !image->floatPixels.empty() || image->width == 0 || image->height == 0) {
            VE_CORE_ERROR_PRINT("Window: cannot load icon '%s'", imagePath.c_str());
            return false;
        }

        const size_t pixelCount = static_cast<size_t>(image->width) * image->height;
        if (pixelCount == 0 || image->pixels.size() % pixelCount != 0) {
            VE_CORE_ERROR_PRINT("Window: malformed icon pixels '%s'", imagePath.c_str());
            return false;
        }
        const size_t stride = image->pixels.size() / pixelCount;   // R8, RGB or RGBA
        if (stride != 1 && stride != 3 && stride != 4) {
            VE_CORE_ERROR_PRINT("Window: unsupported icon format '%s'", imagePath.c_str());
            return false;
        }

        // The importer stores rows bottom-up for GL; GLFW wants RGBA top-down.
        std::vector<uint8_t> rgba(pixelCount * 4);
        for (uint32_t y = 0; y < image->height; ++y) {
            const uint8_t* src = image->pixels.data()
                + (static_cast<size_t>(image->height) - 1 - y) * image->width * stride;
            uint8_t* dst = rgba.data() + static_cast<size_t>(y) * image->width * 4;
            for (uint32_t x = 0; x < image->width; ++x, src += stride, dst += 4) {
                if (stride == 1) {
                    dst[0] = dst[1] = dst[2] = src[0];
                    dst[3] = 255;
                }
                else {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = stride == 4 ? src[3] : 255;
                }
            }
        }

        m_impl->setIcon(image->width, image->height, rgba.data());
        return true;
    }

    bool Window::setRawMouseMotion(bool enabled) {
        return m_impl->setRawMouseMotion(enabled);
    }

    bool Window::isRawMouseMotionEnabled() const {
        return m_impl->isRawMouseMotionEnabled();
    }

}