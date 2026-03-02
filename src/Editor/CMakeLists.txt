find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE EDITOR_SRC CONFIGURE_DEPENDS "src/*.cpp")

add_executable(AntelopeApp ${EDITOR_SRC})

target_compile_definitions(AntelopeApp PUBLIC ANTELOPE_EDITOR_MODE)

target_link_libraries(AntelopeApp PRIVATE 
    Antelope 
    imgui::imgui
)

set_target_properties(AntelopeApp PROPERTIES OUTPUT_NAME "AntelopeEngine")