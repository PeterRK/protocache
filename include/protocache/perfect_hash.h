// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_PERFECT_HASH_H_
#define PROTOCACHE_PERFECT_HASH_H_

#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <vector>

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

struct KeyReader {
	virtual void Reset() = 0;
	virtual size_t Total() = 0;
	virtual Slice<uint8_t> Read() = 0;
	virtual ~KeyReader() noexcept = default;
};

class PerfectHash {
public:
	bool operator!() const noexcept {
		return data_ == nullptr;
	}
	PerfectHash() noexcept = default;
	PerfectHash(const PerfectHash& other) : section_(other.section_), data_size_(other.data_size_) {
		if (buffer_ == nullptr) {
			data_ = other.data_;
		} else {
			buffer_.reset(new uint8_t[data_size_]);
			memcpy(buffer_.get(), other.buffer_.get(), data_size_);
			data_ = buffer_.get();
		}
	}
	PerfectHash(PerfectHash&& other) noexcept :
			section_(other.section_), data_size_(other.data_size_),
			data_(other.data_), buffer_(std::move(other.buffer_)) {
		other.section_ = 0;
		other.data_size_ = 0;
		other.data_ = nullptr;
	}

	PerfectHash& operator=(const PerfectHash& other) {
		if (&other != this) {
			this->~PerfectHash();
			new(this)PerfectHash(other);
		}
		return *this;
	}
	PerfectHash& operator=(PerfectHash&& other) noexcept {
		if (&other != this) {
			this->~PerfectHash();
			new(this)PerfectHash(std::move(other));
		}
		return *this;
	}
	PerfectHash(const uint8_t* data, uint32_t size=0) noexcept;

	Slice<uint8_t> Data() const noexcept {
		return {data_, data_size_};
	}

	uint32_t Size() const noexcept;
	uint32_t Locate(const uint8_t* key, unsigned key_len) const noexcept;

	static PerfectHash Build(KeyReader& source, bool no_check=false);

private:
	uint32_t section_ = 0;
	uint32_t data_size_ = 0;
	const uint8_t* data_ = nullptr;
	std::unique_ptr<uint8_t[]> buffer_;
};

} //protocache
#endif //PROTOCACHE_PERFECT_HASH_H_
