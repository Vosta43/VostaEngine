#include "vepch.h"
#include "Window.h"
#include "Events/EventDispatcher.h"
#include "Events/KeyPressedEvent.h"
#include "Events/KeyReleasedEvent.h"
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

    private:
        int m_width;
        int m_height;
        GLFWwindow* m_window;
        GraphicsContext* m_context;
        EventDispatcher* m_dispatcher;
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

}