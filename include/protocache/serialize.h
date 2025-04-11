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

template<typename T, typename std::enable_if<std::is_scalar<T>::value, bool>::type = true>
static inline bool Serialize(T v, Buffer* buf) {
	buf->Put(v);
	return true;
}

extern bool Serialize(const Slice<char>& str, Buffer* buf);

static inline bool Serialize(const Slice<uint8_t>& data, Buffer* buf) {
	return Serialize(SliceCast<char>(data), buf);
}
static inline bool Serialize(const Slice<bool>& data, Buffer* buf) {
	return Serialize(SliceCast<char>(data), buf);
}
static inline bool Serialize(const std::string& str, Buffer* buf) {
	return Serialize(Slice<char>(str), buf);
}

struct Part {
	unsigned len;
	union {
		uint32_t data[3];
		Buffer::Seg seg;
	};
};

static inline Buffer::Seg Segment(size_t last, size_t now) {
	return {now, now - last};
}

extern bool SerializeMessage(const std::vector<Buffer::Seg>& fields, Buffer& buf, size_t end);
extern bool SerializeArray(const std::vector<Buffer::Seg>& elements, Buffer& buf, size_t end);
extern bool SerializeMap(const Slice<uint8_t>& index, const std::vector<Buffer::Seg>& keys,
						 const std::vector<Buffer::Seg>& values, Buffer& buf, size_t end);
} // protocache
#endif //PROTOCACHE_SERIALIZE_H_