// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_ACCESS_H_
#define PROTOCACHE_ACCESS_H_

#include <cassert>
#include <cstdint>
#include <string>
#include "utils.h"
#include "perfect_hash.h"

namespace protocache {

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "little endian only"
#endif

using EnumValue = int32_t;

class String final {
public:
	String() noexcept = default;
	explicit String(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		auto p = reinterpret_cast<const uint8_t*>(ptr);
		auto e = reinterpret_cast<const uint8_t*>(end);
		if (p == nullptr || (*p & 3) != 0) {
			return;
		}
		size_t mark = 0;
		for (unsigned sft = 0; sft < 32U; sft += 7U) {
			if (e != nullptr && p >= e) {
				return;
			}
			uint8_t b = *p++;
			if (b & 0x80U) {
				mark |= static_cast<size_t>(b & 0x7fU) << sft;
			} else {
				mark |= static_cast<size_t>(b) << sft;
				if (e != nullptr && p+(mark>>2U) > e) {
					return;
				}
				data_ = Slice<uint8_t>(p, mark>>2U);
				return;
			}
		}
	}
	bool operator!() const noexcept {
		return !data_;
	}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		String str(ptr, end);
		if (!str) {
			return {};
		}
		auto off = str.data_.data() - reinterpret_cast<const uint8_t*>(ptr);
		auto size = (off + str.data_.size() + 3) / 4;
		return {ptr, size};
	}

	Slice<char> Get() const noexcept  {
		return SliceCast<char>(data_);
	}
	Slice<uint8_t> GetBytes() const noexcept  {
		return data_;
	}
	Slice<bool> GetBoolArray() const noexcept  {
		return SliceCast<bool>(data_);
	}

private:
	Slice<uint8_t> data_;
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

static inline unsigned Count32(uint32_t v) noexcept {
	v = (v&0x33333333U) + ((v>>2U)&0x33333333U);
	v = v + (v>>4U);
	v = (v&0xf0f0f0fU) + ((v>>8U)&0xf0f0f0fU);
	v = v + (v>>16U);
	return v & 0xffU;
}

static inline unsigned Count64(uint64_t v) noexcept {
	v = (v&0x3333333333333333ULL) + ((v>>2U)&0x3333333333333333ULL);
	v = v + (v>>4U);
	v = (v&0xf0f0f0f0f0f0f0fULL) + ((v>>8U)&0xf0f0f0f0f0f0f0fULL);
	v = v + (v >> 16U);
	v = v + (v >> 32U);
	return v & 0xffU;
}

class Message {
public:
	Message() noexcept : ptr_(&s_empty) {};
	explicit Message(const Slice<uint32_t>& data) : Message(data.data(), data.end()) {}
	explicit Message(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept : ptr_(&s_empty) {
		if (ptr == nullptr) {
			return;
		}
		uint32_t section = *ptr & 0xff;
		auto body = ptr + 1 + section*2;
		if (end != nullptr && body > end) {
			return;
		}
		ptr_ = ptr;
	};
	bool operator!() const noexcept {
		return ptr_ == &s_empty;
	}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		if (ptr == nullptr) {
			return {};
		}
		uint32_t section = *ptr & 0xff;
		auto tail = ptr + 1 + section*2;
		if (section == 0) {
			tail += Count32(*ptr);
		} else {
			auto sec = *reinterpret_cast<const uint64_t*>(tail - 2);
			tail += Count64(sec << 14U) + (sec >> 50U);
		}
		if (end != nullptr && tail > end) {
			return {};
		}
		return {ptr, static_cast<size_t>(tail-ptr)};
	}

	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		uint32_t section = *ptr_ & 0xff;
		if (id < 12) {
			uint32_t v = *ptr_ >> 8U;
			auto width = (v >> id * 2) & 3;
			return width != 0;
		}
		auto vec = reinterpret_cast<const uint64_t*>(ptr_ + 1);
		auto a = (id-12) / 25;
		auto b = (id-12) % 25;
		if (a >= section) {
			return false;
		}
		auto width = (vec[a] >> b*2) & 3;
		return width != 0;
	}

	Field GetField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		uint32_t section = *ptr_ & 0xff;
		auto body = ptr_ + 1 + section*2;
		unsigned off = 0;
		unsigned width = 0;
		if (id < 12) {
			uint32_t v = *ptr_ >> 8U;
			width = (v >> id*2) & 3;
			if (width == 0) {
				return {};
			}
			v &= ~(0xffffffffU << id*2);
			off = Count32(v);
		} else {
			auto vec = reinterpret_cast<const uint64_t*>(ptr_ + 1);
			auto a = (id-12) / 25;
			auto b = (id-12) % 25;
			if (a >= section) {
				return {};
			}
			width = (vec[a] >> b*2) & 3;
			if (width == 0) {
				return {};
			}
			uint64_t mask = ~(0xffffffffffffffffULL << b*2);
			off = Count64(vec[a] & mask) + (vec[a] >> 50U);
		}
		if (end != nullptr && body + off + width > end) {
			return {};
		}
		return Field(body + off, width);
	}

	static Message Cast(const void* pt) noexcept {
		return *reinterpret_cast<Message*>(&pt);
	}

	template<typename T>
	const T* Cast() const noexcept {
		return reinterpret_cast<const T*>(ptr_);
	}

