cmake_minimum_required(VERSION 3.0)
project(protocache)

set(CMAKE_CXX_STANDARD 11)

find_package(Protobuf REQUIRED)
execute_process(
        COMMAND protoc --cpp_out=. test.proto
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB source
    src/*.cc
)

add_library(protocache ${source})
target_link_libraries(protocache protobuf pthread)

add_library(protocache-lite src/access.cc src/perfect_hash.cc src/hash.cc)

file(GLOB test_src
    test/*.cc
)

add_executable(test ${test_src})
target_link_libraries(test gtest protocache protobuf pthread)