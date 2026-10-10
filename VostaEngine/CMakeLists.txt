set(ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ── Source files ────────────────────────────────────────────────────────
set(ENGINE_SOURCES
    # Core
    src/Core/Application.cpp
    src/Core/ComponentRegistry.cpp
    src/Core/GameModuleHost.cpp
    src/Core/AssetConfig.cpp
    src/Core/EnginePaths.cpp
    src/Core/Deltatime.cpp
    src/Core/JobSystem.cpp
    src/Core/Json.cpp
    src/Core/Layers/Layers.cpp
    src/Core/Layers/LayerStack.cpp
    src/Core/Log.cpp
    src/Core/Project.cpp
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
    src/Renderer/LayeredMaterial.cpp
    src/Renderer/MaterialLayerAsset.cpp
    src/Renderer/MaterialInstance.cpp
    src/Renderer/SingleMaterial.cpp
    src/Renderer/RenderCommand.cpp
    src/Renderer/Renderer.cpp
    src/Renderer/Renderer2D.cpp
    src/Renderer/Renderer3D.cpp
    src/Renderer/RenderPipeline.cpp
    src/Renderer/SceneRenderer.cpp
    src/Renderer/SceneViewRenderer.cpp
    src/Renderer/SceneView.cpp
    src/Renderer/Shader.cpp
    src/Renderer/ShaderGenerator.cpp
    src/Renderer/SplashScreen.cpp
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
    src/Renderer/RenderPass/CloudTAAPass.cpp
    src/Renderer/RenderPass/TAAPass.cpp
    src/Renderer/RenderPass/PostBufferPass.cpp
    src/Renderer/RenderPass/PassBinding.cpp
    src/Renderer/RenderPass/PresentPass.cpp
    src/Renderer/RenderPass/ShadowPass.cpp
    src/Renderer/Shadow/ShadowCascadeCalculator.cpp
    src/Renderer/Pipeline/PipelineConfig.cpp
    src/Renderer/Pipeline/SettingsRegistry.cpp
    src/Renderer/Preprocess/BakeService.cpp
    src/Renderer/Preprocess/IBLBaker.cpp
    src/Renderer/Preprocess/WorleyNoiseBaker.cpp
    src/Renderer/Preprocess/AtmosphereBaker.cpp
    src/Renderer/Preprocess/AtmosphereSkyBaker.cpp
    src/Renderer/Preprocess/WeatherMapBaker.cpp

    # Scene / ECS
    src/Scene/Archive.cpp
    src/Scene/BuiltinPrefabs.cpp
    src/Scene/ECSystems/ECSystem.cpp
    src/Scene/ECSystems/ECSystemGraph.cpp
    src/Scene/Entity.cpp
    src/Scene/EntityRegistry.cpp
    src/Scene/PrefabRegistry.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneClone.cpp
    src/Scene/SceneSerializer.cpp

    # Asset import
    src/Asset/AssetAuthoring.cpp
    src/Asset/AssetLibrary.cpp
    src/Asset/ImportManager.cpp
    src/Asset/StaticMeshImporter.cpp
    src/Asset/TextureImporter.cpp

    # GUI
    src/Gui/Gui.cpp

    # Vellum (retained-mode UI)
    src/Gui/Vellum/Core/Widget.cpp
    src/Gui/Vellum/Text/FontAtlas.cpp
    src/Gui/Vellum/Text/FontManager.cpp
    src/Gui/Vellum/Widgets/Button.cpp
    src/Gui/Vellum/Widgets/ColoredBox.cpp
    src/Gui/Vellum/Widgets/Stack.cpp
    src/Gui/Vellum/Widgets/TextBlock.cpp
    src/Gui/Vellum/VellumRenderer.cpp
    src/Gui/Vellum/WidgetTree.cpp
    src/Gui/Vellum/VellumLayer.cpp

    # MCP tool layer
    src/MCP/ToolRegistry.cpp
    src/MCP/ToolDispatchQueue.cpp
    src/MCP/SceneTools.cpp
    src/MCP/AssetTools.cpp
    src/MCP/EditorTools.cpp
    src/MCP/HttpClient.cpp
    src/MCP/LlmClient.cpp
    src/MCP/AgentSession.cpp

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
    thirdparty/stb_truetype/stb_truetype.cpp
    thirdparty/ufbx/ufbx.c
)

# ── Engine shared library ───────────────────────────────────────────────
add_library(VostaEngine SHARED ${ENGINE_SOURCES})

# ── Jolt Physics (vendored, see THIRD_PARTY_NOTICES.md) ─────────────────
add_subdirectory(thirdparty/Jolt)

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
        ${ENGINE_ROOT}/thirdparty/stb_truetype
        ${ENGINE_ROOT}/thirdparty/ufbx
        ${ENGINE_ROOT}/thirdparty/JSON/include
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
        Jolt
)

# ── WinHTTP — transport for the AI client (system library, ties to no source) ─
if(WIN32)
    target_link_libraries(VostaEngine PRIVATE winhttp)
endif()

# ── Precompiled header ──────────────────────────────────────────────────
target_precompile_headers(VostaEngine PRIVATE "src/Core/vepch.h")

# glad.c 编译为 C++ 并跳过预编译头（vepch.h 是 C++ 头文件）
set_source_files_properties(
    thirdparty/GLAD/src/glad.c
    PROPERTIES
        LANGUAGE CXX
        SKIP_PRECOMPILE_HEADERS ON
)

# ufbx.c 保持 C 编译，同样跳过预编译头
set_source_files_properties(
    thirdparty/ufbx/ufbx.c
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)

# stb_truetype 实现单独一个 TU，同样跳过预编译头
set_source_files_properties(
    thirdparty/stb_truetype/stb_truetype.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)

# ── Compile definitions ─────────────────────────────────────────────────
target_compile_definitions(VostaEngine
    PUBLIC
        IMGUI_IMPL_OPENGL_LOADER_GLAD
    PRIVATE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        VE_BUILD_DLL
)

# ── Warnings ────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(VostaEngine PRIVATE /W3 /wd4251 /utf-8)
else()
    target_compile_options(VostaEngine PRIVATE -Wall -Wextra)
endif()
