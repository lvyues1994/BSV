function(bsv_add_test target source_file)
    add_executable(${target} ${source_file})
    target_link_libraries(${target} PRIVATE bsv)
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/controller/include
        ${PROJECT_SOURCE_DIR}/src/camera/include
        ${PROJECT_SOURCE_DIR}/src/csc/include
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

bsv_add_test(test_egl_context_shader scenario/test_egl_context_shader.cpp)
bsv_add_test(test_gpu_csc_conversion scenario/test_gpu_csc_conversion.cpp)
bsv_add_test(test_cpu_reference_compare scenario/test_cpu_reference_compare.cpp)
bsv_add_test(test_cmake_build scenario/test_cmake_build.cpp)
bsv_add_test(test_linux_gpu_csc_compare scenario/test_linux_gpu_csc_compare.cpp)
bsv_add_test(test_controller_api scenario/test_controller_api.cpp)
bsv_add_test(test_controller_pipeline scenario/test_controller_pipeline.cpp)
bsv_add_test(test_controller_errors scenario/test_controller_errors.cpp)
