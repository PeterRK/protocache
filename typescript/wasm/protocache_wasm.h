#pragma once

#include <cstdint>

#if defined(__wasm32__)
using pc_wasm_ptr_t = uint32_t;
#else
using pc_wasm_ptr_t = uintptr_t;
#endif

extern "C" {

enum PcWasmStatus : int32_t {
  PC_WASM_OK = 0,
  PC_WASM_INVALID_ARGUMENT = 1,
  PC_WASM_OUT_OF_BOUNDS = 2,
  PC_WASM_UNALIGNED = 3,
  PC_WASM_BUILD_FAILED = 4,
  PC_WASM_OUT_OF_MEMORY = 5,
  PC_WASM_INTERNAL_ERROR = 6,
  PC_WASM_INVALID_DATA = 7,
};

enum PcPerfectHashFlags : uint32_t {
  PC_PH_TRUST_UNIQUE = 1U,
};

struct PcKeySpan {
  uint32_t offset;
  uint32_t length;
};

struct PcPhBuildResult {
  pc_wasm_ptr_t index_ptr;
  uint32_t index_len;
  pc_wasm_ptr_t slot_to_input_ptr;
  uint32_t slot_count;
  pc_wasm_ptr_t allocation_handle;
};

struct PcBytesResult {
  pc_wasm_ptr_t data_ptr;
  uint32_t data_len;
  pc_wasm_ptr_t allocation_handle;
};

uint32_t pc_wasm_abi_version();
void pc_seed_initialize(uint32_t seed) noexcept;
pc_wasm_ptr_t pc_alloc(uint32_t size);
void pc_free(pc_wasm_ptr_t ptr);
int32_t pc_ph_build(
    pc_wasm_ptr_t keys_ptr,
    uint32_t keys_len,
    pc_wasm_ptr_t key_spans_ptr,
    uint32_t key_count,
    uint32_t flags,
    pc_wasm_ptr_t result_ptr);
void pc_ph_build_release(pc_wasm_ptr_t allocation_handle);
int32_t pc_compress(
    pc_wasm_ptr_t input_ptr,
    uint32_t input_len,
    pc_wasm_ptr_t result_ptr);
int32_t pc_decompress(
    pc_wasm_ptr_t input_ptr,
    uint32_t input_len,
    pc_wasm_ptr_t result_ptr);
void pc_bytes_release(pc_wasm_ptr_t allocation_handle);
pc_wasm_ptr_t pc_arena_buffer() noexcept;
uint32_t pc_arena_capacity() noexcept;
pc_wasm_ptr_t pc_arena_reserve(uint32_t minimum_capacity) noexcept;
uint32_t pc_arena_serialize(
    uint32_t schema_handle,
    pc_wasm_ptr_t arena_ptr,
    uint32_t arena_len) noexcept;
pc_wasm_ptr_t pc_arena_output() noexcept;
pc_wasm_ptr_t pc_decode_input_reserve(uint32_t minimum_capacity) noexcept;
uint32_t pc_decode_input_capacity() noexcept;
pc_wasm_ptr_t pc_decode_plan_reserve(uint32_t minimum_capacity) noexcept;
uint32_t pc_decode_schema_reserve(uint32_t kind) noexcept;
uint32_t pc_decode_schema_count() noexcept;
uint32_t pc_decode_schema_kind(uint32_t schema_handle) noexcept;
uint32_t pc_decode_plan_commit(
    uint32_t schema_handle,
    uint32_t plan_length) noexcept;
uint32_t pc_decode_tape(
    uint32_t schema_handle,
    uint32_t input_length) noexcept;
pc_wasm_ptr_t pc_decode_tape_output() noexcept;
uint32_t pc_decode_last_error() noexcept;
uint32_t pc_last_error();

}  // extern "C"
