#include "protocache_wasm.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "protocache/perfect_hash.h"
#include "protocache/utils.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/heap.h>
#endif

namespace {

constexpr uint32_t kAbiVersion = 5;
thread_local uint32_t g_last_error = PC_WASM_OK;

template <typename T>
T* FromAddress(pc_wasm_ptr_t address) noexcept {
  return reinterpret_cast<T*>(static_cast<uintptr_t>(address));
}

template <typename T>
pc_wasm_ptr_t ToAddress(T* pointer) noexcept {
  return static_cast<pc_wasm_ptr_t>(reinterpret_cast<uintptr_t>(pointer));
}

int32_t Fail(PcWasmStatus status) noexcept {
  g_last_error = static_cast<uint32_t>(status);
  return status;
}

PcWasmStatus ValidateRange(
    pc_wasm_ptr_t address,
    uint64_t length,
    size_t alignment) noexcept {
  if (address == 0) {
    return PC_WASM_INVALID_ARGUMENT;
  }
  if (address % alignment != 0) {
    return PC_WASM_UNALIGNED;
  }
  const uint64_t end = static_cast<uint64_t>(address) + length;
  if (end < address) {
    return PC_WASM_OUT_OF_BOUNDS;
  }
#if defined(__EMSCRIPTEN__)
  return end <= emscripten_get_heap_size()
      ? PC_WASM_OK
      : PC_WASM_OUT_OF_BOUNDS;
#else
  return end <= std::numeric_limits<uintptr_t>::max()
      ? PC_WASM_OK
      : PC_WASM_OUT_OF_BOUNDS;
#endif
}

struct Allocation final {
  std::vector<uint8_t> index;
  std::vector<uint32_t> slot_to_input;
};

struct BytesAllocation final {
  std::string data;
};

bool ValidDecompressedSizeHeader(
    const uint8_t* input,
    uint32_t input_len) noexcept {
  if (input_len == 0) {
    return true;
  }
  uint64_t size = 0;
  for (uint32_t index = 0, shift = 0;
       index < input_len && shift < 32;
       ++index, shift += 7) {
    const auto byte = input[index];
    size |= static_cast<uint64_t>(byte & 0x7fU) << shift;
    if ((byte & 0x80U) == 0) {
      return size <= std::numeric_limits<uint32_t>::max() - 7U;
    }
  }
  return false;
}

int32_t TransformBytes(
    pc_wasm_ptr_t input_ptr,
    uint32_t input_len,
    pc_wasm_ptr_t result_ptr,
    bool decompress) noexcept {
  const auto result_validation = ValidateRange(
      result_ptr, sizeof(PcBytesResult), alignof(PcBytesResult));
  if (result_validation != PC_WASM_OK) {
    return Fail(result_validation);
  }

  auto* result = FromAddress<PcBytesResult>(result_ptr);
  *result = {};

  if (input_len != 0) {
    const auto input_validation = ValidateRange(input_ptr, input_len, 1);
    if (input_validation != PC_WASM_OK) {
      return Fail(input_validation);
    }
  }
  const auto* input = FromAddress<const uint8_t>(input_ptr);
  if (decompress &&
      !ValidDecompressedSizeHeader(input, input_len)) {
    return Fail(PC_WASM_INVALID_DATA);
  }

  try {
    auto allocation = std::make_unique<BytesAllocation>();
    if (decompress) {
      if (!protocache::Decompress(
              input, input_len, &allocation->data)) {
        return Fail(PC_WASM_INVALID_DATA);
      }
    } else {
      protocache::Compress(input, input_len, &allocation->data);
    }
    if (allocation->data.size() >
        std::numeric_limits<uint32_t>::max()) {
      return Fail(PC_WASM_OUT_OF_MEMORY);
    }

    result->data_ptr = allocation->data.empty()
        ? 0
        : ToAddress(allocation->data.data());
    result->data_len =
        static_cast<uint32_t>(allocation->data.size());
    result->allocation_handle = ToAddress(allocation.release());
    g_last_error = PC_WASM_OK;
    return PC_WASM_OK;
  } catch (const std::bad_alloc&) {
    return Fail(PC_WASM_OUT_OF_MEMORY);
  } catch (...) {
    return Fail(PC_WASM_INTERNAL_ERROR);
  }
}

class PackedKeyReader final : public protocache::KeyReader {
 public:
  PackedKeyReader(
      const uint8_t* keys,
      const PcKeySpan* spans,
      uint32_t count) noexcept
      : keys_(keys), spans_(spans), count_(count) {}

