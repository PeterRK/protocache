#include "protocache_wasm.h"

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "protocache/perfect_hash.h"
#include "protocache/serialize.h"

#if defined(__wasm32__)
#define PC_EXPORT __attribute__((visibility("default")))
#else
#define PC_EXPORT
#endif

namespace {

constexpr uint32_t kInitialArenaCapacity = 64U * 1024U;
constexpr uint32_t kMaximumArenaCapacity = 1U << 30U;
std::vector<uint8_t> g_arena(kInitialArenaCapacity);

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

uint32_t Load32(const uint8_t* pointer) noexcept {
  uint32_t value;
  std::memcpy(&value, pointer, sizeof(value));
  return value;
}

bool IsScalar(uint32_t kind) noexcept {
  return kind >= kF64 && kind <= kEnum;
}

uint32_t ScalarWidth(uint32_t kind) noexcept {
  switch (kind) {
    case kBool:
      return 1;
    case kF32:
    case kU32:
    case kI32:
    case kEnum:
      return 4;
    case kF64:
    case kU64:
    case kI64:
      return 8;
    default:
      return 0;
  }
}

struct NodeView final {
  const uint8_t* start = nullptr;
  const uint8_t* end = nullptr;
  uint32_t kind = kNone;
  uint32_t auxiliary = 0;
  uint32_t count = 0;
};

bool ParseNode(
    const uint8_t*& cursor,
    const uint8_t* limit,
    NodeView* out) noexcept {
  if (cursor > limit || static_cast<uint32_t>(limit - cursor) < 16U) {
    return false;
  }
  const auto* start = cursor;
  const auto kind = Load32(start);
  const auto auxiliary = Load32(start + 4);
  const auto count = Load32(start + 8);
  uint32_t size = 16;
  if (kind != kNone && !IsScalar(kind)) {
    size = Load32(start + 12);
    if (size < 16U || (size & 3U) != 0 ||
        static_cast<uint32_t>(limit - start) < size) {
      return false;
    }
  } else if (kind != kNone && ScalarWidth(kind) == 0) {
    return false;
  }
  cursor += size;
  *out = {start, start + size, kind, auxiliary, count};
  return true;
}

template <typename T>
T NodeScalar(const NodeView& node) noexcept {
  T value;
  std::memcpy(&value, node.start + 8, sizeof(value));
  return value;
}

protocache::Slice<uint8_t> KeyBytes(
    const NodeView& node,
    uint32_t expected_kind) noexcept {
  if (node.kind != expected_kind) return {};
  if (node.kind == kString) {
    if (node.count > static_cast<uint32_t>(node.end - node.start - 16)) {
      return {};
    }
    return {node.start + 16, node.count};
  }
  const auto width = ScalarWidth(node.kind);
  if (width != 4 && width != 8) return {};
  return {node.start + 8, width};
}

class ArenaKeyReader final : public protocache::KeyReader {
 public:
  explicit ArenaKeyReader(
      const std::vector<protocache::Slice<uint8_t>>& keys) noexcept
      : keys_(keys) {}

  void Reset() override { position_ = 0; }
  size_t Total() override { return keys_.size(); }
  protocache::Slice<uint8_t> Read() override {
    return position_ < keys_.size() ? keys_[position_++]
                                    : protocache::Slice<uint8_t>();
  }

