add_subdirectory(controller)
add_subdirectory(camera)
add_subdirectory(csc)

add_library(bsv INTERFACE)

target_include_directories(bsv
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(bsv
    INTERFACE
        bsv_controller
)

if(BSV_PLATFORM STREQUAL "linux")
    target_link_libraries(bsv INTERFACE bsv_camera_linux bsv_csc_linux)
elseif(BSV_PLATFORM STREQUAL "android")
    target_link_libraries(bsv INTERFACE bsv_camera_android bsv_csc_android)
endif()
