add_library(bsv_camera_linux STATIC
    buffer_linux.cpp
)

target_include_directories(bsv_camera_linux
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/camera/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/controller/include
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(bsv_camera_linux
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
