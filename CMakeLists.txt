# Copyright (c) 2023, Ruan Kunliang.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

cmake_minimum_required(VERSION 3.0)
project(protocache)

set(CMAKE_CXX_STANDARD 11)

option(WITH_TEST "enable test" ON)

find_package(Protobuf REQUIRED)
find_package(gflags REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB source
    src/*.cc
)

add_library(protocache-lite ${source})

file(GLOB extension
    src/extension/*.cc
)

add_library(protocache ${source} ${extension})
target_link_libraries(protocache protobuf pthread)

if(WITH_TEST)
    find_package(GTest REQUIRED)

    file(GLOB test_src
        test/*.cc
    )

    add_executable(test ${test_src})
    target_link_libraries(test protocache protobuf gtest pthread)
endif()

add_executable(json-to-binary tools/json-to-binary.cc)
target_link_libraries(json-to-binary protocache protobuf gflags pthread)

add_executable(binary-to-json tools/binary-to-json.cc)
target_link_libraries(binary-to-json protocache protobuf gflags pthread)

add_executable(protoc-gen-pccx tools/protoc-gen-pccx.cc)
target_link_libraries(protoc-gen-pccx protoc protobuf pthread)

add_executable(protoc-gen-pcjv tools/protoc-gen-pcjv.cc)
target_link_libraries(protoc-gen-pcjv protoc protobuf pthread)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/protocache"
    DESTINATION include
)

install(
    TARGETS protocache protocache-lite protoc-gen-pccx protoc-gen-pcjv
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

add_executable(random-small tools/random-small.cc)
target_link_libraries(random-small protocache-lite)

#file(GLOB bench_src
#    test/benchmark/*.cc
#)
#
#add_executable(benchmark ${bench_src})
#target_link_libraries(benchmark protocache protobuf flatbuffers pthread)