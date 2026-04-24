# Copyright (c) 2023, Ruan Kunliang.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

cmake_minimum_required(VERSION 3.10)
project(protocache)

set(CMAKE_CXX_STANDARD 17)

option(WITH_TEST "enable test" ON)
option(WITH_TOOLS "build command line tools and protoc plugins" ON)
option(WITH_BENCHMARK "build benchmark target" OFF)
option(PROTOCACHE_ENABLE_NATIVE_OPT "enable native CPU optimizations" OFF)

find_package(Protobuf REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(PROTOCACHE_USE_NATIVE_OPT ${PROTOCACHE_ENABLE_NATIVE_OPT})
if(WITH_BENCHMARK)
    set(PROTOCACHE_USE_NATIVE_OPT ON)
endif()

if(PROTOCACHE_USE_NATIVE_OPT)
    add_compile_options(-march=native)
endif()

set(PROTOCACHE_CORE_SOURCES
    src/access.cc
    src/hash.cc
    src/perfect_hash.cc
    src/serialize.cc
    src/utils.cc
)

add_library(protocache-lite ${PROTOCACHE_CORE_SOURCES})
set_target_properties(protocache-lite PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(PROTOCACHE_EXTENSION_SOURCES
    src/extension/deserialize.cc
    src/extension/reflection.cc
    src/extension/serialize.cc
    src/extension/utils.cc
)

add_library(protocache ${PROTOCACHE_CORE_SOURCES} ${PROTOCACHE_EXTENSION_SOURCES})
target_link_libraries(protocache protobuf pthread)

add_library(protocache-shared SHARED ${PROTOCACHE_CORE_SOURCES} ${PROTOCACHE_EXTENSION_SOURCES})
target_link_libraries(protocache-shared protobuf pthread)
set_target_properties(protocache-shared PROPERTIES OUTPUT_NAME protocache)

if(WITH_TEST)
    enable_testing()
    find_package(GTest REQUIRED)

    set(PROTOCACHE_TEST_SOURCES
        test/main.cc
        test/perfect_hash.cc
        test/protocache.cc
    )

    add_executable(protocache-test ${PROTOCACHE_TEST_SOURCES})
    target_link_libraries(protocache-test protocache protobuf gtest pthread)
    add_test(NAME protocache-test COMMAND protocache-test)
    set_tests_properties(protocache-test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    )
endif()

if(WITH_TOOLS)
    find_package(gflags REQUIRED)

    add_executable(json-to-binary tools/json-to-binary.cc)
    target_link_libraries(json-to-binary protocache protobuf gflags pthread)

    add_executable(binary-to-json tools/binary-to-json.cc)
    target_link_libraries(binary-to-json protocache protobuf gflags pthread)

    add_executable(protoc-gen-pccx tools/protoc-gen-pccx.cc)
    target_link_libraries(protoc-gen-pccx protoc protobuf pthread)

    add_executable(protoc-gen-pcjv tools/protoc-gen-pcjv.cc)
    target_link_libraries(protoc-gen-pcjv protoc protobuf pthread)

    add_executable(protoc-gen-pc.net tools/protoc-gen-pc.net.cc)
    target_link_libraries(protoc-gen-pc.net protoc protobuf pthread)
endif()

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/protocache"
    DESTINATION include
)

install(
    TARGETS protocache protocache-shared protocache-lite
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

if(WITH_TOOLS)
    install(
        TARGETS protoc-gen-pccx protoc-gen-pcjv protoc-gen-pc.net
        RUNTIME DESTINATION bin
    )
endif()

if(WITH_BENCHMARK)
    file(GLOB PROTOCACHE_BENCHMARK_SOURCES
        test/benchmark/*.cc
    )

    add_executable(benchmark ${PROTOCACHE_BENCHMARK_SOURCES})
    target_link_libraries(benchmark protocache protobuf flatbuffers capnp kj pthread)
endif()
