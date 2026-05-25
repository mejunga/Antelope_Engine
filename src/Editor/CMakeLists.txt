find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)

file(GLOB_RECURSE EDITOR_SRC CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE EDITOR_HDR CONFIGURE_DEPENDS "include/*.hpp")

add_executable(AntelopeApp ${EDITOR_SRC} ${EDITOR_HDR})

set_target_properties(AntelopeApp PROPERTIES WIN32_EXECUTABLE "$<CONFIG:Release>")
if(MSVC)
    target_link_options(AntelopeApp PRIVATE "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>")
endif()

if(WIN32)
    target_sources(AntelopeApp PRIVATE "${CMAKE_SOURCE_DIR}/Assets/resources.rc")
endif()

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

add_custom_command(TARGET AntelopeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/Engine/include"
        "$<TARGET_FILE_DIR:AntelopeApp>/Engine/include"
    COMMENT "Copying engine headers for script compilation"
)

add_custom_command(TARGET AntelopeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_LINKER_FILE:Antelope>"
        "$<TARGET_FILE_DIR:AntelopeApp>/Engine/lib/$<TARGET_LINKER_FILE_NAME:Antelope>"
    COMMENT "Copying Antelope import lib for script compilation"
)

add_custom_command(TARGET AntelopeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Antelope>"
        "$<TARGET_FILE_DIR:AntelopeApp>/$<TARGET_FILE_NAME:Antelope>"
    COMMENT "Copying Antelope.dll to output"
)

if(DEFINED _VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_vcpkg_inc "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    if(EXISTS "${_vcpkg_inc}")
        add_custom_command(TARGET AntelopeApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_vcpkg_inc}"
                "$<TARGET_FILE_DIR:AntelopeApp>/Engine/include"
            COMMENT "Copying vcpkg headers for script compilation"
        )
    endif()
endif()