 private:
  const std::vector<protocache::Slice<uint8_t>>& keys_;
  size_t position_ = 0;
};

bool SerializeNode(
    const NodeView& node,
    protocache::Buffer& buffer,
    protocache::Unit& unit);

bool SerializeScalarArray(
    const NodeView& node,
    protocache::Buffer& buffer,
    protocache::Unit& unit) {
  const auto width = ScalarWidth(node.auxiliary);
  if (width == 0) return false;
  const uint64_t byte_length = static_cast<uint64_t>(width) * node.count;
  const uint64_t padded_length = (byte_length + 3U) & ~uint64_t{3U};
  if (padded_length != static_cast<uint64_t>(node.end - node.start - 16)) {
    return false;
  }
  const auto* payload = node.start + 16;
  if (node.auxiliary == kBool) {
    return protocache::Serialize(
        protocache::Slice<bool>(
            reinterpret_cast<const bool*>(payload), node.count),
        buffer,
        unit);
  }
  const auto width_words = width / 4;
  if (node.count == 0) {
    unit.len = 1;
    unit.data[0] = width_words;
    return true;
  }
  const auto last = buffer.Size();
  for (uint32_t index = node.count; index-- > 0;) {
    auto* destination = buffer.Expand(width_words);
    std::memcpy(destination, payload + index * width, width);
  }
  buffer.Put((node.count << 2U) | width_words);
  unit = protocache::Segment(last, buffer.Size());
  return true;
}

bool SerializeArrayNode(
    const NodeView& node,
    protocache::Buffer& buffer,
    protocache::Unit& unit) {
  if (ScalarWidth(node.auxiliary) != 0) {
    return SerializeScalarArray(node, buffer, unit);
  }
  const auto* cursor = node.start + 16;
  std::vector<NodeView> elements(node.count);
  for (uint32_t index = 0; index < node.count; ++index) {
    if (!ParseNode(cursor, node.end, &elements[index])) return false;
  }
  if (cursor != node.end) return false;

  const auto last = buffer.Size();
  std::vector<protocache::Unit> units(node.count);
  for (uint32_t index = node.count; index-- > 0;) {
    if (!SerializeNode(elements[index], buffer, units[index])) return false;
  }
  return protocache::SerializeArray(units, buffer, last, unit);
}

bool SerializeMessageNode(
    const NodeView& node,
    protocache::Buffer& buffer,
    protocache::Unit& unit) {
  if (node.auxiliary == 0 || node.auxiliary > 12U + 25U * 255U) {
    return false;
  }
  const auto* cursor = node.start + 16;
  std::vector<std::pair<uint32_t, NodeView>> fields(node.count);
  std::vector<uint8_t> occupied(node.auxiliary);
  for (uint32_t index = 0; index < node.count; ++index) {
    if (cursor > node.end ||
        static_cast<uint32_t>(node.end - cursor) < 4U) {
      return false;
    }
    const auto field_id = Load32(cursor);
    cursor += 4;
    if (field_id >= node.auxiliary || occupied[field_id] != 0) return false;
    occupied[field_id] = 1;
    fields[index].first = field_id;
    if (!ParseNode(cursor, node.end, &fields[index].second)) return false;
  }
  if (cursor != node.end) return false;

  const auto last = buffer.Size();
  std::vector<protocache::Unit> units(node.auxiliary);
  for (uint32_t index = node.count; index-- > 0;) {
    const auto field_id = fields[index].first;
    auto& field_unit = units[field_id];
    if (!SerializeNode(fields[index].second, buffer, field_unit)) return false;
    protocache::FoldField(buffer, field_unit);
  }
  return protocache::SerializeMessage(units, buffer, last, unit);
}

bool SerializeMapNode(
    const NodeView& node,
    protocache::Buffer& buffer,
    protocache::Unit& unit) {
  const auto key_kind = node.auxiliary & 0xffU;
  const auto value_kind = (node.auxiliary >> 8U) & 0xffU;
  const auto* cursor = node.start + 16;
  struct Entry final {
    NodeView key;
    NodeView value;
  };
  std::vector<Entry> entries(node.count);
  std::vector<protocache::Slice<uint8_t>> keys(node.count);
  for (uint32_t index = 0; index < node.count; ++index) {
    if (!ParseNode(cursor, node.end, &entries[index].key) ||
        !ParseNode(cursor, node.end, &entries[index].value) ||
        entries[index].value.kind != value_kind) {
      return false;
    }
    keys[index] = KeyBytes(entries[index].key, key_kind);
    if (!keys[index]) return false;
  }
  if (cursor != node.end) return false;
  if (entries.empty()) {
    std::vector<std::pair<protocache::Unit, protocache::Unit>> units;
    return protocache::SerializeMap({}, units, buffer, buffer.Size(), unit);
  }

  ArenaKeyReader reader(keys);
  auto perfect_hash = protocache::PerfectHashObject::Build(reader, true);
  for (uint32_t attempt = 1; attempt < 8 && !perfect_hash; ++attempt) {
    perfect_hash = protocache::PerfectHashObject::Build(reader, true);
  }
  if (!perfect_hash) return false;

  std::vector<uint32_t> slot_to_input(node.count);
  std::vector<uint8_t> occupied(node.count);
  for (uint32_t input = 0; input < node.count; ++input) {
    const auto slot = perfect_hash.Locate(keys[input].data(), keys[input].size());
    if (slot >= node.count || occupied[slot] != 0) return false;
    occupied[slot] = 1;
    slot_to_input[slot] = input;
  }

  const auto last = buffer.Size();
  std::vector<std::pair<protocache::Unit, protocache::Unit>> units(node.count);
  for (uint32_t slot = node.count; slot-- > 0;) {
    const auto input = slot_to_input[slot];
    if (!SerializeNode(entries[input].value, buffer, units[slot].second) ||
        !SerializeNode(entries[input].key, buffer, units[slot].first)) {
      return false;
    }
  }
  return protocache::SerializeMap(
      perfect_hash.Data(), units, buffer, last, unit);
}

bool SerializeNode(
    const NodeView& node,
    protocache::Buffer& buffer,
    protocache::Unit& unit) {
  switch (node.kind) {
    case kNone:
      unit = {};
      return true;
    case kMessage:
      return SerializeMessageNode(node, buffer, unit);
    case kArray:
      return SerializeArrayNode(node, buffer, unit);
    case kMap:
      return SerializeMapNode(node, buffer, unit);
    case kBytes:
      if (((static_cast<uint64_t>(node.count) + 3U) & ~uint64_t{3U}) !=
          static_cast<uint32_t>(node.end - node.start - 16)) {
        return false;
      }
      return protocache::Serialize(
          protocache::Slice<uint8_t>(node.start + 16, node.count),
          buffer,
          unit);
    case kString:
      if (((static_cast<uint64_t>(node.count) + 3U) & ~uint64_t{3U}) !=
          static_cast<uint32_t>(node.end - node.start - 16)) {
        return false;
      }
      return protocache::Serialize(
          protocache::Slice<char>(
              reinterpret_cast<const char*>(node.start + 16), node.count),
          buffer,
          unit);
    case kF64:
      return protocache::Serialize(NodeScalar<double>(node), buffer, unit);
    case kF32:
      return protocache::Serialize(NodeScalar<float>(node), buffer, unit);
    case kU64:
      return protocache::Serialize(NodeScalar<uint64_t>(node), buffer, unit);
    case kU32:
      return protocache::Serialize(NodeScalar<uint32_t>(node), buffer, unit);
    case kI64:
      return protocache::Serialize(NodeScalar<int64_t>(node), buffer, unit);
    case kI32:
    case kEnum:
      return protocache::Serialize(NodeScalar<int32_t>(node), buffer, unit);
    case kBool:
      return protocache::Serialize(
          NodeScalar<uint32_t>(node) != 0, buffer, unit);
    default:
      return false;
  }
}

protocache::Buffer g_output;

}  // namespace