  void Reset() override { position_ = 0; }
  size_t Total() override { return count_; }

  protocache::Slice<uint8_t> Read() override {
    if (position_ >= count_) {
      return {};
    }
    const auto& span = spans_[position_++];
    return {keys_ + span.offset, span.length};
  }

 private:
  const uint8_t* keys_;
  const PcKeySpan* spans_;
  uint32_t count_;
  uint32_t position_ = 0;
};

PcWasmStatus ValidateInput(
    const uint8_t* keys,
    uint32_t keys_len,
    const PcKeySpan* spans,
    uint32_t key_count,
    uint32_t flags) noexcept {
  if ((flags & ~PC_PH_TRUST_UNIQUE) != 0) {
    return PC_WASM_INVALID_ARGUMENT;
  }
  if (key_count >= (1U << 28U)) {
    return PC_WASM_INVALID_ARGUMENT;
  }
  if (key_count == 0) {
    return PC_WASM_OK;
  }
  if (keys == nullptr || spans == nullptr || keys_len == 0) {
    return PC_WASM_INVALID_ARGUMENT;
  }
  if (reinterpret_cast<uintptr_t>(keys) % alignof(uint64_t) != 0 ||
      reinterpret_cast<uintptr_t>(spans) % alignof(PcKeySpan) != 0) {
    return PC_WASM_UNALIGNED;
  }
  for (uint32_t index = 0; index < key_count; ++index) {
    const auto& span = spans[index];
    if ((span.offset & 7U) != 0) {
      return PC_WASM_UNALIGNED;
    }
    if (span.offset > keys_len || span.length > keys_len - span.offset) {
      return PC_WASM_OUT_OF_BOUNDS;
    }
  }
  return PC_WASM_OK;
}

}  // namespace

#if defined(__wasm__)
namespace protocache {
void InitializeWasmSeed(uint32_t seed) noexcept;
}
#endif

