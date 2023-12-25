// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_ACCESS_H_
#define PROTOCACHE_ACCESS_H_

#include <cstdint>
#include <string>
#include "perfect_hash.h"

namespace protocache {

extern uint32_t CalcMagic(const char* name, unsigned len) noexcept;

static inline uint32_t CalcMagic(const std::string& name) noexcept {
	return CalcMagic(name.data(), name.size());
}

class String final {
public:
	String() noexcept = default;
	explicit String(const uint32_t* ptr) noexcept : ptr_(reinterpret_cast<const uint8_t*>(ptr)) {
		if (ptr_ == nullptr) {
			return;
		}
		if ((*ptr_ & 3) != 0) {
			ptr_ = nullptr;
		}
	};
	bool operator!() const noexcept {
		return ptr_ == nullptr;
	}

	Slice<uint8_t> Get(const uint32_t* end=nullptr) const noexcept;

private:
	const uint8_t* ptr_ = nullptr;
};

class Field final {
public:
	Field() noexcept = default;
	explicit Field(const uint32_t* ptr, unsigned width) noexcept : ptr_(ptr), width_(width) {};
	bool operator!() const noexcept {
		return ptr_ == nullptr;
	}

	Slice<uint32_t> GetValue(const uint32_t* end=nullptr) const noexcept {
		if (end != nullptr && ptr_ + width_ > end) {
			return {};
		}
		return {ptr_, width_};
	}

	const uint32_t* GetObject(const uint32_t* end=nullptr) const noexcept {
		if (end != nullptr && ptr_ >= end) {
			return {};
		}
		if (ptr_ == nullptr) {
			return nullptr;
		}
		auto ptr = ptr_;
		if ((*ptr & 3) == 3) {
			ptr += *ptr >> 2U;
		}
		if (end != nullptr && ptr >= end) {
			return nullptr;
		}
		return ptr;
	}

private:
	const uint32_t* ptr_ = nullptr;
	unsigned width_ = 0;
};

class Message final {
public:
	Message() noexcept = default;
	explicit Message(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept : ptr_(ptr) {};
	bool operator!() const noexcept {
		return ptr_ == nullptr;
	}

	Field GetField(unsigned id, const uint32_t* end=nullptr) const noexcept;

private:
	const uint32_t* ptr_ = nullptr;
};

class Array final {
public:
	Array() noexcept = default;
	explicit Array(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept;
	bool operator!() const noexcept {
		return ptr_ == nullptr;
	}

	uint32_t Size() const noexcept {
		return size_;
	}

	class Iterator final {
	public:
		Iterator() = default;
		bool operator==(const Iterator& other) const noexcept {
			return ptr_ == other.ptr_;
		}
		bool operator!=(const Iterator& other) const noexcept {
			return ptr_ != other.ptr_;
		}
		Field operator*() const noexcept {
			return Field(ptr_, width_);
		}
		Iterator& operator+=(ptrdiff_t step) noexcept {
			ptr_ += static_cast<ptrdiff_t>(width_) * step;
			return *this;
		}
		Iterator& operator-=(ptrdiff_t step) noexcept {
			ptr_ -= static_cast<ptrdiff_t>(width_) * step;
			return *this;
		}
		Iterator operator+(ptrdiff_t step) noexcept {
			return {ptr_ + static_cast<ptrdiff_t>(width_) * step, width_};
		}
		Iterator operator-(ptrdiff_t step) noexcept {
			return {ptr_ - static_cast<ptrdiff_t>(width_) * step, width_};
		}
		Iterator& operator++() noexcept {
			ptr_ += width_;
			return *this;
		}
		Iterator& operator--() noexcept {
			ptr_ -= width_;
			return *this;
		}
		Iterator operator++(int) noexcept {
			auto old = *this;
			ptr_ += width_;
			return old;
		}
		Iterator operator--(int) noexcept {
			auto old = *this;
			ptr_ -= width_;
			return old;
		}

	private:
		const uint32_t* ptr_ = nullptr;
		unsigned width_ = 0;

		Iterator(const uint32_t* ptr, uint32_t width) : ptr_(ptr), width_(width) {}
		friend class Array;
	};

	Iterator begin() const noexcept {
		return {ptr_ + 1, width_};
	}
	Iterator end() const noexcept {
		return {ptr_ + 1 + width_ * size_, width_};
	}

	Field operator[](unsigned pos) const noexcept {
		return Field(ptr_ + 1 + width_ * pos, width_);
	}

	template<typename T>
	Slice<T> AsScalarV() const noexcept {
		static_assert(std::is_scalar<T>::value && (sizeof(T) == 4 || sizeof(T) == 8), "");
		if (ptr_ == nullptr || width_ != sizeof(T)/4) {
			return {};
		}
		return {reinterpret_cast<const T*>(ptr_+1), size_};
	}

private:
	const uint32_t* ptr_ = nullptr;
	uint32_t size_ = 0;
	unsigned width_ = 0;
};

class Pair final {
public:
	Pair() noexcept = default;
	Pair(const uint32_t* ptr, unsigned key_width, unsigned value_width) noexcept
		: ptr_(ptr), key_width_(key_width), value_width_(value_width) {};
	bool operator!() const noexcept {
		return ptr_ == nullptr;
	}

