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

static inline constexpr size_t WordSize(size_t sz) noexcept {
	return (sz+3)/4;
}

using EnumValue = int32_t;

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
		static_assert(std::is_scalar<T>::value, "");
		if (ptr_ == nullptr || width_ != WordSize(sizeof(T))) {
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

	Iterator Find(const Slice<char>& key, const uint32_t* end=nullptr) const noexcept {
		return Find(key.data(), key.size(), end);
	}
	Iterator Find(const std::string& key, const uint32_t* end=nullptr) const noexcept {
		return Find(key.data(), key.size(), end);
	}

	Iterator Find(uint64_t key, const uint32_t* end=nullptr) const noexcept {
		return Find(reinterpret_cast<const uint32_t *>(&key), 2, end);
	}
	Iterator Find(int64_t key, const uint32_t* end=nullptr) const noexcept {
		return Find(reinterpret_cast<const uint32_t *>(&key), 2, end);
	}
	Iterator Find(uint32_t key, const uint32_t* end=nullptr) const noexcept {
		return Find(&key, 1, end);
	}
	Iterator Find(int32_t key, const uint32_t* end=nullptr) const noexcept {
		return Find(reinterpret_cast<const uint32_t *>(&key), 1, end);
	}

private:
	PerfectHash index_;
	const uint32_t* body_ = nullptr;
	unsigned key_width_ = 0;
	unsigned value_width_ = 0;

	Iterator Find(const uint32_t* val, unsigned len, const uint32_t* end=nullptr) const noexcept;
};

template <typename T>
class FieldT final {
public:
	explicit FieldT(const Field& field) : core_(field) {}
	bool operator!() const noexcept {
		return !core_;
	}

	T Get(const uint32_t* end=nullptr) const noexcept;

private:
	Field core_;

	T GetScalar(const uint32_t* end=nullptr) const noexcept {
		auto view = core_.GetValue();
		if (!view || view.size() != WordSize(sizeof(T))) {
			return 0;
		}
		return *reinterpret_cast<const T*>(view.data());
	}

	Slice<uint8_t> GetBytes(const uint32_t* end=nullptr) const noexcept {
		String view(core_.GetObject(end));
		if (!view) {
			return {};
		}
		return view.Get(end);
	}
};

template <>
inline uint64_t FieldT<uint64_t>::Get(const uint32_t* end) const noexcept {
	return GetScalar(end);
}

template <>
inline int64_t FieldT<int64_t>::Get(const uint32_t* end) const noexcept {
	return GetScalar(end);
}

template <>
inline uint32_t FieldT<uint32_t>::Get(const uint32_t* end) const noexcept {
	return GetScalar(end);
}

template <>
inline int32_t FieldT<int32_t>::Get(const uint32_t* end) const noexcept {
	return GetScalar(end);
}

template <>
inline bool FieldT<bool>::Get(const uint32_t* end) const noexcept {
	return GetScalar(end);
}

template <>
inline float FieldT<float>::Get(const uint32_t* end) const noexcept {
	return GetScalar(end);
}

template <>
inline double FieldT<double>::Get(const uint32_t* end) const noexcept {
	return GetScalar(end);
}

template <>
inline Slice<uint8_t> FieldT<Slice<uint8_t>>::Get(const uint32_t* end) const noexcept {
	return GetBytes(end);
}

template <>
inline Slice<char> FieldT<Slice<char>>::Get(const uint32_t* end) const noexcept {
	return GetBytes(end).cast<char>();
}

template <>
inline Slice<bool> FieldT<Slice<bool>>::Get(const uint32_t* end) const noexcept {
	return GetBytes(end).cast<bool>();
}

template <>
inline Message FieldT<Message>::Get(const uint32_t* end) const noexcept {
	return Message(core_.GetObject(end));
}

template <>
inline Array FieldT<Array>::Get(const uint32_t* end) const noexcept {
	return Array(core_.GetObject(end), end);
}

template <>
inline Map FieldT<Map>::Get(const uint32_t* end) const noexcept {
	return Map(core_.GetObject(end), end);
}

template <typename T>
class ArrayT final {
public:
	explicit ArrayT(const Array& array) : core_(array) {}
	bool operator!() const noexcept {
		return !core_;
	}

	uint32_t Size() const noexcept {
		return core_.Size();
  	}

	class Iterator final {
	public:
		explicit Iterator(const Array::Iterator& core) : core_(core) {}
		bool operator==(const Iterator& other) const noexcept {
			return core_ == other.core_;
		}
	  	bool operator!=(const Iterator& other) const noexcept {
			return core_ != other.core_;
	  	}
		Iterator& operator+=(ptrdiff_t step) noexcept {
			core_ += step;
			return *this;
		}
		Iterator& operator-=(ptrdiff_t step) noexcept {
		  	core_ -= step;
			return *this;
		}
		Iterator operator+(ptrdiff_t step) noexcept {
			return Iterator(core_ + step);
		}
		Iterator operator-(ptrdiff_t step) noexcept {
			return Iterator(core_ - step);
		}
		Iterator& operator++() noexcept {
			++core_;
			return *this;
		}
		Iterator& operator--() noexcept {
			--core_;
			return *this;
		}
		Iterator operator++(int) noexcept {
			return Iterator(core_++);
		}
		Iterator operator--(int) noexcept {
			return Iterator(core_--);
		}

