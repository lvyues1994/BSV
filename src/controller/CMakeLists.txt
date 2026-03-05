add_library(bsv_controller STATIC
    controller.cpp
)

target_include_directories(bsv_controller
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/controller/include
)

target_link_libraries(bsv_controller
    PUBLIC
        Threads::Threads
)

target_compile_options(bsv_controller
    PRIVATE
        -Wall
        -Wextra
        -Werror
)
