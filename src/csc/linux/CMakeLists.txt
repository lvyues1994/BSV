add_library(bsv_csc_linux STATIC
    gpu_csc_linux.cpp
)

target_include_directories(bsv_csc_linux
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/csc/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/controller/include
        ${PROJECT_SOURCE_DIR}/include
)

find_package(OpenGL REQUIRED)

target_link_libraries(bsv_csc_linux
    PUBLIC
        EGL
        GLESv2
        OpenGL::GL
)

target_compile_options(bsv_csc_linux
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
