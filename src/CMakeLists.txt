add_library(bsv STATIC
    buffer_linux.cpp
    controller.cpp
    gpu_csc_linux.cpp
)

if(ANDROID)
    target_sources(bsv
        PRIVATE
            camera2_provider_android.cpp
            gpu_csc_android.cpp
    )
endif()

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
        Threads::Threads
)

target_compile_options(bsv
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
