#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"

#include <glm.hpp>
#include <unordered_map>

namespace ve {

    class Shader;
    class VertexArray;
    class VertexBuffer;
    class Texture2D;

}

namespace ve::vellum {

    // Turns a DrawList into engine RHI draw calls. Deliberately concrete: Vellum
    // is an engine module and drives the engine's render primitives directly, the
    // same way the sprite renderer does. There is no backend interface.
    class VE_API VellumRenderer {
    public:
        // Requires a live GL context. Safe to call more than once.
        void init();

        // Replays `list` into the currently bound render target. `surfaceSize` is
        // the pixel size the list was painted against; clip rects live in that space.
        void render(const DrawList& list, const glm::vec2& surfaceSize);

        // Makes `id` resolvable by render(). Id 0 is the built-in 1x1 white texture
        // and is always registered.
        void registerTexture(TextureId id, const Ref<Texture2D>& texture);

        // Reserves a fresh id for a texture that does not exist yet. The sole
        // allocator of UI texture ids, so a font atlas and a widget image can never
        // collide; the caller registers the texture once it has one.
        TextureId acquireTextureId();

        // Reserves an id and registers `texture` under it in one step.
        TextureId addTexture(const Ref<Texture2D>& texture);

    private:
        // One quad per four vertices / six indices is the DrawList contract, so the
        // index buffer is a fixed pattern and only the vertices stream per frame.
        static constexpr uint32_t kMaxQuads = 16384;

        Ref<Shader>       m_shader;
        Ref<VertexArray>  m_vertexArray;
        Ref<VertexBuffer> m_vertexBuffer;
        Ref<Texture2D>    m_whiteTexture;

        std::unordered_map<TextureId, Ref<Texture2D>> m_textures;
        TextureId m_nextTextureId = 1;   // 0 is the white pixel
        bool m_initialized = false;
    };

}
