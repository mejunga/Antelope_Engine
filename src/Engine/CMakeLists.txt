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

    add_dependencies(Antelope CompileEngineShaders)

    target_include_directories(Antelope PUBLIC include)
    target_include_directories(Antelope PRIVATE src)
    target_include_directories(Antelope PRIVATE ${Stb_INCLUDE_DIR})
    target_include_directories(Antelope PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

    target_compile_definitions(Antelope PUBLIC GLFW_INCLUDE_VULKAN ANTELOPE_EDITOR_MODE)

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