	Field Key() const noexcept {
		return Field(ptr_, key_width_);
	}
	Field Value() const noexcept {
		return Field(ptr_ + key_width_, value_width_);
	}

private:
	const uint32_t* ptr_ = nullptr;
	unsigned key_width_ = 0;
	unsigned value_width_ = 0;
};

class Map final {
public:
	Map() noexcept = default;
	explicit Map(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept;
	bool operator!() const noexcept {
		return body_ == nullptr;
	}

	uint32_t Size() const noexcept {
		return index_.Size();
	}

	class Iterator final {
	public:
		Iterator() = default;
		bool operator==(const Iterator& other) const noexcept {
			return ptr_ == other.ptr_;
		}
		bool operator!=(const Iterator& other) const noexcept {
			return ptr_ != other.ptr_;
		}
		Pair operator*() const noexcept {
			return Pair(ptr_, key_width_, value_width_);
		}
		Iterator& operator+=(ptrdiff_t step) noexcept {
			ptr_ += static_cast<ptrdiff_t>(key_width_ + value_width_) * step;
			return *this;
		}
		Iterator& operator-=(ptrdiff_t step) noexcept {
			ptr_ -= static_cast<ptrdiff_t>(key_width_ + value_width_) * step;
			return *this;
		}
		Iterator operator+(ptrdiff_t step) noexcept {
			return {ptr_ + static_cast<ptrdiff_t>(key_width_ + value_width_) * step, key_width_, value_width_};
		}
		Iterator operator-(ptrdiff_t step) noexcept {
			return {ptr_ - static_cast<ptrdiff_t>(key_width_ + value_width_) * step, key_width_, value_width_};
		}
		Iterator& operator++() noexcept {
			ptr_ += key_width_ + value_width_;
			return *this;
		}
		Iterator& operator--() noexcept {
			ptr_ -= key_width_ + value_width_;
			return *this;
		}
		Iterator operator++(int) noexcept {
			auto old = *this;
			ptr_ += key_width_ + value_width_;
			return old;
		}
		Iterator operator--(int) noexcept {
			auto old = *this;
			ptr_ -= key_width_ + value_width_;
			return old;
		}

	private:
		const uint32_t* ptr_ = nullptr;
		unsigned key_width_ = 0;
		unsigned value_width_ = 0;

		Iterator(const uint32_t* ptr, unsigned key_width, unsigned value_width) noexcept
				: ptr_(ptr), key_width_(key_width), value_width_(value_width) {};
		friend class Map;
	};

	Iterator begin() const noexcept {
		return {body_, key_width_, value_width_};
	}
	Iterator end() const noexcept {
		return {body_ + (key_width_ + value_width_) * Size(), key_width_, value_width_};
	}

	Iterator Find(const char* str, unsigned len, const uint32_t* end=nullptr) const noexcept;
	Iterator Find(const Slice<char>& key, const uint32_t* end=nullptr) {
		return Find(key.data(), key.size(), end);
	}
	Iterator Find(const std::string& key, const uint32_t* end=nullptr) {
		return Find(key.data(), key.size(), end);
	}

	Iterator Find(uint64_t key, const uint32_t* end=nullptr) {
		return Find(reinterpret_cast<const uint32_t *>(&key), 2, end);
	}
	Iterator Find(int64_t key, const uint32_t* end=nullptr) {
		return Find(reinterpret_cast<const uint32_t *>(&key), 2, end);
	}
	Iterator Find(uint32_t key, const uint32_t* end=nullptr) {
		return Find(&key, 1, end);
	}
	Iterator Find(int32_t key, const uint32_t* end=nullptr) {
		return Find(reinterpret_cast<const uint32_t *>(&key), 1, end);
	}

private:
	PerfectHash index_;
	const uint32_t* body_ = nullptr;
	unsigned key_width_ = 0;
	unsigned value_width_ = 0;

	Iterator Find(const uint32_t* val, unsigned len, const uint32_t* end= nullptr) const noexcept;
};

static inline size_t WordSize(size_t sz) noexcept {
	return (sz+3)/4;
}

static inline int32_t GetInt32(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 1) {
		return 0;
	}
	return *reinterpret_cast<const int32_t*>(view.data());
}

static inline uint32_t GetUInt32(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 1) {
		return 0;
	}
	return *view.data();
}

static inline int64_t GetInt64(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 2) {
		return 0;
	}
	return *reinterpret_cast<const int64_t*>(view.data());
}

static inline uint64_t GetUInt64(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 2) {
		return 0;
	}
	return *reinterpret_cast<const uint64_t*>(view.data());
}

