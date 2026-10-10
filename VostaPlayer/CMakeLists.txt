set(VOSTAPLAYER_SOURCES
    src/VostaPlayer.cpp
)

# Thin standalone runtime host: opens a project, renders its default scene and
# drives its gameplay module (same game.dll the editor's Play loads).
add_executable(VostaPlayer ${VOSTAPLAYER_SOURCES})

target_include_directories(VostaPlayer PRIVATE
    ${CMAKE_SOURCE_DIR}/VostaEngine
    ${CMAKE_SOURCE_DIR}/VostaEngine/src
    ${CMAKE_SOURCE_DIR}/VostaEngine/thirdparty/IMGUI
    ${CMAKE_SOURCE_DIR}/VostaEngine/thirdparty/glm
)

target_link_libraries(VostaPlayer PRIVATE VostaEngine)

if(MSVC)
    target_compile_options(VostaPlayer PRIVATE /utf-8)
endif()

set_target_properties(VostaPlayer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VS_DEBUGGER_ENVIRONMENT "VOSTA_ENGINE_ROOT=${CMAKE_SOURCE_DIR}"
)
