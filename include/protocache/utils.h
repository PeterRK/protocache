// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_UTILS_H_
#define PROTOCACHE_UTILS_H_

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <memory>

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

static inline void Compress(const std::string& src, std::string* out) {
	Compress(reinterpret_cast<const uint8_t*>(src.data()), src.size(), out);
}
static inline bool Decompress(const std::string& src, std::string* out) {
	return Decompress(reinterpret_cast<const uint8_t*>(src.data()), src.size(), out);
}

class Buffer final {
public:
	Buffer() = default;
	Buffer(const Buffer&) = delete;
	Buffer(Buffer &&other) noexcept:
		data_(std::move(other.data_)), size_(other.size_), off_(other.off_) {
		other.size_ = 0;
		other.off_ = 0;
	}
	Buffer& operator=(const Buffer&) = delete;
	Buffer& operator=(Buffer&& other) noexcept {
		if (&other != this) {
			this->~Buffer();
			new(this) Buffer(std::move(other));
		}
		return *this;
	}

	Slice<uint32_t> View() const noexcept {
		return {data_.get()+off_, Size()};
	};
	size_t Size() const noexcept {
		return size_ - off_;
	}
	uint32_t* At(size_t size) noexcept {
		return data_.get() + size_ - size;
	}
	uint32_t* Head() const noexcept {
		return data_.get() + off_;
	}
	void Clear() noexcept {
		off_ = size_;
	}

	void Put(uint32_t v) noexcept {
		*Expand(1) = v;
	}
	void Put(const Slice<uint32_t>& data) noexcept {
		auto dest = Expand(data.size());
		for (size_t i = 0; i < data.size(); i++) {
			dest[i] = data[i];
		}
	}

	uint32_t* Expand(size_t delta) {
		if (off_ >= delta) {
			off_ -= delta;
		} else {
			DoExpand(delta);
		}
		return Head();
	}

	void Shrink(size_t delta) noexcept {
		assert(delta <= Size());
		off_ += delta;
	}

	void Reserve(size_t size) {
		if (size > size_) {
			DoReserve(size);
		}
	}

private:
	std::unique_ptr<uint32_t[]> data_;
	size_t size_ = 0;
	size_t off_ = 0;

	void DoExpand(size_t delta);
	void DoReserve(size_t size);
};

} // protocache
#endif //PROTOCACHE_UTILS_H_