protected:
	const uint32_t* ptr_;
	static const uint32_t s_empty;
};

class Array final {
public:
	Array() noexcept = default;
	explicit Array(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		if (ptr == nullptr) {
			return;
		}
		body_ = ptr + 1;
		size_ = *ptr >> 2U;
		width_ = *ptr & 3U;
		if (width_ == 0 || (end != nullptr && body_ + width_ * size_ > end)) {
			body_ = nullptr;
		}
	}
	bool operator!() const noexcept {
		return body_ == nullptr;
	}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		if (ptr == nullptr) {
			return {};
		}
		auto body = ptr + 1;
		auto size = *ptr >> 2U;
		auto width = *ptr & 3U;
		if (width == 0) {
			return {};
		}
		auto tail = body + width * size;
		if (end != nullptr && tail > end) {
			return {};
		}
		return {ptr, static_cast<size_t>(tail-ptr)};
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
		Iterator& operator++() noexcept {
			ptr_ += width_;
			return *this;
		}
		Iterator& operator--() noexcept {
			ptr_ -= width_;
			return *this;
		}

	private:
		const uint32_t* ptr_ = nullptr;
		unsigned width_ = 0;

		Iterator(const uint32_t* ptr, uint32_t width) : ptr_(ptr), width_(width) {}
		friend class Array;
	};

	Iterator begin() const noexcept {
		return {body_, width_};
	}
	Iterator end() const noexcept {
		return {body_ + width_ * size_, width_};
	}

	Field operator[](unsigned pos) const noexcept {
		return Field(body_ + width_ * pos, width_);
	}

	template<typename T>
	Slice<T> Numbers() const noexcept {
		static_assert(std::is_scalar<T>::value && sizeof(T) % 4 == 0, "");
		if (width_ != WordSize(sizeof(T))) {
			return {};
		}
		return {reinterpret_cast<const T*>(body_), size_};
	}

private:
	const uint32_t* body_ = nullptr;
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
	explicit Map(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		if (ptr == nullptr) {
			return;
		}
		key_width_ = (*ptr >> 30U) & 3U;
		value_width_ = (*ptr >> 28U) & 3U;
		if (key_width_ == 0 || value_width_ == 0) {
			return;
		}
		uint32_t sz = 0;
		if (end != nullptr) {
			sz = (end - ptr) * 4;
		}
		index_ = PerfectHash(reinterpret_cast<const uint8_t*>(ptr), sz);
		if (!index_) {
			return;
		}
		body_ = ptr + WordSize(index_.Data().size());
		if (end != nullptr && body_ + (key_width_ + value_width_) * index_.Size() > end) {
			body_ = nullptr;
		}
	}
	bool operator!() const noexcept {
		return body_ == nullptr;
	}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		if (ptr == nullptr) {
			return {};
		}
		auto key_width = (*ptr >> 30U) & 3U;
		auto value_width = (*ptr >> 28U) & 3U;
		if (key_width == 0 || value_width == 0) {
			return {};
		}
		uint32_t sz = 0;
		if (end != nullptr) {
			sz = (end - ptr) * 4;
		}
		PerfectHash index(reinterpret_cast<const uint8_t*>(ptr), sz);
		if (!index) {
			return {};
		}
		auto body = ptr + WordSize(index.Data().size());
		auto tail = body + (key_width + value_width) * index.Size();
		if (end != nullptr && tail > end) {
			return {};
		}
		return {ptr, static_cast<size_t>(tail-ptr)};
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
		Iterator& operator++() noexcept {
			ptr_ += key_width_ + value_width_;
			return *this;
		}
		Iterator& operator--() noexcept {
			ptr_ -= key_width_ + value_width_;
			return *this;
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

	Iterator Find(const char* str, unsigned len, const uint32_t* end=nullptr) const noexcept {
		auto pos = index_.Locate(reinterpret_cast<const uint8_t*>(str), len);
		if (pos >= index_.Size()) {
			return this->end();
		}
		Iterator it(body_ + (key_width_ + value_width_) * pos, key_width_, value_width_);
		auto view = String((*it).Key().GetObject(end), end).Get();
		if (!view || view != Slice<char>(str, len)) {
			return this->end();
		}
		return it;
	}

	Iterator Find(const Slice<char>& key, const uint32_t* end=nullptr) const noexcept {
		return Find(key.data(), key.size(), end);
	}
	Iterator Find(const std::string& key, const uint32_t* end=nullptr) const noexcept {
		return Find(key.data(), key.size(), end);
	}

	template<typename T, typename std::enable_if<std::is_scalar<T>::value, bool>::type = true>
	Iterator Find(T val, const uint32_t* end=nullptr) const noexcept {
		auto pos = index_.Locate(reinterpret_cast<const uint8_t *>(val), sizeof(T));
		if (pos >= index_.Size()) {
			return this->end();
		}
		Iterator it(body_ + (key_width_ + value_width_) * pos, key_width_, value_width_);
		auto view = (*it).Key().GetValue(end);
		if (view.size() ==  (sizeof(T)+3) / 4
			&& val == *reinterpret_cast<const T*>(view.data())) {
			return it;
		}
		return this->end();
	}

private:
	PerfectHash index_;
	const uint32_t* body_ = nullptr;
	unsigned key_width_ = 0;
	unsigned value_width_ = 0;
};

