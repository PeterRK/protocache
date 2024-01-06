# Copyright (c) 2023, Ruan Kunliang.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

cmake_minimum_required(VERSION 3.0)
project(protocache)

set(CMAKE_CXX_STANDARD 11)

find_package(Protobuf REQUIRED)
find_package(gflags REQUIRED)
find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB source
    src/*.cc
)

add_library(protocache ${source})
target_link_libraries(protocache protobuf pthread)

add_library(protocache-lite src/perfect_hash.cc src/hash.cc)

file(GLOB test_src
    test/*.cc
)

add_executable(test ${test_src})
target_link_libraries(test protocache protobuf gtest pthread)

add_executable(json-to-binary tools/json-to-binary.cc)
target_link_libraries(json-to-binary protocache protobuf gflags pthread)

add_executable(protoc-gen-pccx tools/protoc-gen-pccx.cc)
target_link_libraries(protoc-gen-pccx protoc protobuf pthread)

#file(GLOB bench_src
#    test/benchmark/*.cc
#)
#
#add_executable(benchmark ${bench_src})
#target_link_libraries(benchmark protocache protobuf flatbuffers pthread)