cmake_minimum_required(VERSION 3.21)
project(VostaEngine VERSION 0.2.9 LANGUAGES C CXX)

option(VE_BUILD_SANDBOX "Build SandBox gameplay module" ON)
option(VE_BUILD_EDITOR "Build VostaEditor application" ON)
option(VE_BUILD_PLAYER "Build VostaPlayer runtime host" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLAD 需要 OpenGL
find_package(OpenGL REQUIRED)

# vepch.h 中检查 _WIN32 而非 CMake 默认的 WIN32
if(WIN32)
    add_compile_definitions(_WIN32)
endif()

# 权威世界坐标精度：宏改变 sizeof(TransformComponent)，引擎/编辑器/宿主/游戏模块
# 必须一致，故在根目录统一定义。移除本行即切回 float32 权威坐标的“小世界”模式。
add_compile_definitions(VE_LARGE_WORLD_COORDINATES)

add_subdirectory(VostaEngine)

if(VE_BUILD_SANDBOX)
    add_subdirectory(SandBox)
endif()

if(VE_BUILD_EDITOR)
    add_subdirectory(VostaEditor)
endif()

if(VE_BUILD_PLAYER)
    add_subdirectory(VostaPlayer)
endif()