template <typename T>
class ScalarField {
public:
	explicit ScalarField(const Field &field) : core_(field) {}
	bool operator!() const noexcept {
		return !core_;
	}

	T Get(const uint32_t* end=nullptr) const {
		auto view = core_.GetValue(end);
		if (view.size() != WordSize(sizeof(T))) {
			return 0;
		}
		return *reinterpret_cast<const T*>(view.data());
	}

	Slice<uint32_t> Detect(const uint32_t* end=nullptr) const {
		return core_.GetValue(end);
	}

private:
	Field core_;
};

template <typename T>
class StringField {
public:
	explicit StringField(const Field &field) : core_(field) {}
	bool operator!() const noexcept {
		return !core_;
	}

	Slice<T> Get(const uint32_t* end=nullptr) const {
		return SliceCast<T>(String(core_.GetObject(end), end).Get());
	}

	Slice<uint32_t> Detect(const uint32_t* end=nullptr) const {
		return String::Detect(core_.GetObject(end), end);
	}

private:
	Field core_;
};

template <typename T>
class FieldT final {
public:
	explicit FieldT(const Field& field) : core_(field) {}
	bool operator!() const noexcept {
		return !core_;
	}

	T Get(const uint32_t* end=nullptr) const {
		return T(core_.GetObject(end), end);
	}

	Slice<uint32_t> Detect(const uint32_t* end=nullptr) const {
		return T::Detect(core_.GetObject(end), end);
	}

private:
	Field core_;
};

template <typename T>
struct FieldT<const T*> final {
public:
	explicit FieldT(const Field& field) : core_(field) {}
	bool operator!() const noexcept {
		return !core_;
	}

	const T* Get(const uint32_t* end=nullptr) const {
		return Message(core_.GetObject(end), end).Cast<T>();
	}

	Slice<uint32_t> Detect(const uint32_t* end=nullptr) const {
		return T::Detect(core_.GetObject(end), end);
	}

private:
	Field core_;
};

template <>
struct FieldT<bool> final : public ScalarField<bool> {
	explicit FieldT(const Field& field) : ScalarField<bool>(field) {}
};

template <>
struct FieldT<int32_t> final : public ScalarField<int32_t> {
	explicit FieldT(const Field& field) : ScalarField<int32_t>(field) {}
};

template <>
struct FieldT<uint32_t> final : public ScalarField<uint32_t> {
	explicit FieldT(const Field& field) : ScalarField<uint32_t>(field) {}
};

template <>
struct FieldT<int64_t> final : public ScalarField<int64_t> {
	explicit FieldT(const Field& field) : ScalarField<int64_t>(field) {}
};

template <>
struct FieldT<uint64_t> final : public ScalarField<uint64_t> {
	explicit FieldT(const Field& field) : ScalarField<uint64_t>(field) {}
};

template <>
struct FieldT<float> final : public ScalarField<float> {
	explicit FieldT(const Field& field) : ScalarField<float>(field) {}
};

template <>
struct FieldT<double> final : public ScalarField<double> {
	explicit FieldT(const Field& field) : ScalarField<double>(field) {}
};

template <>
struct FieldT<Slice<char>> final : public StringField<char> {
	explicit FieldT(const Field& field) : StringField<char>(field) {}
};

template <>
struct FieldT<Slice<uint8_t>> final : public StringField<uint8_t> {
	explicit FieldT(const Field& field) : StringField<uint8_t>(field) {}
};

template <>
struct FieldT<Slice<bool>> final : public StringField<bool> {
	explicit FieldT(const Field& field) : StringField<bool>(field) {}
};

