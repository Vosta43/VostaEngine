#pragma once

#include "Core/Core.h"

namespace ve {

    class ShaderLibrary;
    class Texture2D;
    class Shader;
    class VertexArray;

    // Renders the startup splash image. Owns the texture/shader/VA so that
    // Core's Application can stay ignorant of the renderer backend.
    class VE_API SplashScreen {
    public:
        explicit SplashScreen(ShaderLibrary& shaderLibrary);
        ~SplashScreen();

        void show();
        void hide();
        void render();
        bool isVisible() const { return m_visible; }

    private:
        void createResources();

        ShaderLibrary& m_shaderLibrary;
        Ref<Texture2D> m_texture;
        Ref<Shader> m_shader;
        Ref<VertexArray> m_vertexArray;
        bool m_visible = false;
    };

}
