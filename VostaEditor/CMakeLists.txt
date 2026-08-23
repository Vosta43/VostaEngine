set(VOSTA_ROOT ${CMAKE_SOURCE_DIR}/VostaEngine)

set(EDITOR_SOURCES
    src/VostaEditor.cpp
    src/EditorLayer.cpp
    src/EditorViewController.cpp
    src/Panels/FileBrowser.cpp
    src/Panels/SceneOutlinerPanel.cpp
    src/Panels/PropertyPanel.cpp
    src/Panels/MaterialGraphPanel.cpp

    # Vendored editor-only third-party (compiled into editor, not engine)
    ${VOSTA_ROOT}/thirdparty/IMGUIZMO/ImGuizmo.cpp
    ${VOSTA_ROOT}/thirdparty/imgui-node-editor/crude_json.cpp
    ${VOSTA_ROOT}/thirdparty/imgui-node-editor/imgui_canvas.cpp
    ${VOSTA_ROOT}/thirdparty/imgui-node-editor/imgui_node_editor.cpp
    ${VOSTA_ROOT}/thirdparty/imgui-node-editor/imgui_node_editor_api.cpp
)

add_executable(VostaEditor ${EDITOR_SOURCES})

target_include_directories(VostaEditor PRIVATE
    ${VOSTA_ROOT}
    ${VOSTA_ROOT}/src
    ${VOSTA_ROOT}/thirdparty/IMGUI
    ${VOSTA_ROOT}/thirdparty/IMGUIZMO
    ${VOSTA_ROOT}/thirdparty/imgui-node-editor
    ${VOSTA_ROOT}/thirdparty/glm
)

target_link_libraries(VostaEditor PRIVATE VostaEngine)

if(MSVC)
    target_compile_options(VostaEditor PRIVATE /utf-8)
endif()

set_target_properties(VostaEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
