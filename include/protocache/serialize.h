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

template <typename T>
static inline T* Cast(const void* p) noexcept {
	return const_cast<T*>(reinterpret_cast<const T*>(p));
}

template <typename T>
static inline bool SerializeScalar(T v, Data* out) {
	static_assert(std::is_scalar<T>::value, "");
	out->clear();
	out->resize(WordSize(sizeof(T)), 0);
	*Cast<T>(out->data()) = v;
	return out;
}

static inline bool Serialize(bool v, Data* out) { return SerializeScalar(v, out); }
static inline bool Serialize(int32_t v, Data* out) { return SerializeScalar(v, out); }
static inline bool Serialize(uint32_t v, Data* out) { return SerializeScalar(v, out); }
static inline bool Serialize(int64_t v, Data* out) { return SerializeScalar(v, out); }
static inline bool Serialize(uint64_t v, Data* out) { return SerializeScalar(v, out); }
static inline bool Serialize(float v, Data* out) { return SerializeScalar(v, out); }
static inline bool Serialize(double v, Data* out) { return SerializeScalar(v, out); }

template <typename T>
static inline bool SerializeScalarArray(const std::vector<T>& array, Data* out) {
	static_assert(std::is_scalar<T>::value, "");
	auto m = WordSize(sizeof(T));
	if (m*array.size() >= (1U << 30U)) {
		return false;
	}
	out->clear();
	out->resize(1 + m*array.size(), 0);
	out->front() = (array.size() << 2U) | m;
	auto p = out->data() + 1;
	for (auto v : array) {
		*Cast<T>(p) = v;
		p += m;
	}
	return out;
}

extern bool Serialize(const Slice<char>& str, Data* out);

static inline bool Serialize(const Slice<uint8_t>& data, Data* out) {
	return Serialize(SliceCast<char>(data), out);
}

static inline bool Serialize(const Slice<bool>& data, Data* out) {
	return Serialize(SliceCast<char>(data), out);
}

static inline bool Serialize(const std::string& str, Data* out) {
	return Serialize(Slice<char>(str), out);
}

extern bool SerializeMessage(std::vector<Slice<uint32_t>>& parts, Data* out);
extern bool SerializeMessage(std::vector<Data>& parts, Data* out);
extern bool SerializeArray(const std::vector<Data>& elements, Data* out);
extern bool SerializeMap(const Slice<uint8_t>& index,
						 const std::vector<Data>& keys, const std::vector<Data>& values, Data* out);

} // protocache
#endif //PROTOCACHE_SERIALIZE_H_