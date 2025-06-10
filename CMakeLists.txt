cmake_minimum_required(VERSION 3.31)
project(EM3DLab)

set(CMAKE_CXX_STANDARD 26)

# check if the compiler is gcc
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
        set(CMAKE_CXX_SCAN_FOR_MODULES ON)
    else()
        message(FATAL_ERROR "GCC version must be greater than 14")
    endif()
endif()

add_executable(EM3DLab main.cpp)

add_subdirectory(engine)

target_link_libraries(EM3DLab PUBLIC engine)

find_package(SDL3 CONFIG REQUIRED)
add_custom_command(
        TARGET EM3DLab         # 目标名称
        POST_BUILD           # 执行时机：目标构建完成后
        COMMAND ${CMAKE_COMMAND} -E copy  # 调用CMake内置的文件操作工具
        $<TARGET_FILE:SDL3::SDL3>      # 源文件：SDL3动态库的完整路径
        $<TARGET_FILE_DIR:EM3DLab>        # 目标目录：EM3DLab可执行文件所在目录
)