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


add_subdirectory(engine)


find_package(SDL3 CONFIG REQUIRED)

find_program(GLSL_VALIDATOR glslangValidator
        HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

#add_custom_target(
#        Shaders
#        DEPENDS ${SPIRV_BINARY_FILES}
#)



add_executable(EM3DLab main.cpp ${SPIRV_BINARY_FILES})
target_link_libraries(EM3DLab PUBLIC engine)

add_custom_command(
        TARGET EM3DLab         # 目标名称
        POST_BUILD           # 执行时机：目标构建完成后
        COMMAND ${CMAKE_COMMAND} -E copy  # 调用CMake内置的文件操作工具
        $<TARGET_FILE:SDL3::SDL3>      # 源文件：SDL3动态库的完整路径
        $<TARGET_FILE_DIR:EM3DLab>        # 目标目录：EM3DLab可执行文件所在目录
)

# 查找所有spv文件
file(GLOB_RECURSE SPV_FILES
        "${PROJECT_SOURCE_DIR}/shaders/*.spv"
)

#在运行目录创建shaders文件夹
add_custom_command(
        TARGET EM3DLab
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory  # 先创建目标目录
        "$<TARGET_FILE_DIR:EM3DLab>/Shaders"
        COMMENT "Create Shaders directory"
)

#拷贝spv文件
foreach(SPV ${SPV_FILES})
    add_custom_command(
            TARGET EM3DLab         # 目标名称
            POST_BUILD           # 执行时机：目标构建完成后
            COMMAND ${CMAKE_COMMAND} -E copy  # 调用CMake内置的文件操作工具
            ${SPV}      # 源文件
            $<TARGET_FILE_DIR:EM3DLab>/Shaders        # 目标目录：EM3DLab可执行文件所在目录
    )
endforeach(SPV)

