add_library(bsv_core STATIC
    src/controller.cpp
)

target_include_directories(bsv_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/core/include
)

add_library(bsv INTERFACE)

target_include_directories(bsv
    INTERFACE
        ${PROJECT_SOURCE_DIR}/core/include
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(bsv
    INTERFACE
        bsv_core
)

target_link_libraries(bsv_core
    PUBLIC
        Threads::Threads
)

target_compile_options(bsv_core
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
