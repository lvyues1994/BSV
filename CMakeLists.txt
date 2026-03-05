cmake_minimum_required(VERSION 3.16)

project(BSV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BSV_BUILD_TESTS "Build BSV unit tests" ON)

add_subdirectory(src)

if(BSV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