static inline bool GetBool(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 1) {
		return false;
	}
	return *reinterpret_cast<const bool*>(view.data());
}

static inline int GetEnumValue(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 1) {
		return 0;
	}
	return *reinterpret_cast<const int*>(view.data());
}

static inline float GetFloat(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 1) {
		return false;
	}
	return *reinterpret_cast<const float*>(view.data());
}

static inline double GetDouble(const Field& field, const uint32_t* end=nullptr) noexcept {
	auto view = field.GetValue();
	if (!view || view.size() != 2) {
		return false;
	}
	return *reinterpret_cast<const double*>(view.data());
}

static inline Slice<uint8_t> GetBytes(const Field& field, const uint32_t* end=nullptr) noexcept {
	String view(field.GetObject(end));
	if (!view) {
		return {};
	}
	return view.Get(end);
}

static inline Slice<char> GetString(const Field& field, const uint32_t* end=nullptr) noexcept {
	return GetBytes(field, end).cast<char>();
}

static inline Slice<bool> GetBoolArray(const Field& field, const uint32_t* end=nullptr) noexcept {
	return GetBytes(field, end).cast<bool>();
}

static inline Array GetArray(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Array(field.GetObject(end), end);
}

static inline Slice<int32_t> GetInt32Array(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Array(field.GetObject(end), end).AsScalarV<int32_t>();
}

static inline Slice<uint32_t> GetUInt32Array(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Array(field.GetObject(end), end).AsScalarV<uint32_t>();
}

static inline Slice<int64_t> GetInt64Array(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Array(field.GetObject(end), end).AsScalarV<int64_t>();
}

static inline Slice<uint64_t> GetUInt64Array(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Array(field.GetObject(end), end).AsScalarV<uint64_t>();
}

static inline Slice<float> GetFloatArray(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Array(field.GetObject(end), end).AsScalarV<float>();
}

static inline Slice<double> GetDoubleArray(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Array(field.GetObject(end), end).AsScalarV<double>();
}

static inline Map GetMap(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Map(field.GetObject(end), end);
}

static inline Message GetMessage(const Field& field, const uint32_t* end=nullptr) noexcept {
	return Message(field.GetObject(end), end);
}

} // protocache
#endif //PROTOCACHE_ACCESS_H_