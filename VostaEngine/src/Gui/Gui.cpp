#include "vepch.h"
#include "Gui.h"

#define IMGUI_API __declspec(dllexport)
#define IMGUI_IMPL_API __declspec(dllexport)
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "../Core/Application.h"
#include "../Core/AssetConfig.h"
#include "../Core/Window.h"
#include "../Core/Events/EventDispatcher.h"
#include "../Core/Events/MouseButtonPressedEvent.h"
#include "../Core/Events/MouseButtonReleasedEvent.h"
#include "../Core/Events/MouseMovedEvent.h"
#include "../Core/Events/MouseScrolledEvent.h"
#include "../Core/Events/KeyPressedEvent.h"
#include "../Core/Events/KeyReleasedEvent.h"
#include "../Core/Events/KeyTypedEvent.h"
#include "../Core/Events/WindowResizeEvent.h"

#include "Renderer/CameraController.h"
#include "Renderer/RenderCommand.h"
#include "Keycodes.h"
#include "MouseButtonCodes.h"

#include <GLFW/glfw3.h>

namespace ve {

	GuiLayer::GuiLayer(){
	
	}

	GuiLayer::~GuiLayer(){
	
	}

    void GuiLayer::setViewportBounds(const glm::vec2& pos, const glm::vec2& size) {
        m_viewportPos = pos;
        m_viewportSize = size;
    }

	void GuiLayer::onAttach(){
		//Imgui::createContext();
		ImGui::CreateContext();
        VE_CORE_SUCCESS_PRINT("ImGui context (GuiLayer onAttach): %p", (void*)ImGui::GetCurrentContext());
		ImGui::StyleColorsDark();
		ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ImGui::GetMainViewport();

        Application& app = Application::get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.getWindow()->getNativeWindow());

        ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 410");

        EventDispatcher* dispatcher = Application::get().getDispatcher();
        dispatcher->subscribe(Event::Type::MouseButtonPressed, [this](Event& e) { onEvent(e); });
        dispatcher->subscribe(Event::Type::MouseButtonReleased, [this](Event& e) { onEvent(e); });
        dispatcher->subscribe(Event::Type::MouseMoved, [this](Event& e) { onEvent(e); });
        dispatcher->subscribe(Event::Type::MouseScrolled, [this](Event& e) { onEvent(e); });
        dispatcher->subscribe(Event::Type::KeyPressed, [this](Event& e) { onEvent(e); });
        dispatcher->subscribe(Event::Type::KeyReleased, [this](Event& e) { onEvent(e); });
        dispatcher->subscribe(Event::Type::KeyTyped, [this](Event& e) { onEvent(e); });
        dispatcher->subscribe(Event::Type::WindowResize, [this](Event& e) { onEvent(e); });


