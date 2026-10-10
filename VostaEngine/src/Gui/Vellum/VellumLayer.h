#pragma once

#include "Core/Core.h"
#include "Core/Layers/Layers.h"
#include "Text/FontManager.h"
#include "WidgetTree.h"

namespace ve {
    class EventDispatcher;
}

namespace ve::vellum {

    // Engine-owned UI layer — the Vellum counterpart of GuiLayer. It owns the widget
    // tree, samples mouse input, and runs the whole frame for it during the UI phase;
    // content layers hand it a root from their onAttach().
    //
    // It subscribes to the same EventDispatcher as GuiLayer and is registered after
    // it, so events GuiLayer marks handled (ImGui wants the mouse) never reach here
    // — ImGui's priority comes for free. Releases are never blocked upstream, so the
    // router must guard them with its own capture state.
    class VE_API VellumLayer : public Layer {
    public:
        void onAttach() override;
        void onDetach() override;

        void setRoot(const Ref<Widget>& root) { m_tree.setRoot(root); }
        Widget* root() const { return m_tree.root(); }

        // The engine's default UI font, borrowed by TextBlocks. Null if unavailable.
        FontAtlas* defaultFont() { return m_fonts.defaultFont(); }
        // The default face at an arbitrary pixel size, created on first request.
        FontAtlas* font(float pixelSize) { return m_fonts.font(pixelSize); }

        void onUIRender() override;

    private:
        WidgetTree m_tree;
        FontManager m_fonts;
        ve::EventDispatcher* m_dispatcher = nullptr;

        // Button edges latched by the event callbacks and consumed once per frame.
        bool m_justDown = false;
        bool m_justUp = false;
        bool m_buttonDown = false;
    };

}
