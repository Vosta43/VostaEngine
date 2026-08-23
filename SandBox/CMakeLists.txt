set(SANDBOX_SOURCES
    EntryPoint.cpp
    src/Application.cpp
    src/SandBoxLayer.cpp
)

add_executable(SandBox ${SANDBOX_SOURCES})

target_include_directories(SandBox PRIVATE
    ${CMAKE_SOURCE_DIR}/VostaEngine
    ${CMAKE_SOURCE_DIR}/VostaEngine/src
    ${CMAKE_SOURCE_DIR}/VostaEngine/thirdparty/IMGUI
    ${CMAKE_SOURCE_DIR}/VostaEngine/thirdparty/glm
)

target_link_libraries(SandBox PRIVATE VostaEngine)

# 工作目录设为 SandBox 目录 (assets/ 相对路径)
if(MSVC)
    target_compile_options(SandBox PRIVATE /utf-8)
endif()

set_target_properties(SandBox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
