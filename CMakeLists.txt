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

add_library(simple_module)
target_sources(simple_module
    PUBLIC
        FILE_SET CXX_MODULES FILES
        test.ixx
)

add_executable(EM3DLab main.cpp)
target_link_libraries(EM3DLab simple_module)