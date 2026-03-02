find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE EDITOR_SRC CONFIGURE_DEPENDS "src/*.cpp")

add_executable(EditorApp ${EDITOR_SRC})

target_compile_definitions(EditorApp PUBLIC ENGINE_EDITOR_MODE)

target_link_libraries(EditorApp PRIVATE 
    Engine 
    imgui::imgui
)

set_target_properties(EditorApp PROPERTIES OUTPUT_NAME "GameEngine_Editor")