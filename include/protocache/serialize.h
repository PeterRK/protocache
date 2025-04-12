// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_SERIALIZE_H_
#define PROTOCACHE_SERIALIZE_H_

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <type_traits>
#include "utils.h"
#include "access.h"
#include "perfect_hash.h"

namespace protocache {

struct Unit {
	unsigned len = 0;
	union {
		uint32_t data[3];
		Buffer::Seg seg = {0, 0};
	};
	unsigned size() const noexcept {
		if (len == 0) {
			return seg.len;
		}
		return len;
	}
};

template<typename T, typename std::enable_if<std::is_scalar<T>::value, bool>::type = true>
static inline bool Serialize(T v, Buffer& buf, Unit& unit) {
	unit.len = sizeof(T)/sizeof(uint32_t);
	*reinterpret_cast<T*>(unit.data) = v;
	return true;
}

static inline bool Serialize(bool v, Buffer& buf, Unit& unit) {
	unit.len = 1;
	unit.data[0] = v;
	return true;
}

extern bool Serialize(const Slice<char>& str, Buffer& buf, Unit& unit);

static inline bool Serialize(const Slice<uint8_t>& data, Buffer& buf, Unit& unit) {
	return Serialize(SliceCast<char>(data), buf, unit);
}
static inline bool Serialize(const Slice<bool>& data, Buffer& buf, Unit& unit) {
	return Serialize(SliceCast<char>(data), buf, unit);
}
static inline bool Serialize(const std::string& str, Buffer& buf, Unit& unit) {
	return Serialize(Slice<char>(str), buf, unit);
}

static inline Unit Segment(size_t last, size_t now) {
	Unit out;
	out.len = 0;
	out.seg.pos = now;
	out.seg.len = now - last;
	return out;
}

extern bool SerializeMessage(std::vector<Unit>& fields, Buffer& buf, size_t last, Unit& unit);
extern bool SerializeArray(std::vector<Unit>& elements, Buffer& buf, size_t last, Unit& unit);
extern bool SerializeMap(const Slice<uint8_t>& index, std::vector<Unit>& keys,
						 std::vector<Unit>& values, Buffer& buf, size_t last, Unit& unit);
} // protocache
#endif //PROTOCACHE_SERIALIZE_H_