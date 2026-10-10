set(SANDBOX_SOURCES
    src/SandBoxModule.cpp
)

# SandBox is the built-in project's gameplay module: a shared library that a
# host (editor Play / VostaPlayer) loads from <projectRoot>/Binaries/game.dll.
add_library(SandBox SHARED ${SANDBOX_SOURCES})

target_include_directories(SandBox PRIVATE
    ${CMAKE_SOURCE_DIR}/VostaEngine
    ${CMAKE_SOURCE_DIR}/VostaEngine/src
    ${CMAKE_SOURCE_DIR}/VostaEngine/thirdparty/IMGUI
    ${CMAKE_SOURCE_DIR}/VostaEngine/thirdparty/glm
)

target_link_libraries(SandBox PRIVATE VostaEngine)

if(MSVC)
    target_compile_options(SandBox PRIVATE /utf-8)
endif()

# Output name and location match the host's module convention exactly.
set_target_properties(SandBox PROPERTIES
    OUTPUT_NAME game
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries"
)
