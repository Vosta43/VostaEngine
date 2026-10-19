#include "vepch.h"
#include "VellumLayer.h"

#include "Core/Application.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Core/Window.h"
#include "Input.h"
#include "Core/Events/EventDispatcher.h"
#include "Core/Events/KeyPressedEvent.h"
#include "Core/Events/KeyTypedEvent.h"
#include "Core/Events/MouseButtonPressedEvent.h"
#include "Core/Events/MouseButtonReleasedEvent.h"
#include "Core/Events/MouseScrolledEvent.h"
#include "MouseButtonCodes.h"

#include <utility>

namespace ve::vellum {

    void VellumLayer::onAttach() {
        // The GL context is live by the time a layer attaches (the window builds it),
        // so atlases can be created (and grow) right here and from any later request.
        m_fonts.init(&m_tree.renderer(),
                     toAbsolute("VostaEngine/resources/fonts/NotoSans-Regular-2.ttf"));
        if (!m_fonts.defaultFont())
            VE_CORE_ERROR_PRINT("Vellum: default font failed to load; text will not render");

        m_dispatcher = Application::get().getDispatcher();
        if (!m_dispatcher)
            return;

        m_dispatcher->subscribe(Event::Type::MouseButtonPressed, [this](Event& e) {
            if (static_cast<MouseButtonPressedEvent&>(e).getButton() != VE_MOUSE_BUTTON_LEFT)
                return;
            m_justDown = true;
            m_buttonDown = true;
        });
        m_dispatcher->subscribe(Event::Type::MouseButtonReleased, [this](Event& e) {
            if (static_cast<MouseButtonReleasedEvent&>(e).getButton() != VE_MOUSE_BUTTON_LEFT)
                return;
            m_justUp = true;
            m_buttonDown = false;
        });
        m_dispatcher->subscribe(Event::Type::MouseScrolled, [this](Event& e) {
            const auto& scrolled = static_cast<MouseScrolledEvent&>(e);
            m_wheel += scrolled.getYOffset();

            const auto mouse = Input::getMousePosition();
            if (m_tree.wantsWheel(glm::vec2(mouse.first, mouse.second)))
                e.setHandled(true);
        });
        m_dispatcher->subscribe(Event::Type::KeyPressed, [this](Event& e) {
            const auto& k = static_cast<KeyPressedEvent&>(e);
            m_keys.push_back({ KeyInput::Kind::Down, k.getKeyCode(), k.getRepeatCount(), 0 });
        });
        m_dispatcher->subscribe(Event::Type::KeyTyped, [this](Event& e) {
            const auto& t = static_cast<KeyTypedEvent&>(e);
            m_keys.push_back({ KeyInput::Kind::Text, 0, 0,
                               static_cast<uint32_t>(t.getKeyCode()) });
        });
    }

    void VellumLayer::onDetach() {
        m_dispatcher = nullptr;
    }

    void VellumLayer::onUIRender() {
        const Window* window = Application::get().getWindow();

        InputFrame input;
        const auto mouse = Input::getMousePosition();  // authoritative; edges may be stale
        input.pos = { mouse.first, mouse.second };
        input.justDown = m_justDown;
        input.justUp = m_justUp;
        input.buttonDown = m_buttonDown;
        input.wheel = m_wheel;
        input.keys = std::move(m_keys);

        m_tree.frame(glm::vec2(static_cast<float>(window->getWidth()),
                               static_cast<float>(window->getHeight())),
                     input);

        m_justDown = false;
        m_justUp = false;
        m_wheel = 0.0f;
        m_keys.clear();
    }

}
