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
find_package(enkiTS CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")

include(FetchContent)
FetchContent_Declare(
    rpmalloc
    GIT_REPOSITORY https://github.com/mjansson/rpmalloc.git
    GIT_TAG 1.4.5
)
FetchContent_MakeAvailable(rpmalloc)

if(NOT TARGET rpmalloc)
    add_library(rpmalloc STATIC ${rpmalloc_SOURCE_DIR}/rpmalloc/rpmalloc.c)
    target_include_directories(rpmalloc PUBLIC ${rpmalloc_SOURCE_DIR})
endif()

file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE ENGINE_HDR CONFIGURE_DEPENDS "include/*.hpp")

add_library(Antelope SHARED ${ENGINE_SRC} ${ENGINE_HDR})
set_target_properties(Antelope PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

add_dependencies(Antelope CompileEngineShaders)

target_include_directories(Antelope PUBLIC include)
target_include_directories(Antelope PRIVATE src)
target_include_directories(Antelope PRIVATE ${Stb_INCLUDE_DIR})
target_include_directories(Antelope PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

target_compile_definitions(Antelope PUBLIC GLFW_INCLUDE_VULKAN ANTELOPE_EDITOR_MODE)
target_compile_definitions(Antelope PRIVATE ANTELOPE_BUILD_DLL)

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
    enkiTS::enkiTS
    rpmalloc
)

target_link_libraries(Antelope PRIVATE
    imgui::imgui
    imguizmo::imguizmo
)

get_target_property(ENGINE_INCLUDE_DIRS Antelope INCLUDE_DIRECTORIES)
get_target_property(ENGINE_LINK_LIBS Antelope LINK_LIBRARIES)

install(DIRECTORY include/ DESTINATION Engine/include)
install(FILES $<TARGET_FILE:Antelope> DESTINATION Engine/lib)