cmake_minimum_required(VERSION 3.13)
project(ProtoCacheConsumer LANGUAGES CXX)

find_package(ProtoCache 1.2 CONFIG REQUIRED)

add_executable(protocache-consumer main.cc)
target_link_libraries(protocache-consumer PRIVATE ProtoCache::protocache)

enable_testing()
add_test(NAME protocache-consumer COMMAND protocache-consumer)
