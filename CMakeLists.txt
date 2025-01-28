cmake_minimum_required(VERSION 3.28)

project(god_complex)

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W2 /WX")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_subdirectory(zf4)

find_package(glfw3 CONFIG REQUIRED)

add_executable(god_complex
	src/gc.cpp
)

target_include_directories(god_complex PRIVATE
    zf4/zf4/include
    zf4/zf4_common/include
	zf4/vendor/glad/include
)

target_link_libraries(god_complex PRIVATE zf4 zf4_common glfw)

target_compile_definitions(god_complex PRIVATE GLFW_INCLUDE_NONE)

add_custom_target(asset_packing
    COMMAND $<TARGET_FILE:zf4_asset_packer> ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets
    VERBATIM
)

add_dependencies(god_complex asset_packing)
