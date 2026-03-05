add_library(bsv_platform_android STATIC
    camera2_provider_android.cpp
    gpu_csc_android.cpp
)

target_include_directories(bsv_platform_android
    PUBLIC
        ${PROJECT_SOURCE_DIR}/core/include
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(bsv_platform_android
    PUBLIC
        bsv_core
)

target_link_libraries(bsv
    INTERFACE
        bsv_platform_android
)

target_include_directories(bsv
    INTERFACE
        ${PROJECT_SOURCE_DIR}/platform/android/include
)

target_compile_options(bsv_platform_android
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
