// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_ACCESS_EX_H_
#define PROTOCACHE_ACCESS_EX_H_

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <atomic>
#include <memory>
#include <bitset>
#include <vector>
#include <unordered_map>
#include "access.h"


namespace protocache {

template <typename T>
class Ref final {
private:
	struct Object {
		std::atomic<uintptr_t> ref;
		T obj;
	};
	Object* ptr_ = nullptr;

public:
	Ref() = default;
	~Ref() noexcept {
		if (ptr_ != nullptr && --ptr_->ref == 0) {
			ptr_->obj.~T();
		}
	}
	Ref(const Ref& other) noexcept : ptr_(other.ptr_) {
		if (ptr_ != nullptr) {
			ptr_->ref++;
		}
	}
	Ref(Ref&& other) noexcept : ptr_(other.ptr_) {
		other.ptr_ = nullptr;
	}
	Ref<T>& operator=(const Ref<T>& other) noexcept {
		if (&other != this) {
			this->~Ref<T>();
			new(this)Ref(other);
		}
		return *this;
	}
	Ref<T>& operator=(Ref<T>&& other) noexcept {
		if (&other != this) {
			this->~Ref<T>();
			new(this)Ref(std::move(other));
		}
		return *this;
	}

	bool operator==(std::nullptr_t) const noexcept {
		return ptr_ == nullptr;
	}
	T& operator*() const noexcept {
		return ptr_->obj;
	}
	T* operator->() const noexcept {
		return &ptr_->obj;
	}

	template <typename... Args>
	static Ref New(Args&&... args) {
		Ref out;
		out.ptr_ = reinterpret_cast<Object*>(::operator new(sizeof(Object)));
		out.ptr_->ref = 1;
		new(&out.ptr_->obj)T(std::forward<Args>(args)...);
		return out;
	}
};

using SharedData = Ref<std::basic_string<uint32_t>>;

template <typename T>
struct Adapter { using TypeEX = T; };

template <>
struct Adapter<bool> { using TypeEX = uint8_t; };

template <>
struct Adapter<Slice<char>> { using TypeEX = std::string; };

template <>
struct Adapter<Slice<uint8_t>> { using TypeEX = std::basic_string<uint8_t>; };


template <unsigned N>
class MessageEX {
protected:
	protocache::SharedData data_;
	protocache::Message view_;
	std::bitset<N> mask_;

	MessageEX() = default;
	MessageEX(const protocache::SharedData& src, size_t offset)
			: data_(src), view_(src->data()+offset, src->data()+src->size()) {}

public:
	bool operator!() const noexcept { return data_ == nullptr; }
};

enum State : uint8_t {
	STATE_UNREADY, STATE_DIRTY, STATE_CLEAN
};

template <typename T>
class ArrayEX final {
private:
	using TypeEX = typename Adapter<T>::TypeEX;
	using Iterator = typename std::vector<TypeEX>::iterator;
	protocache::SharedData data_;
	unsigned offset_  = 0;
	State state_ = STATE_DIRTY;
	std::vector<TypeEX> core_;

	bool operator!() const noexcept { return data_ == nullptr; }

	void Init();
	void touch() {
		if (state_ != STATE_DIRTY) {
			if (state_== STATE_UNREADY) {
				Init();
			}
			state_ = STATE_DIRTY;
		}
	}

	size_t ViewSize() const noexcept;

public:
	ArrayEX() = default;
	ArrayEX(const protocache::SharedData& src, size_t offset)
		: data_(src), offset_(offset), state_(STATE_UNREADY) {}

	size_t size() const noexcept {
		if (state_ != STATE_UNREADY) {
			return core_.size();
		} else {
			return ViewSize();
		}
	}
	bool empty() const noexcept {
		return size() == 0;
	}
	void clear() noexcept {
		state_ = STATE_DIRTY;
		core_.clear();
	}
	void reserve(size_t n) {
		touch();
		core_.reserve(n);
	}
	void resize(size_t n) {
		touch();
		core_.resize(n);
	}
	Iterator begin() {
		touch();
		return core_.begin();
	}
	Iterator end() {
		touch();
		return core_.end();
	}
	TypeEX& operator[](size_t pos) {
		touch();
		return core_[pos];
	}
	TypeEX& front() {
		touch();
		return core_.front();
	}
	TypeEX& back() {
		touch();
		return core_.back();
	}
	void pop_back() {
		touch();
		core_.push_back();
	}

	template< class... Args >
	void emplace_back(Args&&... args) {
		touch();
		core_.emplace_back(std::forward<Args>(args)...);
	}
};

template <> inline void ArrayEX<bool>::Init() {
	auto view = String(data_->data()+offset_, data_->data()+data_->size()).GetBoolArray();
	core_.clear();
	core_.resize(view.size());
	memcpy(core_.data(), view.data(), view.size());
}
template <> inline size_t ArrayEX<bool>::ViewSize() const noexcept {
	auto view = String(data_->data()+offset_, data_->data()+data_->size()).GetBoolArray();
	return view.size();
}

template <> inline void ArrayEX<protocache::Slice<char>>::Init() {
	auto view = ArrayT<protocache::Slice<char>>(data_->data()+offset_, data_->data()+data_->size());
	core_.clear();
	core_.reserve(view.Size());
	for (auto one : view) {
		core_.emplace_back(one.data(), one.size());
	}
}

template <> inline void ArrayEX<protocache::Slice<uint8_t>>::Init() {
	auto view = ArrayT<protocache::Slice<uint8_t>>(data_->data()+offset_, data_->data()+data_->size());
	core_.clear();
	core_.reserve(view.Size());
	for (auto one : view) {
		core_.emplace_back(one.data(), one.size());
	}
}

template <> inline void ArrayEX<int32_t>::Init() {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size()).Numbers<int32_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline void ArrayEX<uint32_t>::Init() {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size()).Numbers<uint32_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline void ArrayEX<int64_t>::Init() {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size()).Numbers<int64_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline void ArrayEX<uint64_t>::Init() {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size()).Numbers<uint64_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline void ArrayEX<float>::Init() {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size()).Numbers<float>();
	core_.assign(view.begin(), view.end());
}
template <> inline void ArrayEX<double>::Init() {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size()).Numbers<double>();
	core_.assign(view.begin(), view.end());
}

