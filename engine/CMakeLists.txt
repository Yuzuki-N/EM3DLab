cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 26)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows")
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_library(engine)

target_sources(engine
        PUBLIC
        FILE_SET CXX_MODULES FILES
        window.ixx
)

target_link_libraries(engine PRIVATE Vulkan::Vulkan SDL3::SDL3)

