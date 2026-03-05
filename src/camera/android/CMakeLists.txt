add_library(bsv_camera_android STATIC
    camera2_provider_android.cpp
)

target_include_directories(bsv_camera_android
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/camera/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/controller/include
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(bsv_camera_android
    PRIVATE
        __ANDROID__
)

target_compile_options(bsv_camera_android
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