        setDarkThemeColors();
	}

	void GuiLayer::onDetach(){
		
	}

    void GuiLayer::renderViewportSettings() {
        //ImGui::Begin("Viewport Settings");

       
        //auto& camera = CameraController::getCamera();

        //float yaw = camera.getYaw();
        //float pitch = camera.getPitch();
        //glm::vec3 pos = camera.getPosition();

        //ImGui::Text("Camera: Yaw=%.1f  Pitch=%.1f", yaw, pitch);
        //ImGui::Text("Position: X=%.2f  Y=%.2f  Z=%.2f", pos.x, pos.y, pos.z);
        //ImGui::Separator();
        ////ImGui::SliderFloat("Move Speed", &m_moveSpeed, 0.1f, 10.0f, "%.2f");

        //ImGui::End();
    }

    void GuiLayer::setDarkThemeColors()
    {
        auto& colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };
        
        auto& io = ImGui::GetIO();
        io.FontGlobalScale = 1.4f; 
        
        std::string fontPath = toAbsolute("VostaEngine/resources/fonts/NotoSans-Regular-2.ttf");
        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
        if (font) {
            io.FontDefault = font;
        }

        // Headers
        colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Buttons
        colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Frame BG
        colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Tabs
        colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
        colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
        colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

        // Title
        colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    }

	void GuiLayer::onUpdate(){

		static bool show = true;

	}

    void GuiLayer::onImGuiRender()
    {
        
    }

    void GuiLayer::onEvent(Event& event) {
        ImGuiIO& io = ImGui::GetIO();

        switch (event.getType()) {
        case Event::Type::MouseButtonPressed: {
            auto& e = static_cast<MouseButtonPressedEvent&>(event);
            io.AddMouseButtonEvent(e.getButton(), true);
            break;
        }

        case Event::Type::MouseButtonReleased: {
            auto& e = static_cast<MouseButtonReleasedEvent&>(event);
            io.AddMouseButtonEvent(e.getButton(), false);
            break;
        }

        case Event::Type::MouseMoved: {
            auto& e = static_cast<MouseMovedEvent&>(event);
            io.AddMousePosEvent(e.getX(), e.getY());
            break;
        }

        case Event::Type::MouseScrolled: {
            auto& e = static_cast<MouseScrolledEvent&>(event);
            io.AddMouseWheelEvent(e.getXOffset(), e.getYOffset());
            break;
        }

        case Event::Type::KeyPressed: {
            
            auto& e = static_cast<KeyPressedEvent&>(event);
            //VE_CORE_ERROR_PRINT(e.toString().c_str());
            if (e.getKeyCode() > ImGuiKey_NamedKey_COUNT) {
                VE_CORE_ERROR_PRINT("KEYCODE ERROR");
                return;
            }
            io.KeysData[e.getKeyCode()].Down = true;
            //io.AddKeyEvent(e.getKeyCode(), false);
            break;
        }

        case Event::Type::KeyReleased: {
            auto& e = static_cast<KeyReleasedEvent&>(event);
            if (e.getKeyCode() > ImGuiKey_NamedKey_COUNT) {
                VE_CORE_ERROR_PRINT("KEYCODE ERROR");
                return;
            }
            io.KeysData[e.getKeyCode()].Down = false;
            break;
        }

        case Event::Type::KeyTyped: {
            auto& e = static_cast<KeyTypedEvent&>(event);
            io.AddInputCharacter(e.getKeyCode()); 
            break;
        }

        case Event::Type::WindowResize: {
            auto& e = static_cast<WindowResizeEvent&>(event);
            float w = (float)e.getWidth();
            float h = (float)e.getHeight();
            if (w > 0 && h > 0)
                io.DisplaySize = ImVec2(w, h);
            break;
        }

        default:
            break;
        }

        // Stop the event from propagating further to the game layer.
        // Mouse: only block press/scroll when a non-viewport ImGui window wants capture.
        // Release events always propagate so camera controls can deactivate properly.
        // Keyboard: always block when ImGui wants it.
        bool blockMouse = (event.getType() != Event::Type::MouseButtonReleased)
            && io.WantCaptureMouse && !m_isViewportWindowHovered;
        if (blockMouse || io.WantCaptureKeyboard) {
            event.setHandled(true);
        }

        if (event.getType() == Event::Type::MouseMoved) {
            auto& e = static_cast<MouseMovedEvent&>(event);
            float mx = e.getX();
            float my = e.getY();

            bool insideViewport = (mx >= m_viewportPos.x && mx <= m_viewportPos.x + m_viewportSize.x &&
                my >= m_viewportPos.y && my <= m_viewportPos.y + m_viewportSize.y);

            m_isViewportHovered = insideViewport && (!io.WantCaptureMouse || m_isViewportWindowHovered);
        }

    }

    void GuiLayer::begin(){
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x <= 0.0f) io.DisplaySize.x = 1.0f;
        if (io.DisplaySize.y <= 0.0f) io.DisplaySize.y = 1.0f;
        ImGui::NewFrame();
    }

    void GuiLayer::end(){
        ImGuiIO& io = ImGui::GetIO();
        Application& app = Application::get();
        float w = (float)app.getWindow()->getWidth();
        float h = (float)app.getWindow()->getHeight();
        if (w > 0.0f && h > 0.0f)
            io.DisplaySize = ImVec2(w, h);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }


}