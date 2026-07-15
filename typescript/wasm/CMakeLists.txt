cmake_minimum_required(VERSION 3.16)
project(protocache-wasm-adapter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROTOCACHE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(PROTOCACHE_WASM_SOURCES
  protocache_wasm.cc
  decode_tape.cc
  serialize_arena.cc
  "${PROTOCACHE_ROOT}/src/hash.cc"
  "${PROTOCACHE_ROOT}/src/perfect_hash.cc"
  "${PROTOCACHE_ROOT}/src/serialize.cc"
  "${PROTOCACHE_ROOT}/src/utils.cc"
)

if(EMSCRIPTEN)
  add_executable(protocache-wasm ${PROTOCACHE_WASM_SOURCES})
  target_include_directories(protocache-wasm PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${PROTOCACHE_ROOT}/include"
    "${PROTOCACHE_ROOT}/src"
  )
  target_compile_definitions(protocache-wasm PRIVATE NDEBUG)
  target_compile_options(protocache-wasm PRIVATE -fwasm-exceptions)
  target_link_options(protocache-wasm PRIVATE
    --no-entry
    -fwasm-exceptions
    -sSTANDALONE_WASM=1
    -sALLOW_MEMORY_GROWTH=1
    -sFILESYSTEM=0
    "-sEXPORTED_FUNCTIONS=['_pc_wasm_abi_version','_pc_seed_initialize','_pc_alloc','_pc_free','_pc_ph_build','_pc_ph_build_release','_pc_compress','_pc_decompress','_pc_bytes_release','_pc_arena_buffer','_pc_arena_capacity','_pc_arena_reserve','_pc_arena_serialize','_pc_arena_output','_pc_decode_input_reserve','_pc_decode_input_capacity','_pc_decode_plan_reserve','_pc_decode_schema_reserve','_pc_decode_schema_count','_pc_decode_plan_commit','_pc_decode_tape','_pc_decode_tape_output','_pc_decode_last_error','_pc_last_error']"
  )
  set_target_properties(protocache-wasm PROPERTIES
    OUTPUT_NAME protocache
    SUFFIX ".wasm"
  )
else()
  add_library(protocache-wasm-adapter STATIC ${PROTOCACHE_WASM_SOURCES})
  target_include_directories(protocache-wasm-adapter
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}"
    PRIVATE
      "${PROTOCACHE_ROOT}/include"
      "${PROTOCACHE_ROOT}/src"
  )
  target_compile_definitions(protocache-wasm-adapter PRIVATE NDEBUG)

  enable_testing()
  add_executable(protocache-wasm-native-test native_test.cc)
  target_include_directories(protocache-wasm-native-test PRIVATE
    "${PROTOCACHE_ROOT}/include"
  )
  target_link_libraries(protocache-wasm-native-test
    PRIVATE protocache-wasm-adapter
  )
  add_test(NAME protocache-wasm-native-test
    COMMAND protocache-wasm-native-test
  )
endif()
