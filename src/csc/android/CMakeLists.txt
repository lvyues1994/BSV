add_library(bsv_csc_android STATIC
    gpu_csc_android.cpp
)

target_include_directories(bsv_csc_android
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/csc/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/controller/include
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(bsv_csc_android
    PRIVATE
        __ANDROID__
)

target_compile_options(bsv_csc_android
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
