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
#include <vector>
#include <unordered_map>
#include "access.h"

namespace protocache {

template <typename T>
struct Adapter { using TypeEX = T; };

template <>
struct Adapter<bool> { using TypeEX = uint8_t; };

template <>
struct Adapter<Slice<char>> { using TypeEX = std::string; };

template <>
struct Adapter<Slice<uint8_t>> { using TypeEX = std::string; };

template <typename T>
class ArrayEX final {
private:
	using TypeEX = typename Adapter<T>::TypeEX;
	using Iterator = typename std::vector<TypeEX>::iterator;
	std::vector<TypeEX> core_;

public:
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end);

	Data Serialize() const; //TODO

	size_t size() const noexcept { return core_.size(); }
	bool empty() const noexcept { return core_.empty(); }
	void clear() noexcept { core_.clear(); }
	void reserve(size_t n) { core_.reserve(n); }
	void resize(size_t n) { core_.resize(n); }
	Iterator begin() { return core_.begin(); }
	Iterator end() { return core_.end(); }
	TypeEX& operator[](size_t pos) { return core_[pos]; }
	TypeEX& front() { return core_.front(); }
	TypeEX& back() { return core_.back(); }
	void pop_back() { core_.push_back(); }

	template< class... Args >
	void emplace_back(Args&&... args) {
		core_.emplace_back(std::forward<Args>(args)...);
	}
};

template <> inline ArrayEX<bool>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = String(data, end).GetBoolArray();
	core_.resize(view.size());
	memcpy(core_.data(), view.data(), view.size());
}

template <> inline ArrayEX<protocache::Slice<char>>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = ArrayT<protocache::Slice<char>>(data, end);
	core_.reserve(view.Size());
	for (auto one : view) {
		core_.emplace_back(one.data(), one.size());
	}
}

template <> inline ArrayEX<int32_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<int32_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline ArrayEX<uint32_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<uint32_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline ArrayEX<int64_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<int64_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline ArrayEX<uint64_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<uint64_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline ArrayEX<float>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<float>();
	core_.assign(view.begin(), view.end());
}
template <> inline ArrayEX<double>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<double>();
	core_.assign(view.begin(), view.end());
}

template <typename T> inline ArrayEX<T>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end);
	core_.reserve(view.Size());
	for (auto field : view) {
		core_.emplace_back(field.GetObject(), end);
	}
}

template <typename T>
static inline T ExtractField(const FieldT<T>& field, const uint32_t* end) {
	return field.Get(end);
}

static inline std::string ExtractField(const FieldT<Slice<char>>& field, const uint32_t* end) {
	auto view = field.Get(end);
	std::string out(view.data(), view.size());
	return out;
}

template <typename T>
static inline void ExtractField(const Message& message, unsigned id, const uint32_t* end, T* out) {
	*out = FieldT<T>(message.GetField(id, end)).Get(end);
}

static inline void ExtractField(const Message& message, unsigned id, const uint32_t* end, std::string* out) {
	auto view = FieldT<Slice<char>>(message.GetField(id, end)).Get(end);
	out->assign(view.data(), view.size());
}

template <typename Key, typename Val>
class MapEX final {
private:
	using KeyEX = typename Adapter<Key>::TypeEX;
	using ValEX = typename Adapter<Val>::TypeEX;
	using Iterator = typename std::unordered_map<KeyEX,ValEX>::iterator;
	std::unordered_map<KeyEX,ValEX> core_;

public:
	MapEX() = default;
	MapEX(const uint32_t* data, const uint32_t* end) {
		auto view = Map(data, end);
		core_.reserve(view.Size());
		for (auto pair : view) {
			auto key = ExtractField(FieldT<Key>(pair.Key()), end);
			auto val = ExtractField(FieldT<Val>(pair.Value()), end);
			core_.emplace(std::move(key), std::move(val));
		}
	}

	Data Serialize() const; //TODO

	size_t size() const noexcept { return core_.size(); }
	bool empty() const noexcept { return core_.empty(); }
	void clear() noexcept { core_.clear(); }
	void reserve(size_t n) { core_.reserve(n); }
	Iterator find(const KeyEX& key) { return core_.find(key); }
	Iterator begin() { return core_.begin(); }
	Iterator end() { return core_.end(); }
	Iterator erase(Iterator pos) { return core_.erase(pos); }
	bool erase(const KeyEX& key) { return core_.erase(key) != 0; }

	template< class... Args >
	std::pair<Iterator , bool> emplace(Args&&... args) {
		return core_.emplace(std::forward<Args>(args)...);
	}
};

} // protocache
#endif // PROTOCACHE_ACCESS_EX_H_