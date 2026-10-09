# Jolt Physics — vendored third-party library (see THIRD_PARTY_NOTICES.md)

file(GLOB_RECURSE JOLT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Jolt/*.cpp")

add_library(Jolt STATIC ${JOLT_SOURCES})
add_library(Jolt::Jolt ALIAS Jolt)

set_target_properties(Jolt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# Include root so consumers can `#include <Jolt/Jolt.h>`
target_include_directories(Jolt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# SIMD/feature macros gate Jolt's header code paths — keep in sync with /arch:AVX2.
target_compile_definitions(Jolt PRIVATE
    _LIB
    JPH_OBJECT_STREAM
    JPH_USE_SSE4_1 JPH_USE_SSE4_2 JPH_USE_AVX JPH_USE_AVX2
    JPH_USE_LZCNT JPH_USE_TZCNT JPH_USE_F16C JPH_USE_FMADD
)

if(MSVC)
    target_compile_options(Jolt PRIVATE /arch:AVX2 /bigobj /utf-8 /MP)
endif()
