#include "protocache_wasm.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "protocache/perfect_hash.h"

namespace {

[[noreturn]] void CheckFailed(const char* expression, int line) {
  std::fprintf(stderr, "CHECK failed at line %d: %s\n", line, expression);
  std::abort();
}

#define CHECK(expression) \
  do { \
    if (!(expression)) CheckFailed(#expression, __LINE__); \
  } while (false)

template <typename T>
pc_wasm_ptr_t Address(T* pointer) {
  return static_cast<pc_wasm_ptr_t>(reinterpret_cast<uintptr_t>(pointer));
}

struct PackedKeys {
  std::vector<uint64_t> storage;
  std::vector<PcKeySpan> spans;

  explicit PackedKeys(const std::vector<std::string>& keys) {
    size_t bytes = 8;
    for (const auto& key : keys) {
      bytes += (key.size() + 7) & ~size_t{7};
    }
    storage.resize((bytes + 7) / 8);
    auto* data = reinterpret_cast<uint8_t*>(storage.data());
    uint32_t offset = 0;
    for (const auto& key : keys) {
      spans.push_back({offset, static_cast<uint32_t>(key.size())});
      if (!key.empty()) {
        std::memcpy(data + offset, key.data(), key.size());
      }
      offset += static_cast<uint32_t>((key.size() + 7) & ~size_t{7});
      if (offset == 0) {
        offset = 8;
      }
    }
  }

  uint8_t* data() {
    return reinterpret_cast<uint8_t*>(storage.data());
  }
  uint32_t byte_size() const {
    return static_cast<uint32_t>(storage.size() * sizeof(uint64_t));
  }
};

void TestBuildAndPermutation() {
  std::vector<std::string> keys{""};
  for (int index = 0; index < 40; ++index) {
    keys.push_back("key-" + std::to_string(index));
  }
  PackedKeys packed(keys);
  PcPhBuildResult result{};

  CHECK(pc_wasm_abi_version() == 5);
  pc_seed_initialize(0x1234'5678U);
  CHECK(pc_ph_build(
             Address(packed.data()),
             packed.byte_size(),
             Address(packed.spans.data()),
             static_cast<uint32_t>(packed.spans.size()),
             0,
             Address(&result)) == PC_WASM_OK);
  CHECK(result.index_ptr != 0);
  CHECK(result.index_len >= 8);
  CHECK(result.slot_count == keys.size());
  CHECK(result.allocation_handle != 0);

  const auto* index_bytes =
      reinterpret_cast<const uint8_t*>(result.index_ptr);
  const auto* permutation =
      reinterpret_cast<const uint32_t*>(result.slot_to_input_ptr);
  protocache::PerfectHash index(index_bytes, result.index_len);
  std::vector<bool> seen(keys.size());
  for (uint32_t slot = 0; slot < result.slot_count; ++slot) {
    const auto input = permutation[slot];
    CHECK(input < keys.size());
    CHECK(!seen[input]);
    seen[input] = true;
    const auto& span = packed.spans[input];
    CHECK(index.Locate(packed.data() + span.offset, span.length) == slot);
  }
  pc_ph_build_release(result.allocation_handle);
}

void TestEmptyAndInvalidInputs() {
  PcPhBuildResult empty{};
  CHECK(pc_ph_build(0, 0, 0, 0, 0, Address(&empty)) == PC_WASM_OK);
  CHECK(empty.index_len == 4);
  CHECK(empty.slot_count == 0);
  pc_ph_build_release(empty.allocation_handle);

  PackedKeys packed({"alpha", "beta"});
  PcPhBuildResult result{};
  CHECK(pc_ph_build(
             Address(packed.data()) + 1,
             packed.byte_size(),
             Address(packed.spans.data()),
             static_cast<uint32_t>(packed.spans.size()),
             0,
             Address(&result)) == PC_WASM_UNALIGNED);
  CHECK(pc_ph_build(
             Address(packed.data()),
             packed.byte_size(),
             Address(packed.spans.data()) + 1,
             static_cast<uint32_t>(packed.spans.size()),
             0,
             Address(&result)) == PC_WASM_UNALIGNED);

  auto spans = packed.spans;
  spans[1].offset += 1;
  CHECK(pc_ph_build(
             Address(packed.data()),
             packed.byte_size(),
             Address(spans.data()),
             static_cast<uint32_t>(spans.size()),
             0,
             Address(&result)) == PC_WASM_UNALIGNED);
  CHECK(pc_last_error() == PC_WASM_UNALIGNED);
  CHECK(result.allocation_handle == 0);

  spans = packed.spans;
  spans[1].length = packed.byte_size();
  CHECK(pc_ph_build(
             Address(packed.data()),
             packed.byte_size(),
             Address(spans.data()),
             static_cast<uint32_t>(spans.size()),
             0,
             Address(&result)) == PC_WASM_OUT_OF_BOUNDS);

  PackedKeys duplicates({"same", "same"});
  CHECK(pc_ph_build(
             Address(duplicates.data()),
             duplicates.byte_size(),
             Address(duplicates.spans.data()),
             static_cast<uint32_t>(duplicates.spans.size()),
             0,
             Address(&result)) == PC_WASM_BUILD_FAILED);
}

void TestCompressAndDecompress() {
  const std::vector<uint8_t> raw{
      0, 0, 0, 0, 1, 2, 3, 4, 0xff, 0xff, 0xff, 9, 8, 7};
  PcBytesResult compressed{};
  CHECK(pc_compress(
             Address(raw.data()),
             static_cast<uint32_t>(raw.size()),
             Address(&compressed)) == PC_WASM_OK);
  CHECK(compressed.allocation_handle != 0);
  CHECK(compressed.data_len != 0);

  std::string expected;
  protocache::Compress(raw.data(), raw.size(), &expected);
  CHECK(compressed.data_len == expected.size());
  CHECK(std::memcmp(
            reinterpret_cast<const void*>(compressed.data_ptr),
            expected.data(),
            expected.size()) == 0);

  PcBytesResult restored{};
  CHECK(pc_decompress(
             compressed.data_ptr,
             compressed.data_len,
             Address(&restored)) == PC_WASM_OK);
  CHECK(restored.data_len == raw.size());
  CHECK(std::memcmp(
            reinterpret_cast<const void*>(restored.data_ptr),
            raw.data(),
            raw.size()) == 0);
  pc_bytes_release(restored.allocation_handle);
  pc_bytes_release(compressed.allocation_handle);

  PcBytesResult empty{};
  CHECK(pc_compress(0, 0, Address(&empty)) == PC_WASM_OK);
  CHECK(empty.data_len == 0);
  CHECK(empty.allocation_handle != 0);
  pc_bytes_release(empty.allocation_handle);
  CHECK(pc_decompress(0, 0, Address(&empty)) == PC_WASM_OK);
  CHECK(empty.data_len == 0);
  pc_bytes_release(empty.allocation_handle);

  const uint8_t invalid[] = {0xff};
  CHECK(pc_decompress(
             Address(invalid),
             sizeof(invalid),
             Address(&empty)) == PC_WASM_INVALID_DATA);
  CHECK(empty.allocation_handle == 0);
  CHECK(pc_last_error() == PC_WASM_INVALID_DATA);

  alignas(PcBytesResult) uint8_t result_storage[
      sizeof(PcBytesResult) + alignof(PcBytesResult)]{};
  CHECK(pc_compress(
             Address(raw.data()),
             static_cast<uint32_t>(raw.size()),
             Address(result_storage) + 1) == PC_WASM_UNALIGNED);
}

void TestRepeatedBoundaryBuilds() {
  std::vector<std::string> keys;
  for (uint32_t index = 0; index < 256; ++index) {
    keys.push_back("boundary-key-" + std::to_string(index));
  }
  PackedKeys packed(keys);
  for (uint32_t iteration = 0; iteration < 1000; ++iteration) {
    PcPhBuildResult result{};
    CHECK(pc_ph_build(
               Address(packed.data()),
               packed.byte_size(),
               Address(packed.spans.data()),
               static_cast<uint32_t>(packed.spans.size()),
               0,
               Address(&result)) == PC_WASM_OK);
    pc_ph_build_release(result.allocation_handle);
  }
}

void Store32(uint8_t* destination, uint32_t value) {
  std::memcpy(destination, &value, sizeof(value));
}

void TestSerializeArena() {
  constexpr uint32_t kNoType = 0xffff'ffffU;
  auto register_schema = [](uint32_t kind, const uint32_t* plan,
                            uint32_t size) {
    const auto handle = pc_decode_schema_reserve(kind);
    CHECK(handle != 0);
    const auto address = pc_decode_plan_reserve(size);
    CHECK(address != 0);
    std::memcpy(reinterpret_cast<void*>(address), plan, size);
    CHECK(pc_decode_plan_commit(handle, size) == PC_WASM_OK);
    return handle;
  };
  const uint32_t message_plan[] = {
      1, 1, 7, kNoType,
      0, 9, kNoType,
  };
  const uint32_t array_plan[] = {20, 9, 4, kNoType};
  const uint32_t map_plan[] = {21, 3U | (9U << 8U), 4, kNoType};
  const auto message_schema = register_schema(
      1, message_plan, sizeof(message_plan));
  const auto array_schema = register_schema(20, array_plan, sizeof(array_plan));
  const auto map_schema = register_schema(21, map_plan, sizeof(map_plan));

  const auto initial_capacity = pc_arena_capacity();
  CHECK(initial_capacity >= 64U * 1024U);
  auto arena_address = pc_arena_buffer();
  CHECK(arena_address != 0);
  auto* arena = reinterpret_cast<uint8_t*>(arena_address);
  arena[0] = 0xa5;
  arena_address = pc_arena_reserve(initial_capacity + 1U);
  CHECK(arena_address != 0);
  CHECK(pc_arena_capacity() >= initial_capacity + 1U);
  arena = reinterpret_cast<uint8_t*>(arena_address);
  CHECK(arena[0] == 0xa5);

  // Message { field 0: i32(-123) }.
  Store32(arena, 1);
  Store32(arena + 4, 1);
  Store32(arena + 8, 1);
  Store32(arena + 12, 36);
  Store32(arena + 16, 0);
  Store32(arena + 20, 9);
  Store32(arena + 24, 0);
  Store32(arena + 28, static_cast<uint32_t>(-123));
  Store32(arena + 32, 0);
  CHECK(pc_arena_serialize(message_schema, arena_address, 36) == 8);
  const auto* message_data = reinterpret_cast<const uint32_t*>(
      pc_arena_output());
  CHECK(message_data[0] == (1U << 8U));
  CHECK(static_cast<int32_t>(message_data[1]) == -123);

  // Empty Array<i32> and Map<string, i32> are inline one-word roots.
  Store32(arena, 20);
  Store32(arena + 4, 9);
  Store32(arena + 8, 0);
  Store32(arena + 12, 16);
  CHECK(pc_arena_serialize(array_schema, arena_address, 16) == 4);
  CHECK(*reinterpret_cast<const uint32_t*>(pc_arena_output()) == 1);

  Store32(arena + 8, 1);
  Store32(arena + 12, 24);
  Store32(arena + 16, 123);
  Store32(arena + 20, 0);
  CHECK(pc_arena_serialize(array_schema, arena_address, 24) == 0);

  Store32(arena, 21);
  Store32(arena + 4, 3U | (9U << 8U));
  Store32(arena + 8, 0);
  Store32(arena + 12, 16);
  CHECK(pc_arena_serialize(map_schema, arena_address, 16) == 4);
  CHECK(*reinterpret_cast<const uint32_t*>(pc_arena_output()) ==
        (5U << 28U));

  CHECK(pc_arena_serialize(map_schema, arena_address + 4, 12) == 0);
  Store32(arena, 0xffffU);
  CHECK(pc_arena_serialize(map_schema, arena_address, 16) == 0);
  Store32(arena, 1);
  Store32(arena + 12, 20);
  CHECK(pc_arena_serialize(message_schema, arena_address, 16) == 0);
}

void TestDecodeTape() {
  constexpr uint32_t kNoType = 0xffff'ffffU;
  const auto initial_schema_count = pc_decode_schema_count();
  const uint32_t plan[] = {
      1, 1, 7, kNoType,
      0, 9, kNoType,
  };
  const auto schema_handle = pc_decode_schema_reserve(1);
  CHECK(schema_handle != 0);
  CHECK(pc_decode_schema_count() == initial_schema_count + 1U);
  const auto plan_address = pc_decode_plan_reserve(sizeof(plan));
  CHECK(plan_address != 0);
  std::memcpy(reinterpret_cast<void*>(plan_address), plan, sizeof(plan));
  CHECK(pc_decode_plan_commit(schema_handle, sizeof(plan)) == PC_WASM_OK);

  const uint32_t message[] = {1U << 8U, 42};
  const auto input_address = pc_decode_input_reserve(sizeof(message));
  CHECK(input_address != 0);
  CHECK(pc_decode_input_capacity() >= sizeof(message));
  std::memcpy(
      reinterpret_cast<void*>(input_address), message, sizeof(message));
  CHECK(pc_decode_tape(schema_handle, sizeof(message)) ==
        7U * sizeof(uint32_t));
  const auto* tape = reinterpret_cast<const uint32_t*>(
      pc_decode_tape_output());
  CHECK(tape != nullptr);
  CHECK(tape[0] == 1);
  CHECK(tape[1] == schema_handle);
  CHECK(tape[2] == 1);
  CHECK(tape[5] == 1);
  CHECK(tape[6] == 1);

  CHECK(pc_decode_tape(schema_handle, sizeof(uint32_t)) == 0);
  CHECK(pc_decode_last_error() == PC_WASM_INVALID_DATA);
  CHECK(pc_decode_plan_commit(schema_handle, sizeof(plan) - 1U) ==
        PC_WASM_INVALID_DATA);

  const auto cyclic_a = pc_decode_schema_reserve(1);
  const auto cyclic_b = pc_decode_schema_reserve(1);
  CHECK(cyclic_a != 0 && cyclic_b != 0);
  CHECK(pc_decode_schema_count() == initial_schema_count + 3U);
  const uint32_t plan_a[] = {1, 1, 7, kNoType, 0, 1, cyclic_b};
  const uint32_t plan_b[] = {1, 1, 7, kNoType, 0, 1, cyclic_a};
  auto cyclic_plan = pc_decode_plan_reserve(sizeof(plan_a));
  CHECK(cyclic_plan != 0);
  std::memcpy(reinterpret_cast<void*>(cyclic_plan), plan_a, sizeof(plan_a));
  CHECK(pc_decode_plan_commit(cyclic_a, sizeof(plan_a)) == PC_WASM_OK);
  cyclic_plan = pc_decode_plan_reserve(sizeof(plan_b));
  CHECK(cyclic_plan != 0);
  std::memcpy(reinterpret_cast<void*>(cyclic_plan), plan_b, sizeof(plan_b));
  CHECK(pc_decode_plan_commit(cyclic_b, sizeof(plan_b)) == PC_WASM_OK);
  const uint32_t empty_message[] = {0};
  const auto empty_address = pc_decode_input_reserve(sizeof(empty_message));
  CHECK(empty_address != 0);
  std::memcpy(
      reinterpret_cast<void*>(empty_address), empty_message,
      sizeof(empty_message));
  CHECK(pc_decode_tape(cyclic_a, sizeof(empty_message)) ==
        7U * sizeof(uint32_t));
}

}  // namespace

int main() {
  TestBuildAndPermutation();
  TestEmptyAndInvalidInputs();
  TestCompressAndDecompress();
  TestRepeatedBoundaryBuilds();
  TestSerializeArena();
  TestDecodeTape();
  return 0;
}
