set(ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ── Source files ────────────────────────────────────────────────────────
set(ENGINE_SOURCES
    # Core
    src/Core/Application.cpp
    src/Core/ComponentRegistry.cpp
    src/Core/Compression.cpp
    src/Core/GameModuleHost.cpp
    src/Core/AssetConfig.cpp
    src/Core/AssetLoadQueue.cpp
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
    src/Core/ResourceMemory.cpp
    src/Core/Vfs.cpp
    src/Core/Window.cpp
    src/Core/vepch.cpp

    # Platform / Windowing
    src/Platform/WindowInput.cpp

    # OpenGL backend
    src/Platform/OpenGL/OpenGLBuffer.cpp
    src/Platform/OpenGL/OpenGLContext.cpp
    src/Platform/OpenGL/OpenGLFrameBuffer.cpp
    src/Platform/OpenGL/OpenGLGpuReadback.cpp
    src/Platform/OpenGL/OpenGLIndirectBuffer.cpp
    src/Platform/OpenGL/OpenGLRendererAPI.cpp
    src/Platform/OpenGL/OpenGLShader.cpp
    src/Platform/OpenGL/OpenGLStaticMesh.cpp
    src/Platform/OpenGL/OpenGLStorageBuffer.cpp
    src/Platform/OpenGL/OpenGLTexture.cpp
    src/Platform/OpenGL/OpenGLUniformBuffer.cpp
    src/Platform/OpenGL/OpenGLVertexArray.cpp

    # Renderer
    src/Renderer/Buffer.cpp
    src/Renderer/Camera.cpp
    src/Renderer/CameraController.cpp
    src/Renderer/GraphicsContext.cpp
    src/Renderer/IndirectBuffer.cpp
    src/Renderer/Material.cpp
    src/Renderer/MaterialGraph.cpp
    src/Renderer/MaterialNodes.cpp
    src/Renderer/LayeredMaterial.cpp
    src/Renderer/LayeredShaderBuilder.cpp
    src/Renderer/MaterialLayerAsset.cpp
    src/Renderer/MaterialInstance.cpp
    src/Renderer/SingleMaterial.cpp
    src/Renderer/PipelineState.cpp
    src/Renderer/RenderCommand.cpp
    src/Renderer/Renderer.cpp
    src/Renderer/Renderer2D.cpp
    src/Renderer/Renderer3D.cpp
    src/Renderer/RenderPipeline.cpp
    src/Renderer/RuntimeVirtualTexture.cpp
    src/Renderer/GpuReadback.cpp
    src/Renderer/GpuReadbackSelfTest.cpp
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
    src/Renderer/UniformBuffer.cpp
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
    src/Renderer/RenderPass/RVTBakePass.cpp
    src/Renderer/RenderPass/RVTDebugPass.cpp
    src/Renderer/RenderPass/VolumetricLightPass.cpp
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

    # Geometry (procedural mesh graphs, SDF evaluation, cube-sphere LOD)
    src/Geometry/CubeSphereLod.cpp
    src/Geometry/CubeSphereMeshBuilder.cpp
    src/Geometry/GeometryBaker.cpp
    src/Geometry/GeometryBuffer.cpp
    src/Geometry/GeometryEvaluator.cpp
    src/Geometry/GeometryGraph.cpp
    src/Geometry/GeometryGraphResource.cpp

    # Particle (module-graph emitter runtime)
    src/Particle/AttributeRegistry.cpp
    src/Particle/EmitterAssetIO.cpp
    src/Particle/EmitterCompiler.cpp
    src/Particle/Modules.cpp
    src/Particle/ParticleAsset.cpp
    src/Particle/ParticleBillboard.cpp
    src/Particle/ParticleDataSet.cpp
    src/Particle/ParticleEmitter.cpp
    src/Particle/ParticleSelfTest.cpp
    src/Particle/ParticleSystem.cpp
    src/Particle/ProgramBuilder.cpp
    src/Particle/VM.cpp

    # Physics (Jolt facade)
    src/Physics/PhysicsWorld.cpp

    # Scene / ECS
    src/Scene/Archive.cpp
    src/Scene/BuiltinPrefabs.cpp
    src/Scene/ECSystems/ECSystem.cpp
    src/Scene/ECSystems/ECSystemGraph.cpp
    src/Scene/Entity.cpp
    src/Scene/EntityRegistry.cpp
    src/Scene/ParticleComponent.cpp
    src/Scene/PrefabRegistry.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneClone.cpp
    src/Scene/SceneSerializer.cpp
    src/Scene/Terrain/QuadTreeTerrain.cpp
    src/Scene/Terrain/Terrain.cpp
    src/Scene/Terrain/TerrainDataResource.cpp
    src/Scene/Terrain/TerrainMeshBuilder.cpp

    # Asset import
    src/Asset/AssetAuthoring.cpp
    src/Asset/AssetLibrary.cpp
    src/Asset/BuiltinReousrces.cpp
    src/Asset/ImportManager.cpp
    src/Asset/PakKey.cpp
    src/Asset/PakReader.cpp
    src/Asset/StaticMeshImporter.cpp
    src/Asset/TextureImporter.cpp

    # Audio (miniaudio mixer, streaming decoder, sound assets)
    src/Audio/AudioComponent.cpp
    src/Audio/AudioDecode.cpp
    src/Audio/AudioDevice.cpp
    src/Audio/SoundAsset.cpp

    # Graph (node graphs shared by material and noise)
    src/Graph/NodeGraph.cpp

    # Noise (procedural noise graphs and baking)
    src/Noise/NoiseBaker.cpp
    src/Noise/NoiseCPU.cpp
    src/Noise/NoiseGraph.cpp
    src/Noise/NoiseGraphResource.cpp
    src/Noise/NoisePlan.cpp

    # Terrain rule graphs (CPU-baked material layer weights)
    src/Terrain/RuleGraph.cpp
    src/Terrain/RuleGraphResource.cpp
    src/Terrain/RulePlan.cpp
    src/Terrain/GlobeWeightBaker.cpp

    # GUI
    src/Gui/Gui.cpp
    src/Gui/UIRenderTarget.cpp

    # Vellum (retained-mode UI)
    src/Gui/Vellum/Core/Curve.cpp
    src/Gui/Vellum/Core/Widget.cpp
    src/Gui/Vellum/Text/Font.cpp
    src/Gui/Vellum/Text/FontAtlas.cpp
    src/Gui/Vellum/Text/GlyphAtlas.cpp
    src/Gui/Vellum/Text/FontManager.cpp
    src/Gui/Vellum/Widgets/Button.cpp
    src/Gui/Vellum/Widgets/Checkbox.cpp
    src/Gui/Vellum/Widgets/ColoredBox.cpp
    src/Gui/Vellum/Widgets/Column.cpp
    src/Gui/Vellum/Widgets/Image.cpp
    src/Gui/Vellum/Widgets/InputBox.cpp
    src/Gui/Vellum/Widgets/NineGridImage.cpp
    src/Gui/Vellum/Widgets/ScrollPanel.cpp
    src/Gui/Vellum/Widgets/Slider.cpp
    src/Gui/Vellum/Widgets/Splitter.cpp
    src/Gui/Vellum/Widgets/Stack.cpp
    src/Gui/Vellum/Widgets/TextBlock.cpp
    src/Gui/Vellum/Widgets/TooltipBox.cpp
    src/Gui/Vellum/VellumRenderer.cpp
    src/Gui/Vellum/WidgetTree.cpp
    src/Gui/Vellum/VellumLayer.cpp

    # HUD (authored Vellum trees as assets)
    src/Gui/Hud/HudWidgetRegistry.cpp
    src/Gui/Hud/HudAsset.cpp
    src/Gui/Hud/HudInstance.cpp
    src/Gui/Hud/HudSystem.cpp

    # MCP tool layer
    src/MCP/CommandRegistry.cpp
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
    thirdparty/miniaudio-master/miniaudio.c
    thirdparty/stb_image/stb_image.cpp
    thirdparty/stb_truetype/stb_truetype.cpp
    thirdparty/stb_vorbis/stb_vorbis.c
    thirdparty/ufbx/ufbx.c
    thirdparty/Monocypher/monocypher.c
    # zstd: compression only for the packager (no dictBuilder / legacy)
    thirdparty/zstd-dev/lib/common/debug.c
    thirdparty/zstd-dev/lib/common/entropy_common.c
    thirdparty/zstd-dev/lib/common/error_private.c
    thirdparty/zstd-dev/lib/common/fse_decompress.c
    thirdparty/zstd-dev/lib/common/pool.c
    thirdparty/zstd-dev/lib/common/threading.c
    thirdparty/zstd-dev/lib/common/xxhash.c
    thirdparty/zstd-dev/lib/common/zstd_common.c
    thirdparty/zstd-dev/lib/compress/fse_compress.c
    thirdparty/zstd-dev/lib/compress/hist.c
    thirdparty/zstd-dev/lib/compress/huf_compress.c
    thirdparty/zstd-dev/lib/compress/zstd_compress.c
    thirdparty/zstd-dev/lib/compress/zstd_compress_literals.c
    thirdparty/zstd-dev/lib/compress/zstd_compress_sequences.c
    thirdparty/zstd-dev/lib/compress/zstd_compress_superblock.c
    thirdparty/zstd-dev/lib/compress/zstd_double_fast.c
    thirdparty/zstd-dev/lib/compress/zstd_fast.c
    thirdparty/zstd-dev/lib/compress/zstd_lazy.c
    thirdparty/zstd-dev/lib/compress/zstd_ldm.c
    thirdparty/zstd-dev/lib/compress/zstd_opt.c
    thirdparty/zstd-dev/lib/compress/zstd_preSplit.c
    thirdparty/zstd-dev/lib/compress/zstdmt_compress.c
    thirdparty/zstd-dev/lib/decompress/huf_decompress.c
    thirdparty/zstd-dev/lib/decompress/zstd_ddict.c
    thirdparty/zstd-dev/lib/decompress/zstd_decompress.c
    thirdparty/zstd-dev/lib/decompress/zstd_decompress_block.c
)

# Packaging drives MSBuild on a project's .slnx, so the packager itself is
# Windows-only. Process spawning is per platform behind one header.
if(WIN32)
    list(APPEND ENGINE_SOURCES
        src/Build/Packager.cpp
        src/Build/PakWriter.cpp
        src/Platform/Windows/ProcessWindows.cpp
    )
else()
    list(APPEND ENGINE_SOURCES
        src/Platform/Posix/ProcessPosix.cpp
    )
endif()

# ── CPU sampling profiler (Windows x64 for now) ─────────────────────────
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND ENGINE_SOURCES
        src/Core/Diagnostics/Profiler.cpp
        src/Core/Diagnostics/ProfilerExporter.cpp
        src/Core/Diagnostics/Platform/Windows/ProfilerBackendWin64.cpp
    )
