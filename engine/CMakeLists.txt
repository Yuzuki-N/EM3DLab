cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 26)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows")
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)

add_library(engine)

target_sources(engine
        PUBLIC
        FILE_SET CXX_MODULES FILES
        window.ixx
        vk_engine.ixx
)

target_link_libraries(engine PUBLIC Vulkan::Vulkan SDL3::SDL3)
target_link_libraries(engine PUBLIC vk-bootstrap::vk-bootstrap vk-bootstrap::vk-bootstrap-compiler-warnings)

