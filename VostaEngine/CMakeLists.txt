set(ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ── Source files ────────────────────────────────────────────────────────
set(ENGINE_SOURCES
    # Core
    src/Core/Application.cpp
    src/Core/AssetConfig.cpp
    src/Core/Deltatime.cpp
    src/Core/Layers/Layers.cpp
    src/Core/Layers/LayerStack.cpp
    src/Core/Log.cpp
    src/Core/Reflection.cpp
    src/Core/ResourceManager.cpp
    src/Core/Window.cpp
    src/Core/vepch.cpp

    # Platform / Windowing
    src/Platform/WindowInput.cpp

    # OpenGL backend
    src/Platform/OpenGL/OpenGLBuffer.cpp
    src/Platform/OpenGL/OpenGLContext.cpp
    src/Platform/OpenGL/OpenGLFrameBuffer.cpp
    src/Platform/OpenGL/OpenGLRendererAPI.cpp
    src/Platform/OpenGL/OpenGLShader.cpp
    src/Platform/OpenGL/OpenGLStaticMesh.cpp
    src/Platform/OpenGL/OpenGLStorageBuffer.cpp
    src/Platform/OpenGL/OpenGLTexture.cpp
    src/Platform/OpenGL/OpenGLVertexArray.cpp

    # Renderer
    src/Renderer/Buffer.cpp
    src/Renderer/Camera.cpp
    src/Renderer/CameraController.cpp
    src/Renderer/GraphicsContext.cpp
    src/Renderer/Material.cpp
    src/Renderer/MaterialGraph.cpp
    src/Renderer/MaterialNodes.cpp
    src/Renderer/RenderCommand.cpp
    src/Renderer/Renderer.cpp
    src/Renderer/Renderer2D.cpp
    src/Renderer/Renderer3D.cpp
    src/Renderer/RenderPipeline.cpp
    src/Renderer/SceneRenderer.cpp
    src/Renderer/Shader.cpp
    src/Renderer/ShaderGenerator.cpp
    src/Renderer/StaticMesh.cpp
    src/Renderer/StorageBuffer.cpp
    src/Renderer/SubTexture2D.cpp
    src/Renderer/Texture.cpp
    src/Renderer/ThumbnailRenderer.cpp
    src/Renderer/VertexArray.cpp
    src/Renderer/RenderPass/RenderPassBase.cpp
    src/Renderer/RenderPass/GBufferPass.cpp
    src/Renderer/RenderPass/HDRBufferPass.cpp
    src/Renderer/RenderPass/CloudPass.cpp
    src/Renderer/RenderPass/TAAPass.cpp
    src/Renderer/RenderPass/PostBufferPass.cpp
    src/Renderer/Preprocess/IBLBaker.cpp
    src/Renderer/Preprocess/WorleyNoiseBaker.cpp
    src/Renderer/Preprocess/AtmosphereBaker.cpp
    src/Renderer/Preprocess/WeatherMapBaker.cpp

    # Scene / ECS
    src/Scene/Archive.cpp
    src/Scene/ECSystem.cpp
    src/Scene/Entity.cpp
    src/Scene/EntityRegistry.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneSerializer.cpp

    # Asset import
    src/Asset/ImportManager.cpp
    src/Asset/StaticMeshImporter.cpp
    src/Asset/TextureImporter.cpp

    # GUI
    src/Gui/Gui.cpp

    # Third-party — compiled directly into engine
    thirdparty/GLAD/src/glad.c
    thirdparty/IMGUI/imgui.cpp
    thirdparty/IMGUI/imgui_draw.cpp
    thirdparty/IMGUI/imgui_tables.cpp
    thirdparty/IMGUI/imgui_widgets.cpp
    thirdparty/IMGUI/misc/cpp/imgui_stdlib.cpp
    thirdparty/IMGUI/backends/imgui_impl_glfw.cpp
    thirdparty/IMGUI/backends/imgui_impl_opengl3.cpp
    thirdparty/stb_image/stb_image.cpp
)

# ── Engine shared library ───────────────────────────────────────────────
add_library(VostaEngine STATIC ${ENGINE_SOURCES})

# ── Include directories ─────────────────────────────────────────────────
target_include_directories(VostaEngine
    PUBLIC
        ${ENGINE_ROOT}              # VostaEngine.h
        ${ENGINE_ROOT}/src          # 引擎源码
    PRIVATE
        ${ENGINE_ROOT}/src/Core     # vepch.h (预编译头)
        ${ENGINE_ROOT}/thirdparty/GLAD/include
        ${ENGINE_ROOT}/thirdparty/IMGUI
        ${ENGINE_ROOT}/thirdparty/IMGUI/backends
        ${ENGINE_ROOT}/thirdparty/glm
        ${ENGINE_ROOT}/thirdparty/stb_image
)

# ── GLFW (pre-built binary, vendored) ───────────────────────────────────
if(WIN32)
    set(GFLW_LIB_DIR ${ENGINE_ROOT}/thirdparty/GLFW/lib)
    add_library(glfw3 STATIC IMPORTED)
    set_target_properties(glfw3 PROPERTIES
        IMPORTED_LOCATION          "${GFLW_LIB_DIR}/glfw3_mt.lib"
        IMPORTED_LOCATION_DEBUG    "${GFLW_LIB_DIR}/glfw3_mt.lib"
        IMPORTED_LOCATION_RELEASE  "${GFLW_LIB_DIR}/glfw3_mt.lib"
    )
    target_include_directories(glfw3 INTERFACE
        ${ENGINE_ROOT}/thirdparty/GLFW/include
    )
else()
    # Linux / macOS: 使用系统包管理器安装的 glfw
    find_package(glfw3 REQUIRED)
endif()

# ── Dependencies ────────────────────────────────────────────────────────
target_link_libraries(VostaEngine
    PUBLIC
        OpenGL::GL
    PRIVATE
        glfw3
)

# ── Precompiled header ──────────────────────────────────────────────────
target_precompile_headers(VostaEngine PRIVATE "src/Core/vepch.h")

# glad.c 编译为 C++ 并跳过预编译头（vepch.h 是 C++ 头文件）
set_source_files_properties(
    thirdparty/GLAD/src/glad.c
    PROPERTIES
        LANGUAGE CXX
        SKIP_PRECOMPILE_HEADERS ON
)

# ── Compile definitions ─────────────────────────────────────────────────
target_compile_definitions(VostaEngine
    PUBLIC
        IMGUI_IMPL_OPENGL_LOADER_GLAD
    PRIVATE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

# ── Warnings ────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(VostaEngine PRIVATE /W3 /wd4251 /utf-8)
else()
    target_compile_options(VostaEngine PRIVATE -Wall -Wextra)
endif()