endif()

# ── Engine shared library ───────────────────────────────────────────────
add_library(VostaEngine SHARED ${ENGINE_SOURCES})

# ── Profiler smoke test ─────────────────────────────────────────────────
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(ProfilerSmoke
        tests/ProfilerSmoke/main.cpp
        src/Core/Diagnostics/Profiler.cpp
        src/Core/Diagnostics/ProfilerExporter.cpp
        src/Core/Diagnostics/Platform/Windows/ProfilerBackendWin64.cpp
    )
    target_include_directories(ProfilerSmoke PRIVATE
        ${ENGINE_ROOT}/src
        ${ENGINE_ROOT}/src/Core
    )
    target_compile_definitions(ProfilerSmoke PRIVATE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        VE_PROFILER_STATIC
    )
    target_compile_options(ProfilerSmoke PRIVATE
        /W3 /utf-8 /MP /bigobj /Zc:preprocessor
    )
    target_link_libraries(ProfilerSmoke PRIVATE dbghelp ntdll)
endif()

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
        ${ENGINE_ROOT}/thirdparty/FastNoiseLite
        ${ENGINE_ROOT}/thirdparty/miniaudio-master
        ${ENGINE_ROOT}/thirdparty/stb_vorbis
        ${ENGINE_ROOT}/thirdparty/zstd-dev/lib
        ${ENGINE_ROOT}/thirdparty/Monocypher
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
    target_link_libraries(VostaEngine PRIVATE winhttp dbghelp ntdll)
