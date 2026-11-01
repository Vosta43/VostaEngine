#pragma once

#include "Core/Core.h"
#include "Core/InputFrame.h"
#include "Core/Layers/Layers.h"
#include "Text/FontManager.h"
#include "WidgetTree.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ve {
    class EventDispatcher;
    class Texture2D;
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
        FontAtlas* defaultFont() { return m_fonts.font(FontManager::kDefaultSize); }
        // The default face at an arbitrary pixel size, created on first request.
        FontAtlas* font(float pixelSize) { return m_fonts.font(pixelSize); }

        // Registers a named face chain (registered once; a repeated name returns the
        // existing font). Paths resolve like any asset. Null when no face loads.
        Font* addFont(const std::string& name, std::vector<std::string> faces);
        // A registered font at `pixelSize`. Null when `name` is unknown.
        FontAtlas* font(const std::string& name, float pixelSize);

        // Loads `path` as a UI texture on first request and registers it with the
        // renderer; `sizeOut` receives its pixel size. Returns 0 when the file does
        // not load. Paths resolve against the engine/project roots like any asset.
        TextureId texture(const std::string& path, glm::vec2& sizeOut);

        // Registration for textures the caller owns (a render target). Re-registering
        // the same id is how a recreated GL texture survives a resize.
        TextureId acquireTextureId();
        void registerTexture(TextureId id, const Ref<Texture2D>& texture);
        void releaseTexture(TextureId id);

        void onUIRender() override;

    private:
        WidgetTree m_tree;
        FontManager m_fonts;
        ve::EventDispatcher* m_dispatcher = nullptr;

        // Button edges latched by the event callbacks and consumed once per frame.
        bool m_justDown = false;
        bool m_justUp = false;
        bool m_buttonDown = false;
        float m_wheel = 0.0f;

        // Key events latched in arrival order; drained into the InputFrame each frame.
        std::vector<KeyInput> m_keys;

        // Loaded UI textures, keyed by the path they were requested with. The size
        // is cached alongside the id so a repeat request never touches the GPU.
        struct UiTexture {
            TextureId id = 0;
            glm::vec2 size{ 0.0f };
        };
        std::unordered_map<std::string, UiTexture> m_textures;
    };

}
