cmake_minimum_required(VERSION 3.28)

project(wasteland)

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W2 /WX")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_subdirectory(zf4)

find_package(glfw3 CONFIG REQUIRED)

add_executable(wasteland
	src/game.cpp
)

target_include_directories(wasteland PRIVATE
    zf4/zf4/include
    zf4/zf4_common/include
	zf4/vendor/glad/include
)

target_link_libraries(wasteland PRIVATE zf4 zf4_common glfw)

target_compile_definitions(wasteland PRIVATE GLFW_INCLUDE_NONE)

add_custom_target(asset_packing
    COMMAND $<TARGET_FILE:zf4_asset_packer> ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets
    VERBATIM
)

add_dependencies(wasteland asset_packing)
