#ifndef GUI_H
#define GUI_H

#include "../Core/Layers/Layers.h"
#include "Renderer/FrameBuffer.h"

namespace ve {
	
	class VE_API GuiLayer : public Layer {
	public:
		GuiLayer();
		~GuiLayer();

		void onAttach();
		void onDetach();
		void renderViewportSettings();
		void setDarkThemeColors();
		void onUpdate();
		void onUIRender() override;
		void onEvent(Event& event) override;

		void setViewportBounds(const glm::vec2& pos, const glm::vec2& size);
		// First is Pos and second is Size
		std::pair<glm::vec2, glm::vec2> getViewportBounds() {return { m_viewportPos ,m_viewportSize }; }
		bool isViewportHovered() const { return m_isViewportHovered; }
		void setViewportWindowHovered(bool hovered) { m_isViewportWindowHovered = hovered; }

		void begin();
		void end();

		//hack
		Ref<Framebuffer> getFramebuffer() { return m_framebuffer; }

    protected:


	private:
		glm::vec2 m_viewportPos = glm::vec2(0.0f);
		glm::vec2 m_viewportSize = glm::vec2(0.0f);
		bool m_isViewportHovered = false;
		bool m_isViewportWindowHovered = false;

		Ref<Framebuffer> m_framebuffer;
	};


}



#endif // !GUI_H