endif()

# ── libdl — dlopen/dlsym for GameModuleHost (empty on macOS) ─────────────
if(NOT WIN32)
    target_link_libraries(VostaEngine PRIVATE ${CMAKE_DL_LIBS})
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
    thirdparty/Monocypher/monocypher.c
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)

# stb_truetype 实现单独一个 TU，同样跳过预编译头
set_source_files_properties(
    thirdparty/stb_truetype/stb_truetype.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)

# miniaudio / stb_vorbis 是 C 实现，同样跳过预编译头
set_source_files_properties(
    thirdparty/miniaudio-master/miniaudio.c
    thirdparty/stb_vorbis/stb_vorbis.c
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)

# zstd 是 C 实现，跳过预编译头；huf_decompress_amd64.S 未纳入源列表，
# 因此必须关掉汇编路径，否则 x86-64 上会引用未定义的符号。
set_source_files_properties(
    thirdparty/zstd-dev/lib/common/debug.c
    thirdparty/zstd-dev/lib/common/entropy_common.c
    thirdparty/zstd-dev/lib/common/error_private.c
    thirdparty/zstd-dev/lib/common/fse_decompress.c
    thirdparty/zstd-dev/lib/common/pool.c
    thirdparty/zstd-dev/lib/common/threading.c
    thirdparty/zstd-dev/lib/common/xxhash.c
    thirdparty/zstd-dev/lib/common/zstd_common.c
    thirdparty/zstd-dev/lib/compress/fse_compress.c
    thirdparty/zstd-dev/lib/compress/hist.c
    thirdparty/zstd-dev/lib/compress/huf_compress.c
    thirdparty/zstd-dev/lib/compress/zstd_compress.c
    thirdparty/zstd-dev/lib/compress/zstd_compress_literals.c
    thirdparty/zstd-dev/lib/compress/zstd_compress_sequences.c
    thirdparty/zstd-dev/lib/compress/zstd_compress_superblock.c
    thirdparty/zstd-dev/lib/compress/zstd_double_fast.c
    thirdparty/zstd-dev/lib/compress/zstd_fast.c
    thirdparty/zstd-dev/lib/compress/zstd_lazy.c
    thirdparty/zstd-dev/lib/compress/zstd_ldm.c
    thirdparty/zstd-dev/lib/compress/zstd_opt.c
    thirdparty/zstd-dev/lib/compress/zstd_preSplit.c
    thirdparty/zstd-dev/lib/compress/zstdmt_compress.c
    thirdparty/zstd-dev/lib/decompress/huf_decompress.c
    thirdparty/zstd-dev/lib/decompress/zstd_ddict.c
    thirdparty/zstd-dev/lib/decompress/zstd_decompress.c
    thirdparty/zstd-dev/lib/decompress/zstd_decompress_block.c
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
        ZSTD_DISABLE_ASM
)

# ── Warnings ────────────────────────────────────────────────────────────
if(MSVC)
    # /bigobj: the umbrella header and the largest translation units (SceneView,
    # Terrain, SceneViewRenderer) exceed the default 65535-section object limit.
    # /arch:AVX2 must match Jolt's build: the JPH_USE_* macros in its headers
    # select AVX2 code paths that the compiler will not emit otherwise.
    target_compile_options(VostaEngine PRIVATE /W3 /wd4251 /utf-8 /MP /bigobj /arch:AVX2)
else()
    target_compile_options(VostaEngine PRIVATE -Wall -Wextra)
endif()
