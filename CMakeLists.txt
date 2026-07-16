# Copyright (c) 2023, Ruan Kunliang.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

cmake_minimum_required(VERSION 3.13)
project(ProtoCache VERSION 1.2.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WITH_TEST "enable test" ON)
option(WITH_TOOLS "build command line tools and protoc plugins" ON)
option(WITH_BENCHMARK "build benchmark target" OFF)
option(PROTOCACHE_ENABLE_NATIVE_OPT "enable native CPU optimizations" OFF)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

set(PROTOCACHE_USE_NATIVE_OPT ${PROTOCACHE_ENABLE_NATIVE_OPT})
if(WITH_BENCHMARK)
    set(PROTOCACHE_USE_NATIVE_OPT ON)
endif()

function(protocache_configure_library target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_17)
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    if(PROTOCACHE_USE_NATIVE_OPT)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target_name} PRIVATE -march=native)
        else()
            message(WARNING
                "PROTOCACHE_ENABLE_NATIVE_OPT is not supported by "
                "${CMAKE_CXX_COMPILER_ID}; building ${target_name} without it"
            )
        endif()
    endif()
endfunction()

set(PROTOCACHE_CORE_SOURCES
    src/access.cc
    src/hash.cc
    src/perfect_hash.cc
    src/serialize.cc
    src/utils.cc
)

