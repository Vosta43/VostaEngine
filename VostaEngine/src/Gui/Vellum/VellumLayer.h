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

        // Replacing the tree abandons every widget the old one tracked, so a press or
        // release latched earlier this frame but not yet routed goes with it: the
        // widget it was aimed at is gone, and the one now under the cursor was not
        // there when the button went down. The router drops its capture on a swap for
        // the same reason.
        void setRoot(const Ref<Widget>& root) {
            if (root.get() != m_tree.root()) {
                m_justDown = false;
                m_justUp = false;
            }
            m_tree.setRoot(root);
        }
        Widget* root() const { return m_tree.root(); }

        // A widget painted after the tree and its tooltip, laid out against the same
        // design size and never hit-tested. It is not part of the tree, so it keeps
        // painting across root swaps: a banner that outlives every screen's content.
        // Null clears it.
        void setOverlay(const Ref<Widget>& overlay) { m_tree.setOverlay(overlay); }
        Widget* overlay() const { return m_tree.overlay(); }

        // Design resolution the tree lays out in. {0,0} (the default) keeps layout
        // in window pixels. When set, the root is arranged once at this size and the
        // whole UI is scaled and centred to fit the window, so resizing never
        // rebuilds the tree.
        void setDesignSize(const glm::vec2& size) { m_designSize = size; }
        const glm::vec2& designSize() const { return m_designSize; }

        // Full-window backdrop painted before the tree. Unlike widgets it ignores
        // the design-space fit: the texture is scaled to cover the surface (uniform
        // max scale, centred) and its overflow is cropped by the viewport, so
        // full-screen art reaches every edge at any window aspect instead of leaving
        // the letterbox bars a design-sized widget cannot cover. The texture must be
        // registered with this layer so its pixel size is known. 0 removes it.
        void setBackdrop(TextureId id) { m_backdrop = id; }
        TextureId backdrop() const { return m_backdrop; }

        // The engine's default UI font, borrowed by TextBlocks. Null if unavailable.
        FontAtlas* defaultFont() { return m_fonts.font(FontManager::kDefaultSize); }
        // The default face at an arbitrary pixel size, created on first request.
        FontAtlas* font(float pixelSize) { return m_fonts.font(pixelSize); }

        // Registers a named face chain (registered once; a repeated name returns the
        // existing font). Paths resolve like any asset. Null when no face loads.
        Font* addFont(const std::string& name, std::vector<std::string> faces);
        // A registered font at `pixelSize`. Null when `name` is unknown.
        FontAtlas* font(const std::string& name, float pixelSize);
        // The registered face chain itself, for TextBlocks that resolve their own
        // size per layout scale. Null when `name` is unknown.
        Font* findFont(const std::string& name) const { return m_fonts.find(name); }

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
        glm::vec2 m_designSize{ 0.0f };
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

        // Pixel sizes by texture id, for the backdrop's cover scale. Populated by
        // texture() and registerTexture().
        std::unordered_map<TextureId, glm::vec2> m_textureSizes;
        TextureId m_backdrop = 0;
    };

}
