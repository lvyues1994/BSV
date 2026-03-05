add_library(bsv STATIC
    buffer_linux.cpp
    gpu_csc_linux.cpp
)

target_include_directories(bsv
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

find_package(OpenGL REQUIRED)

target_link_libraries(bsv
    PUBLIC
        EGL
        GLESv2
        OpenGL::GL
)

target_compile_options(bsv
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
