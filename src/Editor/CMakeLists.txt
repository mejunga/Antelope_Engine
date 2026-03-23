find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE EDITOR_SRC CONFIGURE_DEPENDS "src/*.cpp")

add_executable(AntelopeApp ${EDITOR_SRC})

target_compile_definitions(AntelopeApp PUBLIC ANTELOPE_EDITOR_MODE)

target_link_libraries(AntelopeApp PRIVATE 
    Antelope 
    imgui::imgui
)

set_target_properties(AntelopeApp PROPERTIES OUTPUT_NAME "AntelopeEngine")

add_custom_command(TARGET AntelopeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Assets"
    "$<TARGET_FILE_DIR:AntelopeApp>/Assets"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_BINARY_DIR}/CompiledShaders"
    "$<TARGET_FILE_DIR:AntelopeApp>/Assets/Shaders"

    COMMENT "Copying Assets and Shaders directories to output directory..."
)