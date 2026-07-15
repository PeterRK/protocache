#include "protocache_wasm.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#if defined(__wasm32__)
#define PC_EXPORT __attribute__((visibility("default")))
#else
#define PC_EXPORT
#endif

namespace {

constexpr uint32_t kNoType = std::numeric_limits<uint32_t>::max();
constexpr uint32_t kMaximumDepth = 256;
constexpr uint32_t kMaximumBufferSize = 1U << 30U;
constexpr uint32_t kInitialInputCapacity = 64U * 1024U;
constexpr uint32_t kInitialPlanCapacity = 8U * 1024U;
constexpr uint32_t kInitialTapeWords = 2U * 1024U;
constexpr uint32_t kRealSizeMask = 0x0fff'ffffU;

enum Kind : uint32_t {
  kNone = 0,
  kMessage = 1,
  kBytes = 2,
  kString = 3,
  kF64 = 4,
  kF32 = 5,
  kU64 = 6,
  kU32 = 7,
  kI64 = 8,
  kI32 = 9,
  kBool = 10,
  kEnum = 11,
  kArray = 20,
  kMap = 21,
};

struct FieldPlan final {
  uint32_t id = 0;
  uint32_t kind = kNone;
  uint32_t child = kNoType;
};

struct TypePlan final {
  uint32_t kind = kNone;
  uint32_t value_kind = kNone;
  uint32_t key_kind = kNone;
  uint32_t child = kNoType;
  bool committed = false;
  std::vector<FieldPlan> fields;
  std::vector<int16_t> field_index_by_id;
};

struct Location final {
  uint32_t word_offset = 0;
  uint32_t limit_word_offset = 0;
};

struct ByteSpan final {
  uint32_t offset = 0;
  uint32_t length = 0;
};

std::vector<uint8_t> g_input(kInitialInputCapacity);
std::vector<uint8_t> g_plan(kInitialPlanCapacity);
std::vector<uint32_t> g_tape;
std::vector<TypePlan> g_types;
uint32_t g_decode_error = PC_WASM_OK;
uint32_t g_input_length = 0;

template <typename T>
pc_wasm_ptr_t ToAddress(T* pointer) noexcept {
  return static_cast<pc_wasm_ptr_t>(reinterpret_cast<uintptr_t>(pointer));
}

uint32_t Load32(const uint8_t* pointer) noexcept {
  uint32_t value;
  std::memcpy(&value, pointer, sizeof(value));
  return value;
}

bool ValidValueKind(uint32_t kind) noexcept {
  return (kind >= kMessage && kind <= kEnum) ||
      kind == kArray || kind == kMap;
}

bool ValidKeyKind(uint32_t kind) noexcept {
  return kind == kString || kind == kU64 || kind == kU32 ||
      kind == kI64 || kind == kI32;
}

bool IsComplex(uint32_t kind) noexcept {
  return kind == kMessage || kind == kArray || kind == kMap;
}

uint32_t ScalarWidthWords(uint32_t kind) noexcept {
  switch (kind) {
    case kBool:
    case kF32:
    case kU32:
    case kI32:
    case kEnum:
      return 1;
    case kF64:
    case kU64:
    case kI64:
      return 2;
    default:
      return 0;
  }
}

bool Reserve(std::vector<uint8_t>* buffer, uint32_t minimum) noexcept {
  if (minimum == 0 || minimum > kMaximumBufferSize) return false;
  if (buffer->size() >= minimum) return true;
  uint32_t capacity = static_cast<uint32_t>(buffer->size());
  while (capacity < minimum) {
    if (capacity > kMaximumBufferSize / 2U) {
      capacity = kMaximumBufferSize;
      break;
    }
    capacity *= 2U;
  }
  try {
    buffer->resize(capacity);
    return true;
  } catch (...) {
    return false;
  }
}

bool RequireWords(
    uint32_t offset,
    uint64_t width,
    uint32_t limit) noexcept {
  return offset <= limit && width <= static_cast<uint64_t>(limit - offset);
}

bool RequireInputWords(uint32_t offset, uint64_t width) noexcept {
  return RequireWords(offset, width, g_input_length / 4U);
}

bool ReadWord(uint32_t offset, uint32_t* out) noexcept {
  if (!RequireInputWords(offset, 1)) return false;
  *out = Load32(g_input.data() + static_cast<size_t>(offset) * 4U);
  return true;
}

bool ObjectLocation(
    uint32_t word_offset,
    uint32_t width,
    Location* out) noexcept {
  uint32_t first;
  if (width == 0 || !ReadWord(word_offset, &first)) return false;
  if ((first & 3U) == 3U) {
    const auto delta = first >> 2U;
    if (delta == 0 || delta > g_input_length / 4U - word_offset) {
      return false;
    }
    const auto target = word_offset + delta;
    if (!RequireInputWords(target, 1)) return false;
    *out = {target, g_input_length / 4U};
    return true;
  }
  if (!RequireInputWords(word_offset, width)) return false;
  *out = {word_offset, word_offset + width};
  return true;
}

bool ParseBytes(const Location& location, ByteSpan* out) noexcept {
  if (!RequireWords(location.word_offset, 1, location.limit_word_offset)) {
    return false;
  }
  uint64_t byte_offset = static_cast<uint64_t>(location.word_offset) * 4U;
  const uint64_t byte_limit =
      static_cast<uint64_t>(location.limit_word_offset) * 4U;
  uint64_t mark = 0;
  for (uint32_t shift = 0; shift <= 28; shift += 7) {
    if (byte_offset >= byte_limit || byte_offset >= g_input_length) {
      return false;
    }
    const auto byte = g_input[byte_offset++];
    mark += static_cast<uint64_t>(byte & 0x7fU) << shift;
    if ((byte & 0x80U) == 0) {
      if ((mark & 3U) != 0) return false;
      const auto length = mark / 4U;
      if (length > std::numeric_limits<uint32_t>::max() ||
          byte_offset > byte_limit || length > byte_limit - byte_offset ||
          byte_offset > g_input_length || length > g_input_length - byte_offset) {
        return false;
      }
      *out = {static_cast<uint32_t>(byte_offset),
              static_cast<uint32_t>(length)};
      return true;
    }
  }
  return false;
}

uint32_t SumWidths(uint32_t widths, uint32_t count) noexcept {
  uint32_t total = 0;
  for (uint32_t index = 0; index < count; ++index) {
    total += widths & 3U;
    widths >>= 2U;
  }
  return total;
}

uint32_t SectionForSize(uint32_t size) noexcept {
  return std::max(10U, static_cast<uint32_t>(
      (static_cast<uint64_t>(size) * 105U + 255U) / 256U));
}

uint32_t BitmapSizeForSection(uint32_t section) noexcept {
  return ((section * 3U + 31U) & ~31U) / 4U;
}

uint64_t IndexByteLength(uint32_t count) noexcept {
  if (count <= 1) return 4;
  const auto bitmap = BitmapSizeForSection(SectionForSize(count));
  uint64_t bytes = bitmap;
  if (count > 0xffffU) {
    bytes += bitmap / 2U;
  } else if (count > 0xffU) {
    bytes += bitmap / 4U;
  } else if (count > 24U) {
    bytes += bitmap / 8U;
  }
  return bytes + 8U;
}

bool Append(uint32_t value) noexcept {
  try {
    g_tape.push_back(value);
    return true;
  } catch (...) {
    g_decode_error = PC_WASM_OUT_OF_MEMORY;
    return false;
  }
}

bool AppendHeader(
    uint32_t kind,
    uint32_t type_id,
    uint32_t count,
    uint32_t data0,
    uint32_t data1) noexcept {
  return Append(kind) && Append(type_id) && Append(count) &&
      Append(data0) && Append(data1);
}

bool Visit(uint32_t type_id, const Location& location, uint32_t depth);

bool ValidateScalar(
    uint32_t kind,
    uint32_t word_offset,
    uint32_t width,
    uint32_t descriptor) noexcept {
  if (kind == kString || kind == kBytes) {
    Location object;
    ByteSpan bytes;
    if (!ObjectLocation(word_offset, width, &object) ||
        !ParseBytes(object, &bytes)) {
      return false;
    }
    g_tape[descriptor] = bytes.offset;
    g_tape[descriptor + 1U] = bytes.length;
    return true;
  }
  return width == ScalarWidthWords(kind);
}

bool VisitMessage(
    uint32_t type_id,
    const TypePlan& plan,
    const Location& location,
    uint32_t depth) {
  uint32_t first;
  if (!RequireWords(location.word_offset, 1, location.limit_word_offset) ||
      !ReadWord(location.word_offset, &first)) {
    return false;
  }
  const auto section_count = first & 0xffU;
  const uint64_t header_words = 1U + static_cast<uint64_t>(section_count) * 2U;
  if (!RequireWords(location.word_offset, header_words,
                    location.limit_word_offset)) {
    return false;
  }
  const auto body = location.word_offset + static_cast<uint32_t>(header_words);
  uint32_t body_words = 0;
  if (section_count == 0) {
    body_words = SumWidths(first >> 8U, 12);
  } else {
    uint32_t low;
    uint32_t high;
    const auto last = location.word_offset + 1U + (section_count - 1U) * 2U;
    if (!ReadWord(last, &low) || !ReadWord(last + 1U, &high)) return false;
    body_words = (high >> 18U) + SumWidths(low, 16) +
        SumWidths(high & 0x0003'ffffU, 9);
  }
  if (!RequireWords(body, body_words, location.limit_word_offset)) return false;

  if (!AppendHeader(kMessage, type_id,
                    static_cast<uint32_t>(plan.fields.size()), 0, 0)) {
    return false;
  }
  const auto descriptors = g_tape.size();
  try {
    g_tape.resize(descriptors + plan.fields.size() * 2U, 0);
  } catch (...) {
    g_decode_error = PC_WASM_OUT_OF_MEMORY;
    return false;
  }

  auto record = [&](uint32_t id, uint32_t word_offset, uint32_t width) {
    if (id >= plan.field_index_by_id.size()) return;
    const auto index = plan.field_index_by_id[id];
    if (index < 0) return;
    g_tape[descriptors + static_cast<uint32_t>(index) * 2U] = word_offset;
    g_tape[descriptors + static_cast<uint32_t>(index) * 2U + 1U] = width;
  };

  uint32_t offset = 0;
  uint32_t widths = first >> 8U;
  for (uint32_t id = 0; id < 12; ++id) {
    const auto width = widths & 3U;
    if (width != 0) {
      if (!RequireWords(body + offset, width, location.limit_word_offset)) {
        return false;
      }
      record(id, body + offset, width);
      offset += width;
    }
    widths >>= 2U;
  }
  for (uint32_t section = 0; section < section_count; ++section) {
    uint32_t low;
    uint32_t high;
    const auto section_offset = location.word_offset + 1U + section * 2U;
    if (!ReadWord(section_offset, &low) ||
        !ReadWord(section_offset + 1U, &high)) {
      return false;
    }
    offset = high >> 18U;
    for (uint32_t index = 0; index < 25; ++index) {
      const auto shift = index * 2U;
      const auto width = shift < 32U
          ? (low >> shift) & 3U
          : (high >> (shift - 32U)) & 3U;
      if (width != 0) {
        if (!RequireWords(body + offset, width, location.limit_word_offset)) {
          return false;
        }
        record(12U + section * 25U + index, body + offset, width);
        offset += width;
      }
    }
  }

  for (uint32_t index = 0; index < plan.fields.size(); ++index) {
    const auto descriptor = descriptors + index * 2U;
    const auto word_offset = g_tape[descriptor];
    const auto width = g_tape[descriptor + 1U];
    if (word_offset == 0) continue;
    const auto& field = plan.fields[index];
    if (!IsComplex(field.kind)) {
      if (!ValidateScalar(field.kind, word_offset, width, descriptor)) {
        return false;
      }
      continue;
    }
    Location child;
    if (depth >= kMaximumDepth || field.child == kNoType ||
        !ObjectLocation(word_offset, width, &child) ||
        !Visit(field.child, child, depth + 1U)) {
      return false;
    }
  }
  return true;
}

bool VisitArray(
    uint32_t type_id,
    const TypePlan& plan,
    const Location& location,
    uint32_t depth) {
  if (plan.value_kind == kBool) {
    ByteSpan bytes;
    if (!ParseBytes(location, &bytes)) return false;
    return AppendHeader(kArray, type_id, bytes.length, bytes.offset, 0);
  }

  uint32_t header;
  if (!RequireWords(location.word_offset, 1, location.limit_word_offset) ||
      !ReadWord(location.word_offset, &header)) {
    return false;
  }
  const auto width = header & 3U;
  const auto count = header >> 2U;
  const auto body = location.word_offset + 1U;
  if (width == 0 ||
      !RequireWords(body, static_cast<uint64_t>(width) * count,
                    location.limit_word_offset) ||
      !AppendHeader(kArray, type_id, count, body, width)) {
    return false;
  }

  if (plan.value_kind == kString || plan.value_kind == kBytes) {
    const auto descriptors = g_tape.size();
    try {
      g_tape.resize(descriptors + static_cast<size_t>(count) * 2U, 0);
    } catch (...) {
      g_decode_error = PC_WASM_OUT_OF_MEMORY;
      return false;
    }
    for (uint32_t index = 0; index < count; ++index) {
      if (!ValidateScalar(plan.value_kind, body + index * width, width,
                          static_cast<uint32_t>(descriptors) + index * 2U)) {
        return false;
      }
    }
    return true;
  }
  if (!IsComplex(plan.value_kind)) {
    return width == ScalarWidthWords(plan.value_kind);
  }
  if (depth >= kMaximumDepth || plan.child == kNoType) return false;
  for (uint32_t index = 0; index < count; ++index) {
    Location child;
    if (!ObjectLocation(body + index * width, width, &child) ||
        !Visit(plan.child, child, depth + 1U)) {
      return false;
    }
  }
  return true;
}

bool VisitMap(
    uint32_t type_id,
    const TypePlan& plan,
    const Location& location,
    uint32_t depth) {
  uint32_t header;
  if (!RequireWords(location.word_offset, 1, location.limit_word_offset) ||
      !ReadWord(location.word_offset, &header)) {
    return false;
  }
  const auto key_width = (header >> 30U) & 3U;
  const auto value_width = (header >> 28U) & 3U;
  const auto count = header & kRealSizeMask;
  const auto index_bytes = IndexByteLength(count);
  const auto body64 = static_cast<uint64_t>(location.word_offset) +
      (index_bytes + 3U) / 4U;
  if (key_width == 0 || value_width == 0 ||
      body64 > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  const auto body = static_cast<uint32_t>(body64);
  const auto stride = key_width + value_width;
  if (!RequireWords(body, static_cast<uint64_t>(stride) * count,
                    location.limit_word_offset) ||
      !AppendHeader(kMap, type_id, count, body,
                    key_width | (value_width << 16U))) {
    return false;
  }

  const bool string_key = plan.key_kind == kString;
  const bool byte_value =
      plan.value_kind == kString || plan.value_kind == kBytes;
  const uint32_t descriptor_stride =
      (string_key ? 2U : 0U) + (byte_value ? 2U : 0U);
  const auto descriptors = g_tape.size();
  if (descriptor_stride != 0) {
    try {
      g_tape.resize(descriptors +
          static_cast<size_t>(descriptor_stride) * count, 0);
    } catch (...) {
      g_decode_error = PC_WASM_OUT_OF_MEMORY;
      return false;
    }
  }

  const auto expected_key_width = ScalarWidthWords(plan.key_kind);
  if (!string_key && key_width != expected_key_width) return false;
  if (!IsComplex(plan.value_kind) && !byte_value &&
      value_width != ScalarWidthWords(plan.value_kind)) {
    return false;
  }

  for (uint32_t index = 0; index < count; ++index) {
    const auto key = body + index * stride;
    const auto value = key + key_width;
    auto descriptor = static_cast<uint32_t>(descriptors) +
        index * descriptor_stride;
    if (string_key) {
      if (!ValidateScalar(kString, key, key_width, descriptor)) return false;
      descriptor += 2U;
    }
    if (byte_value &&
        !ValidateScalar(plan.value_kind, value, value_width, descriptor)) {
      return false;
    }
  }
  if (!IsComplex(plan.value_kind)) return true;
  if (depth >= kMaximumDepth || plan.child == kNoType) return false;
  for (uint32_t index = 0; index < count; ++index) {
    const auto value = body + index * stride + key_width;
    Location child;
    if (!ObjectLocation(value, value_width, &child) ||
        !Visit(plan.child, child, depth + 1U)) {
      return false;
    }
  }
  return true;
}

bool Visit(uint32_t type_id, const Location& location, uint32_t depth) {
  if (type_id == 0 || type_id > g_types.size()) return false;
  const auto& plan = g_types[type_id - 1U];
  if (!plan.committed) return false;
  switch (plan.kind) {
    case kMessage:
      return VisitMessage(type_id, plan, location, depth);
    case kArray:
      return VisitArray(type_id, plan, location, depth);
    case kMap:
      return VisitMap(type_id, plan, location, depth);
    default:
      return false;
  }
}

bool CommitPlan(uint32_t handle, uint32_t length) {
  if (length < 16 || (length & 3U) != 0 || length > g_plan.size()) {
    return false;
  }
  if (handle == 0 || handle > g_types.size()) return false;
  const auto words = length / 4U;
  auto word = [&](uint32_t index) {
    return Load32(g_plan.data() + static_cast<size_t>(index) * 4U);
  };
  const auto kind = word(0);
  const auto auxiliary = word(1);
  const auto record_words = word(2);
  const auto child = word(3);
  if (record_words != words || record_words < 4U) return false;
  auto& reserved = g_types[handle - 1U];
  if (reserved.committed || reserved.kind != kind) return false;
  TypePlan type;
  type.kind = kind;
  type.child = child;
    if (kind == kMessage) {
      const auto field_count = auxiliary;
      if (record_words != 4U + field_count * 3U || field_count > 6387U) {
        return false;
      }
      type.fields.resize(field_count);
      uint32_t maximum_id = 0;
      for (uint32_t index = 0; index < field_count; ++index) {
        auto& field = type.fields[index];
        field.id = word(4U + index * 3U);
        field.kind = word(5U + index * 3U);
        field.child = word(6U + index * 3U);
        if (field.id >= 6387U || !ValidValueKind(field.kind) ||
            (IsComplex(field.kind) != (field.child != kNoType))) {
          return false;
        }
        maximum_id = std::max(maximum_id, field.id);
      }
      if (field_count != 0) {
        type.field_index_by_id.assign(maximum_id + 1U, -1);
        for (uint32_t index = 0; index < field_count; ++index) {
          const auto id = type.fields[index].id;
          if (type.field_index_by_id[id] != -1) return false;
          type.field_index_by_id[id] = static_cast<int16_t>(index);
        }
      }
    } else if (kind == kArray) {
      if (record_words != 4U || !ValidValueKind(auxiliary)) return false;
      type.value_kind = auxiliary;
      if (IsComplex(type.value_kind) != (child != kNoType)) return false;
    } else if (kind == kMap) {
      const auto key_kind = auxiliary & 0xffU;
      const auto value_kind = (auxiliary >> 8U) & 0xffU;
      if (record_words != 4U || !ValidKeyKind(key_kind) ||
          !ValidValueKind(value_kind)) {
        return false;
      }
      type.key_kind = key_kind;
      type.value_kind = value_kind;
      if (IsComplex(value_kind) != (child != kNoType)) return false;
    } else {
      return false;
    }
  auto validate_child = [&](uint32_t expected_kind, uint32_t child_handle) {
    return !IsComplex(expected_kind) ||
        (child_handle != 0 && child_handle <= g_types.size() &&
         g_types[child_handle - 1U].kind == expected_kind);
  };
  if (type.kind == kMessage) {
    for (const auto& field : type.fields) {
      if (!validate_child(field.kind, field.child)) return false;
    }
  } else if (!validate_child(type.value_kind, type.child)) {
    return false;
  }
  type.committed = true;
  reserved = std::move(type);
  return true;
}

}  // namespace

extern "C" {

PC_EXPORT pc_wasm_ptr_t pc_decode_input_reserve(
    uint32_t minimum_capacity) noexcept {
  if (!Reserve(&g_input, minimum_capacity)) {
    g_decode_error = PC_WASM_OUT_OF_MEMORY;
    return 0;
  }
  g_decode_error = PC_WASM_OK;
  return ToAddress(g_input.data());
}

PC_EXPORT uint32_t pc_decode_input_capacity() noexcept {
  return static_cast<uint32_t>(g_input.size());
}

PC_EXPORT pc_wasm_ptr_t pc_decode_plan_reserve(
    uint32_t minimum_capacity) noexcept {
  if (!Reserve(&g_plan, minimum_capacity)) {
    g_decode_error = PC_WASM_OUT_OF_MEMORY;
    return 0;
  }
  g_decode_error = PC_WASM_OK;
  return ToAddress(g_plan.data());
}

PC_EXPORT uint32_t pc_decode_schema_reserve(uint32_t kind) noexcept {
  if (kind != kMessage && kind != kArray && kind != kMap) {
    g_decode_error = PC_WASM_INVALID_ARGUMENT;
    return 0;
  }
  try {
    TypePlan reserved;
    reserved.kind = kind;
    g_types.push_back(std::move(reserved));
    g_decode_error = PC_WASM_OK;
    return static_cast<uint32_t>(g_types.size());
  } catch (const std::bad_alloc&) {
    g_decode_error = PC_WASM_OUT_OF_MEMORY;
    return 0;
  } catch (...) {
    g_decode_error = PC_WASM_INTERNAL_ERROR;
    return 0;
  }
}

PC_EXPORT uint32_t pc_decode_schema_count() noexcept {
  return static_cast<uint32_t>(g_types.size());
}

PC_EXPORT uint32_t pc_decode_schema_kind(uint32_t schema_handle) noexcept {
  if (schema_handle == 0 || schema_handle > g_types.size()) return kNone;
  const auto& schema = g_types[schema_handle - 1U];
  return schema.committed ? schema.kind : kNone;
}

PC_EXPORT uint32_t pc_decode_plan_commit(
    uint32_t schema_handle,
    uint32_t plan_length) noexcept {
  try {
    if (!CommitPlan(schema_handle, plan_length)) {
      g_decode_error = PC_WASM_INVALID_DATA;
      return PC_WASM_INVALID_DATA;
    }
    g_decode_error = PC_WASM_OK;
    return PC_WASM_OK;
  } catch (const std::bad_alloc&) {
    g_decode_error = PC_WASM_OUT_OF_MEMORY;
    return PC_WASM_OUT_OF_MEMORY;
  } catch (...) {
    g_decode_error = PC_WASM_INTERNAL_ERROR;
    return PC_WASM_INTERNAL_ERROR;
  }
}

PC_EXPORT uint32_t pc_decode_tape(
    uint32_t schema_handle,
    uint32_t input_length) noexcept {
  if (input_length == 0 || (input_length & 3U) != 0 ||
      input_length > g_input.size() || schema_handle == 0 ||
      schema_handle > g_types.size() ||
      !g_types[schema_handle - 1U].committed) {
    g_decode_error = PC_WASM_INVALID_ARGUMENT;
    return 0;
  }
  try {
    g_input_length = input_length;
    g_tape.clear();
    if (g_tape.capacity() < kInitialTapeWords) {
      g_tape.reserve(kInitialTapeWords);
    }
    const Location root{0, input_length / 4U};
    if (!Visit(schema_handle, root, 0)) {
      if (g_decode_error == PC_WASM_OK) {
        g_decode_error = PC_WASM_INVALID_DATA;
      }
      g_tape.clear();
      return 0;
    }
    if (g_tape.size() > std::numeric_limits<uint32_t>::max() / 4U) {
      g_decode_error = PC_WASM_OUT_OF_MEMORY;
      g_tape.clear();
      return 0;
    }
    g_decode_error = PC_WASM_OK;
    return static_cast<uint32_t>(g_tape.size() * 4U);
  } catch (const std::bad_alloc&) {
    g_decode_error = PC_WASM_OUT_OF_MEMORY;
    g_tape.clear();
    return 0;
  } catch (...) {
    g_decode_error = PC_WASM_INTERNAL_ERROR;
    g_tape.clear();
    return 0;
  }
}

PC_EXPORT pc_wasm_ptr_t pc_decode_tape_output() noexcept {
  return g_tape.empty() ? 0 : ToAddress(g_tape.data());
}

PC_EXPORT uint32_t pc_decode_last_error() noexcept {
  return g_decode_error;
}

}  // extern "C"