extern "C" {

uint32_t pc_wasm_abi_version() {
  return kAbiVersion;
}

void pc_seed_initialize(uint32_t seed) noexcept {
#if defined(__wasm__)
  protocache::InitializeWasmSeed(seed);
#else
  (void)seed;
#endif
}

pc_wasm_ptr_t pc_alloc(uint32_t size) {
  return size == 0 ? 0 : ToAddress(std::malloc(size));
}

void pc_free(pc_wasm_ptr_t ptr) {
  std::free(FromAddress<void>(ptr));
}

int32_t pc_ph_build(
    pc_wasm_ptr_t keys_ptr,
    uint32_t keys_len,
    pc_wasm_ptr_t key_spans_ptr,
    uint32_t key_count,
    uint32_t flags,
    pc_wasm_ptr_t result_ptr) {
  const auto result_validation = ValidateRange(
      result_ptr, sizeof(PcPhBuildResult), alignof(PcPhBuildResult));
  if (result_validation != PC_WASM_OK) {
    return Fail(result_validation);
  }

  auto* result = FromAddress<PcPhBuildResult>(result_ptr);
  *result = {};

  const uint64_t spans_size =
      static_cast<uint64_t>(key_count) * sizeof(PcKeySpan);
  if (key_count != 0) {
    const auto keys_validation =
        ValidateRange(keys_ptr, keys_len, alignof(uint64_t));
    if (keys_validation != PC_WASM_OK) {
      return Fail(keys_validation);
    }
    const auto spans_validation = ValidateRange(
        key_spans_ptr, spans_size, alignof(PcKeySpan));
    if (spans_validation != PC_WASM_OK) {
      return Fail(spans_validation);
    }
  }

  const auto* keys = FromAddress<const uint8_t>(keys_ptr);
  const auto* spans = FromAddress<const PcKeySpan>(key_spans_ptr);

  const auto validation =
      ValidateInput(keys, keys_len, spans, key_count, flags);
  if (validation != PC_WASM_OK) {
    return Fail(validation);
  }

  try {
    static const PcKeySpan kEmptySpan{0, 0};
    alignas(8) static const uint8_t kEmptyKey = 0;
    if (key_count == 0) {
      keys = &kEmptyKey;
      spans = &kEmptySpan;
    }

    PackedKeyReader reader(keys, spans, key_count);
    auto perfect_hash = protocache::PerfectHashObject::Build(
        reader, (flags & PC_PH_TRUST_UNIQUE) != 0);
    constexpr uint32_t kBuildAttempts = 8;
    for (uint32_t attempt = 1;
         attempt < kBuildAttempts && !perfect_hash;
         ++attempt) {
      perfect_hash = protocache::PerfectHashObject::Build(
          reader, (flags & PC_PH_TRUST_UNIQUE) != 0);
    }
    if (!perfect_hash) {
      return Fail(PC_WASM_BUILD_FAILED);
    }

    auto allocation = std::make_unique<Allocation>();
    const auto index = perfect_hash.Data();
    allocation->index.assign(index.begin(), index.end());
    allocation->slot_to_input.resize(key_count);
    std::vector<uint8_t> occupied(key_count);

    for (uint32_t input = 0; input < key_count; ++input) {
      const auto& span = spans[input];
      const auto slot = perfect_hash.Locate(keys + span.offset, span.length);
      if (slot >= key_count || occupied[slot] != 0) {
        return Fail(PC_WASM_INTERNAL_ERROR);
      }
      occupied[slot] = 1;
      allocation->slot_to_input[slot] = input;
    }

    result->index_ptr = ToAddress(allocation->index.data());
    result->index_len = static_cast<uint32_t>(allocation->index.size());
    result->slot_to_input_ptr =
        ToAddress(allocation->slot_to_input.data());
    result->slot_count = key_count;
    result->allocation_handle = ToAddress(allocation.release());
    g_last_error = PC_WASM_OK;
    return PC_WASM_OK;
  } catch (const std::bad_alloc&) {
    return Fail(PC_WASM_OUT_OF_MEMORY);
  } catch (...) {
    return Fail(PC_WASM_INTERNAL_ERROR);
  }
}

void pc_ph_build_release(pc_wasm_ptr_t allocation_handle) {
  delete FromAddress<Allocation>(allocation_handle);
}

int32_t pc_compress(
    pc_wasm_ptr_t input_ptr,
    uint32_t input_len,
    pc_wasm_ptr_t result_ptr) {
  return TransformBytes(input_ptr, input_len, result_ptr, false);
}

int32_t pc_decompress(
    pc_wasm_ptr_t input_ptr,
    uint32_t input_len,
    pc_wasm_ptr_t result_ptr) {
  return TransformBytes(input_ptr, input_len, result_ptr, true);
}

void pc_bytes_release(pc_wasm_ptr_t allocation_handle) {
  delete FromAddress<BytesAllocation>(allocation_handle);
}

uint32_t pc_last_error() {
  return g_last_error;
}

}  // extern "C"

#if defined(__wasm32__)
static_assert(sizeof(pc_wasm_ptr_t) == 4);
static_assert(sizeof(PcKeySpan) == 8);
static_assert(alignof(PcKeySpan) == 4);
static_assert(sizeof(PcPhBuildResult) == 20);
static_assert(alignof(PcPhBuildResult) == 4);
static_assert(sizeof(PcBytesResult) == 12);
static_assert(alignof(PcBytesResult) == 4);
#endif