		FieldT<T> operator*() const noexcept {
		  	return FieldT<T>(*core_);
		}

	private:
		Array::Iterator core_;
	};

	Iterator begin() const noexcept {
		return Iterator(core_.begin());
  	}
	Iterator end() const noexcept {
		return Iterator(core_.end());
	}

	FieldT<T> operator[](unsigned pos) const noexcept {
	  return FieldT<T>(core_[pos]);
	}

private:
	Array core_;
};

template <typename K, typename V>
class PairT final {
public:
	explicit PairT(const Pair& pair) : core_(pair) {}
	bool operator!() const noexcept {
		return !core_;
	}

	K Key(const uint32_t* end=nullptr) const noexcept {
		return FieldT<K>(core_.Key()).Get(end);
	}

	V Value(const uint32_t* end=nullptr) const noexcept {
		return FieldT<V>(core_.Value()).Get(end);
	}

private:
	Pair core_;
};

template <typename K, typename V>
class MapT final {
public:
	explicit MapT(const Map& map) : core_(map) {}
	bool operator!() const noexcept {
		return !core_;
	}

	uint32_t Size() const noexcept {
		return core_.Size();
	}

	class Iterator final {
	public:
		explicit Iterator(const Map::Iterator& core) : core_(core) {}
		bool operator==(const Iterator& other) const noexcept {
	  		return core_ == other.core_;
		}
		bool operator!=(const Iterator& other) const noexcept {
			return core_ != other.core_;
		}
		Iterator& operator+=(ptrdiff_t step) noexcept {
			core_ += step;
			return *this;
		}
		Iterator& operator-=(ptrdiff_t step) noexcept {
			core_ -= step;
			return *this;
		}
		Iterator operator+(ptrdiff_t step) noexcept {
			return Iterator(core_ + step);
		}
		Iterator operator-(ptrdiff_t step) noexcept {
			return Iterator(core_ - step);
		}
		Iterator& operator++() noexcept {
			++core_;
			return *this;
		}
		Iterator& operator--() noexcept {
			--core_;
			return *this;
		}
		Iterator operator++(int) noexcept {
			return Iterator(core_++);
		}
		Iterator operator--(int) noexcept {
			return Iterator(core_--);
		}

		PairT<K,V> operator*() const noexcept {
			return PairT<K,V>(*core_);
		}

	private:
		Map::Iterator core_;
	};

	Iterator begin() const noexcept {
		return Iterator(core_.begin());
	}
	Iterator end() const noexcept {
		return Iterator(core_.end());
	}

	Iterator Find(K key, const uint32_t* end=nullptr) {
		return Iterator(core_.Find(key, end));
	}

private:
	Map core_;
};

static inline int32_t GetInt32(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<int32_t>(message.GetField(id, end)).Get();
}

static inline uint32_t GetUInt32(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<uint32_t>(message.GetField(id, end)).Get();
}

static inline int64_t GetInt64(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<int64_t>(message.GetField(id, end)).Get();
}

static inline uint64_t GetUInt64(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<uint64_t>(message.GetField(id, end)).Get();
}

static inline bool GetBool(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<bool>(message.GetField(id, end)).Get();
}

static inline int GetEnumValue(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<int32_t>(message.GetField(id, end)).Get();
}

static inline float GetFloat(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<float>(message.GetField(id, end)).Get();
}

static inline double GetDouble(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<double>(message.GetField(id, end)).Get();
}

static inline Slice<uint8_t> GetBytes(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<Slice<uint8_t>>(message.GetField(id, end)).Get(end);
}

static inline Slice<char> GetString(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<Slice<char>>(message.GetField(id, end)).Get(end);
}

static inline Slice<bool> GetBoolArray(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<Slice<bool>>(message.GetField(id, end)).Get(end);
}

static inline Slice<int32_t> GetInt32Array(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return Array(message.GetField(id, end).GetObject(end), end).AsScalarV<int32_t>();
}

static inline Slice<uint32_t> GetUInt32Array(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return Array(message.GetField(id, end).GetObject(end), end).AsScalarV<uint32_t>();
}

static inline Slice<int64_t> GetInt64Array(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return Array(message.GetField(id, end).GetObject(end), end).AsScalarV<int64_t>();
}

static inline Slice<uint64_t> GetUInt64Array(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return Array(message.GetField(id, end).GetObject(end), end).AsScalarV<uint64_t>();
}

static inline Slice<float> GetFloatArray(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return Array(message.GetField(id, end).GetObject(end), end).AsScalarV<float>();
}

static inline Slice<double> GetDoubleArray(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return Array(message.GetField(id, end).GetObject(end), end).AsScalarV<double>();
}

template <typename T>
static inline ArrayT<T> GetArray(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return ArrayT<T>(Array(message.GetField(id, end).GetObject(end), end));
}

template <typename K, typename V>
static inline MapT<K,V> GetMap(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return MapT<K,V>(Map(message.GetField(id, end).GetObject(end), end));
}

static inline Message GetMessage(const Message& message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return Message(message.GetField(id, end).GetObject(end), end);
}

} // protocache
#endif //PROTOCACHE_ACCESS_H_