template <typename T> inline void ArrayEX<T>::Init() {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size());
	core_.clear();
	core_.resize(view.Size());
	for (auto field : view) {
		auto ptr = field.GetObject();
		if (ptr == nullptr) {
			core_.emplace_back();
		} else {
			core_.emplace_back(data_, ptr - data_->data());
		}
	}
}

template <typename T> inline size_t ArrayEX<T>::ViewSize() const noexcept {
	auto view = Array(data_->data()+offset_, data_->data()+data_->size());
	return view.Size();
}

template <typename T>
static inline T ExtractField(const protocache::SharedData& data, const FieldT<T>& field) {
	auto ptr = reinterpret_cast<const Field*>(&field)->GetObject();
	if (ptr == nullptr) {
		return {};
	}
	return T(data, ptr - data->data());
}

static inline bool ExtractField(const protocache::SharedData& data, const FieldT<bool>& field) {
	return field.Get(data->data()+data->size());
}
static inline int32_t ExtractField(const protocache::SharedData& data, const FieldT<int32_t>& field) {
	return field.Get(data->data()+data->size());
}
static inline uint32_t ExtractField(const protocache::SharedData& data, const FieldT<uint32_t>& field) {
	return field.Get(data->data()+data->size());
}
static inline int64_t ExtractField(const protocache::SharedData& data, const FieldT<int64_t>& field) {
	return field.Get(data->data()+data->size());
}
static inline uint64_t ExtractField(const protocache::SharedData& data, const FieldT<uint64_t>& field) {
	return field.Get(data->data()+data->size());
}
static inline float ExtractField(const protocache::SharedData& data, const FieldT<float>& field) {
	return field.Get(data->data()+data->size());
}
static inline double ExtractField(const protocache::SharedData& data, const FieldT<double>& field) {
	return field.Get(data->data()+data->size());
}
static inline std::string ExtractField(const protocache::SharedData& data, const FieldT<Slice<char>>& field) {
	auto view = field.Get(data->data()+data->size());
	return std::string(view.data(), view.size());
}
static inline std::string ExtractField(const protocache::SharedData& data, const FieldT<Slice<uint8_t>>& field) {
	auto view = field.Get(data->data()+data->size());
	return std::string(reinterpret_cast<const char*>(view.data()), view.size());
}

template <typename Key, typename Val>
class MapEX final {
private:
	using KeyEX = typename Adapter<Key>::TypeEX;
	using ValEX = typename Adapter<Val>::TypeEX;
	using Iterator = typename std::unordered_map<KeyEX,ValEX>::iterator;
	protocache::SharedData data_;
	unsigned offset_  = 0;
	State state_ = STATE_DIRTY;
	std::unordered_map<KeyEX,ValEX> core_;

	bool operator!() const noexcept { return data_ == nullptr; }

	void Init() {
		auto view = Map(data_->data()+offset_, data_->data()+data_->size());
		core_.clear();
		core_.reserve(view.Size());
		for (auto pair : view) {
			core_.emplace(ExtractField(data_, FieldT<Key>(pair.Key())),
					ExtractField(data_, FieldT<Val>(pair.Value())));
		}
	}
	void touch() {
		if (state_!= STATE_DIRTY) {
			if (state_== STATE_UNREADY) {
				Init();
			}
			state_ = STATE_DIRTY;
		}
	}

public:
	MapEX() = default;
	MapEX(const protocache::SharedData& src, size_t offset)
			: data_(src), offset_(offset), state_(STATE_UNREADY) {}

	size_t size() const noexcept {
		if (state_ != STATE_UNREADY) {
			return core_.size();
		} else {
			auto view = Map(data_->data()+offset_, data_->data()+data_->size());
			return view.Size();
		}
	}
	bool empty() const noexcept {
		return size() == 0;
	}
	void clear() noexcept {
		state_ = STATE_DIRTY;
		core_.clear();
	}
	void reserve(size_t n) {
		touch();
		core_.reserve(n);
	}
	Iterator find(const KeyEX& key) {
		touch();
		return core_.find(key);
	}
	Iterator begin() {
		touch();
		return core_.begin();
	}
	Iterator end() {
		touch();
		return core_.end();
	}
	Iterator erase(Iterator pos) {
		touch();
		return core_.erase(pos);
	}
	bool erase(const KeyEX& key) {
		touch();
		return core_.erase(key) != 0;
	}

	template< class... Args >
	std::pair<Iterator , bool> emplace(Args&&... args) {
		touch();
		return core_.emplace(std::forward<Args>(args)...);
	}
};

} // protocache
#endif // PROTOCACHE_ACCESS_EX_H_