template <typename T>
class ArrayT final {
public:
	explicit ArrayT(const Array& array) : core_(array) {}
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}
	bool operator!() const noexcept {
		return !core_;
	}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		auto view =  Array::Detect(ptr, end);
		if (!view) {
			return {};
		}
		static_assert(std::is_pointer<T>::value || !std::is_scalar<T>::value, "");
		Array core(ptr);
		for (auto it = core.end(); it != core.begin();) {
			FieldT<T> field(*--it);
			auto t = field.Detect(end);
			if (t.end() > view.end()) {
				return {view.data(), static_cast<size_t>(t.end()-view.data())};
			}
		}
		return view;
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
		Iterator& operator++() noexcept {
			++core_;
			return *this;
		}

		T Get(const uint32_t* end=nullptr) const noexcept {
			return FieldT<T>(*core_).Get(end);
		}
		T operator*() const noexcept {
			return Get();
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

	T At(unsigned pos, const uint32_t* end=nullptr) const noexcept {
		return FieldT<T>(core_[pos]).Get(end);
	}
	T operator[](unsigned pos) const noexcept {
		return At(pos);
	}

private:
	Array core_;
};

template <typename T>
class ScalarArray {
public:
	static_assert(std::is_scalar<T>::value, "");
	explicit ScalarArray(const Slice<T>& slice) : core_(slice) {}
	const Slice<T>& Raw() const noexcept {
		return core_;
	}
	bool operator!() const noexcept {
		return !core_;
	}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		return Array::Detect(ptr, end);
	}
	uint32_t Size() const noexcept {
		return core_.size();
	}
	const T* begin() const noexcept {
		return core_.begin();
	}
	const T* end() const noexcept {
		return core_.end();
	}
	T operator[](unsigned pos) const noexcept {
		return core_[pos];
	}
private:
	Slice<T> core_;
};

template <>
struct ArrayT<bool> final : public ScalarArray<bool> {
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr)
		: ScalarArray<bool>(String(ptr, end).GetBoolArray()) {}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		return String::Detect(ptr, end);
	}
};

template <>
struct ArrayT<int32_t> final : public ScalarArray<int32_t> {
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr)
		: ScalarArray<int32_t>(Array(ptr, end).Numbers<int32_t>()) {}
};

template <>
struct ArrayT<uint32_t> final : public ScalarArray<uint32_t> {
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr)
		: ScalarArray<uint32_t>(Array(ptr, end).Numbers<uint32_t>()) {}
};

template <>
struct ArrayT<int64_t> final : public ScalarArray<int64_t> {
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr)
		: ScalarArray<int64_t>(Array(ptr, end).Numbers<int64_t>()) {}
};

template <>
struct ArrayT<uint64_t> final : public ScalarArray<uint64_t> {
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr)
		: ScalarArray<uint64_t>(Array(ptr, end).Numbers<uint64_t>()) {}
};

template <>
struct ArrayT<float> final : public ScalarArray<float> {
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr)
		: ScalarArray<float>(Array(ptr, end).Numbers<float>()) {}
};

template <>
struct ArrayT<double> final : public ScalarArray<double> {
	explicit ArrayT(const uint32_t* ptr, const uint32_t* end=nullptr)
		: ScalarArray<double>(Array(ptr, end).Numbers<double>()) {}
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
	explicit MapT(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}
	bool operator!() const noexcept {
		return !core_;
	}

	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		auto view =  Map::Detect(ptr, end);
		if (!view) {
			return {};
		} else if (std::is_scalar<K>::value && !std::is_pointer<V>::value && std::is_scalar<V>::value) {
			return view;
		}
		Map core(ptr);
		for (auto it = core.end(); it != core.begin();) {
			Pair pair(*--it);
			Slice<uint32_t> t;
			if (std::is_pointer<V>::value || !std::is_scalar<V>::value) {
				t = FieldT<V>(pair.Value()).Detect(end);
				if (t.end() > view.end()) {
					return {view.data(), static_cast<size_t>(t.end()-view.data())};
				}
			}
			if (!std::is_scalar<K>::value) {
				t = FieldT<K>(pair.Key()).Detect(end);
				if (t.end() > view.end()) {
					return {view.data(), static_cast<size_t>(t.end()-view.data())};
				}
			}
		}
		return view;
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
		Iterator& operator++() noexcept {
			++core_;
			return *this;
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

template <typename T>
static inline T GetField(const Message message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<T>(message.GetField(id, end)).Get(end);
}

template <typename T>
static inline Slice<uint32_t> DetectField(const Message message, unsigned id, const uint32_t* end=nullptr) noexcept {
	return FieldT<T>(message.GetField(id, end)).Detect(end);
}

} // protocache
#endif //PROTOCACHE_ACCESS_H_
