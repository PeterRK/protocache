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
#include <array>
#include <utility>
#include <type_traits>
#include "utils.h"
#include "access.h"
#include "perfect_hash.h"

namespace protocache {

template <typename T>
class Span final {
public:
	Span(T* data, size_t size) : data_(data), size_(size) {}
	explicit Span(std::vector<T>& src) : Span(src.data(), src.size()) {}

	template <size_t N>
	explicit Span(std::array<T,N>& src) : Span(src.data(), N) {}

	bool empty() const noexcept { return size_ == 0; }
	size_t size() const noexcept { return size_; }
	T& front() noexcept { return *data_; }
	T& back() noexcept { return *(data_ + size_-1); }
	void pop_back() noexcept { size_--; }
	T* begin() const noexcept { return data_; }
	T* end() const noexcept { return data_ + size_; }
	T& operator[](size_t i) noexcept { return data_[i]; }

private:
	T* data_ = nullptr;
	size_t size_ = 0;
};

struct Unit {
	struct Seg {
		uint32_t pos;
		uint32_t len;
		uint32_t end() const noexcept { return pos-len; }
	};

	unsigned len = 0;
	union {
		uint32_t data[3];
		Seg seg = {0, 0};
	};
	unsigned size() const noexcept {
		if (len == 0) {
			return seg.len;
		}
		return len;
	}
};

static inline Unit Segment(size_t last, size_t now) {
	Unit out;
	out.len = 0;
	out.seg.pos = now;
	out.seg.len = now - last;
	return out;
}

static void FoldField(Buffer& buf, Unit& unit) {
	if (unit.len == 0 && unit.seg.len != 0 && unit.seg.len < 4) {
		unit.len = unit.seg.len;
		assert(unit.seg.pos == buf.Size());
		auto src = buf.Head();
		for (unsigned i = 0; i < unit.len; i++) {
			unit.data[i] = src[i];
		}
		buf.Shrink(unit.len);
	}
}

template<typename T>
static inline bool Serialize(T v, Buffer& buf, Unit& unit) {
	static_assert(std::is_scalar_v<T>);
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

extern bool SerializeMessage(Span<Unit> fields, Buffer& buf, size_t last, Unit& unit);
extern bool SerializeArray(std::vector<Unit>& elements, Buffer& buf, size_t last, Unit& unit);
extern bool SerializeMap(const Slice<uint8_t>& index, std::vector<std::pair<Unit,Unit>>& pairs,
						 Buffer& buf, size_t last, Unit& unit);

static inline bool SerializeMessage(std::vector<Unit>& fields, Buffer& buf, size_t last, Unit& unit) {
	return SerializeMessage(Span(fields), buf, last, unit);
}

template <size_t N>
static inline bool SerializeMessage(std::array<Unit,N>& fields, Buffer& buf, size_t last, Unit& unit) {
	return SerializeMessage(Span(fields), buf, last, unit);
}

} // protocache
#endif //PROTOCACHE_SERIALIZE_H_
