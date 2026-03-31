cmake_minimum_required(VERSION 3.20)

project(AntelopeEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_program(VULKAN_GLSLC_EXECUTABLE NAMES glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT VULKAN_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found! Please check your Vulkan SDK installation.")
endif()

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders")
set(SHADER_BIN_DIR "${CMAKE_BINARY_DIR}/CompiledShaders")
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

set(SHADERS
    "${SHADER_SRC_DIR}/base.vert"
    "${SHADER_SRC_DIR}/base.frag"
    "${SHADER_SRC_DIR}/editor_picking.vert"
    "${SHADER_SRC_DIR}/editor_picking.frag"
)

set(COMPILED_SHADERS "")
foreach(SHADER IN LISTS SHADERS)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_FILE "${SHADER_BIN_DIR}/${SHADER_NAME}.spv")
    
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${VULKAN_GLSLC_EXECUTABLE} ${SHADER} -o ${SPV_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
    )
    list(APPEND COMPILED_SHADERS ${SPV_FILE})
endforeach()

add_custom_target(CompileEngineShaders ALL DEPENDS ${COMPILED_SHADERS})

add_subdirectory(src/Engine)
add_subdirectory(src/Editor)