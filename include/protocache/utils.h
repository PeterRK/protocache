// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_UTILS_H_
#define PROTOCACHE_UTILS_H_

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>

namespace protocache {

template <typename T>
class Slice {
public:
	Slice() noexcept = default;
	Slice(const T* p, size_t l) noexcept : ptr_(p), len_(l) {}

	explicit Slice(const std::basic_string<T>& v) noexcept : ptr_(v.data()), len_(v.size()) {}
	explicit Slice(const std::vector<T>& v) noexcept : ptr_(v.data()), len_(v.size()) {}

	bool operator!() const noexcept {
		return ptr_ == nullptr;
	}
	const T& operator[](unsigned pos) const noexcept {
		return ptr_[pos];
	}
	const T* begin() const noexcept {
		return ptr_;
	}
	const T* end() const noexcept {
		return ptr_ + len_;
	}
	const T* data() const noexcept {
		return ptr_;
	}
	size_t size() const noexcept {
		return len_;
	}
	bool empty() const noexcept {
		return len_ == 0;
	}

private:
	const T* ptr_ = nullptr;
	size_t len_ = 0;
};

static inline bool operator==(const Slice<char>& a, const Slice<char>& b) noexcept {
	return a.size() == b.size() && (a.data() == b.data() ||
									memcmp(a.data(), b.data(), a.size()) == 0);
}

static inline bool operator!=(const Slice<char>& a, const Slice<char>& b) noexcept {
	return !(a == b);
}

static inline bool operator==(const Slice<char>& a, const std::string& b) noexcept {
	return a == Slice<char>(b);
}

static inline bool operator!=(const Slice<char>& a, const std::string& b) noexcept {
	return a != Slice<char>(b);
}

static inline bool operator==(const std::string& a, const Slice<char>& b) noexcept {
	return b == a;
}

static inline bool operator!=(const std::string& a, const Slice<char>& b) noexcept {
	return b != a;
}

using Data = std::basic_string<uint32_t>;

static inline constexpr size_t WordSize(size_t sz) noexcept {
	return (sz+3)/4;
}

template<typename D, typename S>
static inline constexpr Slice<D> SliceCast(const Slice<S>& src) noexcept {
	static_assert(std::is_scalar<D>::value && std::is_scalar<S>::value && sizeof(D) == sizeof(S), "");
	return {reinterpret_cast<const D*>(src.data()), src.size()};
}

extern bool LoadFile(const std::string& path, std::string* out);

extern void Compress(const uint8_t* src, size_t len, std::string* out);
extern bool Decompress(const uint8_t* src, size_t len, std::string* out);

static inline void Compress(const Slice<uint8_t>& src, std::string* out) {
	Compress(src.data(), src.size(), out);
}
static inline void Compress(const Slice<char>& src, std::string* out) {
	Compress(SliceCast<uint8_t>(src), out);
}
static inline void Compress(const std::string& src, std::string* out) {
	Compress(Slice<char>(src), out);
}
static inline bool Decompress(const Slice<uint8_t>& src, std::string* out) {
	return Decompress(src.data(), src.size(), out);
}
static inline bool Decompress(const Slice<char>& src, std::string* out) {
	return Decompress(SliceCast<uint8_t>(src), out);
}
static inline bool Decompress(const std::string& src, std::string* out) {
	return Decompress(Slice<char>(src), out);
}

} // protocache
#endif //PROTOCACHE_UTILS_H_