extern "C" {

PC_EXPORT pc_wasm_ptr_t pc_arena_buffer() noexcept {
  return static_cast<pc_wasm_ptr_t>(
      reinterpret_cast<uintptr_t>(g_arena.data()));
}

PC_EXPORT uint32_t pc_arena_capacity() noexcept {
  return static_cast<uint32_t>(g_arena.size());
}

PC_EXPORT pc_wasm_ptr_t pc_arena_reserve(
    uint32_t minimum_capacity) noexcept {
  if (minimum_capacity == 0 || minimum_capacity >= kMaximumArenaCapacity) {
    return 0;
  }
  if (minimum_capacity <= g_arena.size()) return pc_arena_buffer();
  uint32_t capacity = static_cast<uint32_t>(g_arena.size());
  while (capacity < minimum_capacity) {
    if (capacity >= kMaximumArenaCapacity / 2U) {
      capacity = minimum_capacity;
      break;
    }
    capacity *= 2U;
  }
  try {
    g_arena.resize(capacity);
  } catch (...) {
    return 0;
  }
  return pc_arena_buffer();
}

PC_EXPORT uint32_t pc_arena_serialize(
    uint32_t schema_handle,
    pc_wasm_ptr_t pointer,
    uint32_t length) noexcept {
  const auto arena = pc_arena_buffer();
  if (pointer != arena || length == 0 || length > g_arena.size()) return 0;
  const auto* cursor = g_arena.data();
  NodeView root;
  if (!ParseNode(cursor, g_arena.data() + length, &root) ||
      cursor != g_arena.data() + length ||
      root.kind != pc_decode_schema_kind(schema_handle)) {
    return 0;
  }
  try {
    g_output.Clear();
    protocache::Unit unit;
    if (!SerializeNode(root, g_output, unit)) return 0;
    if (unit.len != 0) {
      if (g_output.Size() != 0) return 0;
      g_output.Put(protocache::Slice<uint32_t>(unit.data, unit.len));
    }
    const auto view = g_output.View();
    if (view.size() > UINT32_MAX / 4U) return 0;
    return static_cast<uint32_t>(view.size() * 4U);
  } catch (...) {
    return 0;
  }
}

PC_EXPORT pc_wasm_ptr_t pc_arena_output() noexcept {
  return static_cast<pc_wasm_ptr_t>(
      reinterpret_cast<uintptr_t>(g_output.View().data()));
}

}  // extern "C"