add_library(protocache-lite ${PROTOCACHE_CORE_SOURCES})
add_library(ProtoCache::protocache-lite ALIAS protocache-lite)
protocache_configure_library(protocache-lite)
set_target_properties(protocache-lite PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(PROTOCACHE_EXTENSION_SOURCES
    src/extension/deserialize.cc
    src/extension/reflection.cc
    src/extension/serialize.cc
    src/extension/utils.cc
)

add_library(protocache ${PROTOCACHE_CORE_SOURCES} ${PROTOCACHE_EXTENSION_SOURCES})
add_library(ProtoCache::protocache ALIAS protocache)
protocache_configure_library(protocache)
target_link_libraries(protocache PUBLIC protobuf::libprotobuf Threads::Threads)

add_library(protocache-shared SHARED ${PROTOCACHE_CORE_SOURCES} ${PROTOCACHE_EXTENSION_SOURCES})
add_library(ProtoCache::protocache-shared ALIAS protocache-shared)
protocache_configure_library(protocache-shared)
target_link_libraries(protocache-shared PUBLIC protobuf::libprotobuf Threads::Threads)
set_target_properties(protocache-shared PROPERTIES
    OUTPUT_NAME protocache
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(WITH_TEST)
    enable_testing()
    find_package(GTest REQUIRED)

    set(PROTOCACHE_TEST_SOURCES
        test/main.cc
        test/perfect_hash.cc
        test/protocache.cc
    )

    add_executable(protocache-test ${PROTOCACHE_TEST_SOURCES})
    target_link_libraries(protocache-test PRIVATE ProtoCache::protocache GTest::GTest)
    add_test(NAME protocache-test COMMAND protocache-test)
    set_tests_properties(protocache-test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    )
endif()

if(WITH_TOOLS)
    find_package(gflags REQUIRED)

    add_executable(json-to-binary tools/json-to-binary.cc)
    target_link_libraries(json-to-binary PRIVATE ProtoCache::protocache ${GFLAGS_TARGET})

    add_executable(binary-to-json tools/binary-to-json.cc)
    target_link_libraries(binary-to-json PRIVATE ProtoCache::protocache ${GFLAGS_TARGET})

    add_executable(protoc-gen-pccx tools/protoc-gen-pccx.cc)
    target_link_libraries(protoc-gen-pccx PRIVATE protobuf::libprotoc protobuf::libprotobuf Threads::Threads)

    add_executable(protoc-gen-pcjv tools/protoc-gen-pcjv.cc)
    target_link_libraries(protoc-gen-pcjv PRIVATE protobuf::libprotoc protobuf::libprotobuf Threads::Threads)

    add_executable(protoc-gen-pc.net tools/protoc-gen-pc.net.cc)
    target_link_libraries(protoc-gen-pc.net PRIVATE protobuf::libprotoc protobuf::libprotobuf Threads::Threads)

    add_executable(protoc-gen-pcpy tools/protoc-gen-pcpy.cc)
    target_link_libraries(protoc-gen-pcpy PRIVATE protobuf::libprotoc protobuf::libprotobuf Threads::Threads)

    add_executable(protoc-gen-pcts tools/protoc-gen-pcts.cc)
    target_link_libraries(protoc-gen-pcts PRIVATE protobuf::libprotoc protobuf::libprotobuf Threads::Threads)
endif()

if(WITH_TEST AND WITH_TOOLS)
    if(TARGET protobuf::protoc)
        set(PROTOCACHE_PROTOC_EXECUTABLE "$<TARGET_FILE:protobuf::protoc>")
    else()
        set(PROTOCACHE_PROTOC_EXECUTABLE "${Protobuf_PROTOC_EXECUTABLE}")
    endif()

    add_dependencies(protocache-test protoc-gen-pccx protoc-gen-pcpy protoc-gen-pcts)

    add_test(
        NAME protocache-generator-cpp
        COMMAND ${CMAKE_COMMAND}
            -DPROTOC_EXECUTABLE=${PROTOCACHE_PROTOC_EXECUTABLE}
            -DPLUGIN_EXECUTABLE=$<TARGET_FILE:protoc-gen-pccx>
            -DGENERATOR_NAME=pccx
            -DGENERATOR_OPTION=extra
            -DPROTO_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/test
            -DPROTO_FILE=${CMAKE_CURRENT_SOURCE_DIR}/test/test.proto
            -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated/cpp
            -DGENERATED_FILE_1=test.pc.h
            -DEXPECTED_FILE_1=${CMAKE_CURRENT_SOURCE_DIR}/test/test.pc.h
            -DGENERATED_FILE_2=test.pc-ex.h
            -DEXPECTED_FILE_2=${CMAKE_CURRENT_SOURCE_DIR}/test/test.pc-ex.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyGenerated.cmake
    )

    add_test(
        NAME protocache-generator-python
        COMMAND ${CMAKE_COMMAND}
            -DPROTOC_EXECUTABLE=${PROTOCACHE_PROTOC_EXECUTABLE}
            -DPLUGIN_EXECUTABLE=$<TARGET_FILE:protoc-gen-pcpy>
            -DGENERATOR_NAME=pcpy
            -DPROTO_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/test
            -DPROTO_FILE=${CMAKE_CURRENT_SOURCE_DIR}/test/test.proto
            -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated/python
            -DGENERATED_FILE_1=test_pc.py
            -DEXPECTED_FILE_1=${CMAKE_CURRENT_SOURCE_DIR}/test/test_pc.py
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyGenerated.cmake
    )

    add_test(
        NAME protocache-generator-typescript
        COMMAND ${CMAKE_COMMAND}
            -DPROTOC_EXECUTABLE=${PROTOCACHE_PROTOC_EXECUTABLE}
            -DPLUGIN_EXECUTABLE=$<TARGET_FILE:protoc-gen-pcts>
            -DGENERATOR_NAME=pcts
            -DGENERATOR_OPTION=runtime_import=../../src/index.js
            -DPROTO_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/test
            -DPROTO_FILE=${CMAKE_CURRENT_SOURCE_DIR}/test/test.proto
            -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated/typescript
            -DGENERATED_FILE_1=test.pc.ts
            -DEXPECTED_FILE_1=${CMAKE_CURRENT_SOURCE_DIR}/typescript/test/generated/test.pc.ts
            -DGENERATED_FILE_2=test.pc.internal.ts
            -DEXPECTED_FILE_2=${CMAKE_CURRENT_SOURCE_DIR}/typescript/test/generated/test.pc.internal.ts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyGenerated.cmake
    )
endif()

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/protocache"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    TARGETS protocache protocache-shared protocache-lite
    EXPORT ProtoCacheTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(WITH_TOOLS)
    install(
        TARGETS
            json-to-binary
            binary-to-json
            protoc-gen-pccx
            protoc-gen-pcjv
            protoc-gen-pc.net
            protoc-gen-pcpy
            protoc-gen-pcts
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

set(PROTOCACHE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/ProtoCache")
configure_package_config_file(
    cmake/ProtoCacheConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ProtoCacheConfig.cmake"
    INSTALL_DESTINATION "${PROTOCACHE_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ProtoCacheConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ProtoCacheConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ProtoCacheConfigVersion.cmake"
    DESTINATION "${PROTOCACHE_INSTALL_CMAKEDIR}"
)
install(
    EXPORT ProtoCacheTargets
    FILE ProtoCacheTargets.cmake
    NAMESPACE ProtoCache::
    DESTINATION "${PROTOCACHE_INSTALL_CMAKEDIR}"
)

if(WITH_BENCHMARK)
    set(PROTOCACHE_CAPNPROTO_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/capnproto"
        CACHE PATH "Cap'n Proto prefix used by the benchmark"
    )
    set(PROTOCACHE_FORY_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/fory"
        CACHE PATH "Fory prefix used by the benchmark"
    )
    foreach(required_directory IN ITEMS
        "${PROTOCACHE_CAPNPROTO_ROOT}/include"
        "${PROTOCACHE_CAPNPROTO_ROOT}/lib"
        "${PROTOCACHE_FORY_ROOT}/include"
        "${PROTOCACHE_FORY_ROOT}/lib"
    )
        if(NOT IS_DIRECTORY "${required_directory}")
            message(FATAL_ERROR
                "WITH_BENCHMARK requires ${required_directory}; "
                "set PROTOCACHE_CAPNPROTO_ROOT/PROTOCACHE_FORY_ROOT as needed"
            )
        endif()
    endforeach()

    file(GLOB PROTOCACHE_BENCHMARK_SOURCES
        test/benchmark/*.cc
    )

    add_executable(benchmark ${PROTOCACHE_BENCHMARK_SOURCES} ${FORY_BENCHMARK_GENERATED_HEADER})
    target_include_directories(benchmark PRIVATE
        "${PROTOCACHE_CAPNPROTO_ROOT}/include"
        "${PROTOCACHE_FORY_ROOT}/include"
    )
    target_link_directories(benchmark PRIVATE
        "${PROTOCACHE_CAPNPROTO_ROOT}/lib"
        "${PROTOCACHE_FORY_ROOT}/lib"
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(benchmark PRIVATE -march=native)
    endif()
    target_link_libraries(benchmark PRIVATE ProtoCache::protocache flatbuffers capnp kj fory_serialization fory_meta fory_util fory_thirdparty)
endif()
