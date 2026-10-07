cmake_minimum_required(VERSION 3.21)
project(VostaEngine VERSION 0.2.5 LANGUAGES CXX)

option(VE_BUILD_SANDBOX "Build SandBox demo application" ON)
option(VE_BUILD_EDITOR "Build VostaEditor application" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLAD 需要 OpenGL
find_package(OpenGL REQUIRED)

# vepch.h 中检查 _WIN32 而非 CMake 默认的 WIN32
if(WIN32)
    add_compile_definitions(_WIN32)
endif()

add_subdirectory(VostaEngine)

if(VE_BUILD_SANDBOX)
    add_subdirectory(SandBox)
endif()

if(VE_BUILD_EDITOR)
    add_subdirectory(VostaEditor)
endif()
