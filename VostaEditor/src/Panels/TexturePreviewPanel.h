#pragma once
#include <VostaEngine.h>

#include <string>

namespace ve {

// Standalone texture viewer. Shows one texture with per-channel toggles (R/G/B/A,
// UE-style): while any of R/G/B is on the texture draws as colour with the
// disabled channels zeroed; A alone draws the alpha channel as white (so an
// opaque texture reads as solid white). This needs its own offscreen pass rather
// than ImGui::Image, which can only tint a texture — it cannot mask single
// channels, and cannot show alpha on its own.
class TexturePreviewPanel {
public:
    // Binds the panel to a texture and pops the window. The Ref keeps the texture
    // alive for as long as the viewer holds it.
    void open(Ref<Texture2D> texture, const std::string& title);

    void onGuiRender(bool* openFlag);

private:
    // Lazy GL setup: the shader, the fullscreen quad, and the size the offscreen
    // target currently has. Only valid once a GL context exists (editor runtime).
    void ensureResources();
    void renderPreview(uint32_t width, uint32_t height);

    Ref<Texture2D> m_texture;
    std::string    m_title;

    bool m_showR = true;
    bool m_showG = true;
    bool m_showB = true;
    bool m_showA = false;

    Ref<Shader>      m_shader;
    Ref<VertexArray> m_quad;
    Ref<Framebuffer> m_target;
};

} // namespace ve
