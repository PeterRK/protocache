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
static inline Data SerializeScalar(T v) {
	static_assert(std::is_scalar<T>::value, "");
	Data out(WordSize(sizeof(T)), 0);
	*Cast<T>(out.data()) = v;
	return out;
}

static inline Data Serialize(bool v) {
	//return SerializeScalar(v);
	//FIXME: GCC fail with SerializeScalar
	Data out = {(uint32_t)v};
	return out;
}
static inline Data Serialize(int32_t v) { return SerializeScalar(v); }
static inline Data Serialize(uint32_t v) { return SerializeScalar(v); }
static inline Data Serialize(int64_t v) { return SerializeScalar(v); }
static inline Data Serialize(uint64_t v) { return SerializeScalar(v); }
static inline Data Serialize(float v) { return SerializeScalar(v); }
static inline Data Serialize(double v) { return SerializeScalar(v); }

template <typename T>
static inline Data SerializeScalarArray(const std::vector<T>& array) {
	static_assert(std::is_scalar<T>::value, "");
	auto m = WordSize(sizeof(T));
	Data out(1 + m*array.size(), 0);
	if (out.size() >= (1U << 30U)) {
		return {};
	}
	out[0] = (array.size() << 2U) | m;
	auto p = out.data() + 1;
	for (auto v : array) {
		*Cast<T>(p) = v;
		p += m;
	}
	return out;
}

extern Data Serialize(const Slice<char>& str);

static inline Data Serialize(const Slice<uint8_t>& data) {
	return Serialize(SliceCast<char>(data));
}

static inline Data Serialize(const Slice<bool>& data) {
	return Serialize(SliceCast<char>(data));
}

static inline Data Serialize(const std::string& str) {
	return Serialize(Slice<char>(str));
}

extern Data SerializeMessage(std::vector<Slice<uint32_t>>& parts);
extern Data SerializeMessage(std::vector<Data>& parts);
extern Data SerializeArray(const std::vector<Data>& elements);
extern Data SerializeMap(const Slice<uint8_t>& index, const std::vector<Data>& keys, const std::vector<Data>& values);

} // protocache
#endif //PROTOCACHE_SERIALIZE_H_