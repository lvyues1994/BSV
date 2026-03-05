add_library(bsv_platform_linux STATIC
    buffer_linux.cpp
    gpu_csc_linux.cpp
)

target_include_directories(bsv_platform_linux
    PUBLIC
        ${PROJECT_SOURCE_DIR}/core/include
        ${PROJECT_SOURCE_DIR}/platform/linux/include
        ${PROJECT_SOURCE_DIR}/include
)

find_package(OpenGL REQUIRED)

target_link_libraries(bsv_platform_linux
    PUBLIC
        bsv_core
        EGL
        GLESv2
        OpenGL::GL
)

target_link_libraries(bsv
    INTERFACE
        bsv_platform_linux
)

target_include_directories(bsv
    INTERFACE
        ${PROJECT_SOURCE_DIR}/platform/linux/include
)

target_compile_options(bsv_platform_linux
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
