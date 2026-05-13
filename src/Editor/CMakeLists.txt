find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)

file(GLOB_RECURSE EDITOR_SRC CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE EDITOR_HDR CONFIGURE_DEPENDS "include/*.hpp")

add_executable(AntelopeApp ${EDITOR_SRC} ${EDITOR_HDR})

target_include_directories(AntelopeApp PUBLIC include)
target_include_directories(AntelopeApp PRIVATE src)

target_link_libraries(AntelopeApp PRIVATE 
    Antelope 
    imgui::imgui
    imguizmo::imguizmo
    unofficial::imgui-node-editor::imgui-node-editor
)

set_target_properties(AntelopeApp PROPERTIES OUTPUT_NAME "AntelopeEngine")

add_custom_command(TARGET AntelopeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Assets"
    "$<TARGET_FILE_DIR:AntelopeApp>/Assets"

    COMMAND ${CMAKE_COMMAND} -E rm -rf 
    "$<TARGET_FILE_DIR:AntelopeApp>/Assets/Shaders"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_BINARY_DIR}/CompiledShaders"
    "$<TARGET_FILE_DIR:AntelopeApp>/Assets/Shaders"

    COMMENT "Copying Assets and filtering out raw Shaders..."
)