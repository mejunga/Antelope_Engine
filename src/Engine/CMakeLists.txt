find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)

find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")

file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE ENGINE_HDR CONFIGURE_DEPENDS "include/*.hpp")

add_library(Antelope STATIC ${ENGINE_SRC} ${ENGINE_HDR})

find_program(VULKAN_GLSLC_EXECUTABLE NAMES glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT VULKAN_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found! Please check your Vulkan SDK installation.")
endif()

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Renderer/Shaders")
set(SHADER_BIN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders")
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

set(SHADERS
    "${SHADER_SRC_DIR}/base.vert"
    "${SHADER_SRC_DIR}/base.frag"
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
add_dependencies(Antelope CompileEngineShaders)

target_include_directories(Antelope PUBLIC include)
target_include_directories(Antelope PRIVATE src)
target_include_directories(Antelope PRIVATE ${Stb_INCLUDE_DIR})
target_include_directories(Antelope PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

target_compile_definitions(Antelope PUBLIC GLFW_INCLUDE_VULKAN)

target_link_libraries(Antelope PUBLIC 
    Vulkan::Vulkan
    glfw
    glm::glm
    spdlog::spdlog
    EnTT::EnTT
    assimp::assimp
    GPUOpen::VulkanMemoryAllocator
    Jolt::Jolt 
    yaml-cpp::yaml-cpp
    unofficial::shaderc::shaderc
)