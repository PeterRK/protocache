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
#include "serialize.h"
#include "perfect_hash.h"

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
	using ConstIterator = typename std::vector<TypeEX>::const_iterator;
	std::vector<TypeEX> core_;

public:
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end);
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		return ArrayT<T>::Detect(ptr, end);
	}
	Data Serialize() const;

	size_t size() const noexcept { return core_.size(); }
	bool empty() const noexcept { return core_.empty(); }
	void clear() noexcept { core_.clear(); }
	void reserve(size_t n) { core_.reserve(n); }
	void resize(size_t n) { core_.resize(n); }
	Iterator begin() noexcept { return core_.begin(); }
	ConstIterator begin() const noexcept { return core_.begin(); }
	Iterator end() noexcept { return core_.end(); }
	ConstIterator end() const noexcept { return core_.end(); }
	TypeEX& operator[](size_t pos) noexcept { return core_[pos]; }
	const TypeEX& operator[](size_t pos) const noexcept { return core_[pos]; }
	TypeEX& front() noexcept { return core_.front(); }
	const TypeEX& front() const noexcept { return core_.front(); }
	TypeEX& back() noexcept { return core_.back(); }
	const TypeEX& back() const noexcept { return core_.back(); }
	void pop_back() noexcept { core_.push_back(); }

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
template <> inline Data ArrayEX<bool>::Serialize() const {
	return ::protocache::Serialize(Slice<uint8_t>(core_));
}

template <> inline ArrayEX<int32_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<int32_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline Data ArrayEX<int32_t>::Serialize() const {
	return ::protocache::SerializeScalarArray(core_);
}

template <> inline ArrayEX<uint32_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<uint32_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline Data ArrayEX<uint32_t>::Serialize() const {
	return ::protocache::SerializeScalarArray(core_);
}

template <> inline ArrayEX<int64_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<int64_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline Data ArrayEX<int64_t>::Serialize() const {
	return ::protocache::SerializeScalarArray(core_);
}

template <> inline ArrayEX<uint64_t>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<uint64_t>();
	core_.assign(view.begin(), view.end());
}
template <> inline Data ArrayEX<uint64_t>::Serialize() const {
	return ::protocache::SerializeScalarArray(core_);
}

template <> inline ArrayEX<float>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<float>();
	core_.assign(view.begin(), view.end());
}
template <> inline Data ArrayEX<float>::Serialize() const {
	return ::protocache::SerializeScalarArray(core_);
}

template <> inline ArrayEX<double>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end).Numbers<double>();
	core_.assign(view.begin(), view.end());
}
template <> inline Data ArrayEX<double>::Serialize() const {
	return ::protocache::SerializeScalarArray(core_);
}

template <> inline ArrayEX<Slice<char>>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = ArrayT<Slice<char>>(data, end);
	core_.reserve(view.Size());
	for (auto one : view) {
		core_.emplace_back(one.data(), one.size());
	}
}

template <> inline ArrayEX<Slice<uint8_t>>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = ArrayT<Slice<char>>(data, end);
	core_.reserve(view.Size());
	for (auto one : view) {
		core_.emplace_back(one.data(), one.size());
	}
}

template <typename T>
inline ArrayEX<T>::ArrayEX(const uint32_t* data, const uint32_t* end) {
	auto view = Array(data, end);
	core_.reserve(view.Size());
	for (auto field : view) {
		core_.emplace_back(field.GetObject(), end);
	}
}

template <typename T>
static inline Data Serialize(const T& obj) {
	return obj.Serialize();
}

template <typename T>
inline Data ArrayEX<T>::Serialize() const {
	std::vector<Data> elements;
	elements.reserve(core_.size());
	for (auto& one : core_) {
		elements.push_back(::protocache::Serialize(one));
		if (elements.back().empty()) {
			return {};
		}
	}
	return ::protocache::SerializeArray(elements);
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

static inline std::string ExtractField(const FieldT<Slice<uint8_t>>& field, const uint32_t* end) {
	auto view = SliceCast<char>(field.Get(end));
	std::string out(view.data(), view.size());
	return out;
}

template <typename K, typename V>
class _MapKeyReader : public KeyReader {
public:
	static_assert(sizeof(K) >= 4, "");
	explicit _MapKeyReader(const std::unordered_map<K,V>& core)
		: core_(core), it_(core.begin()) {}

	void Reset() override {
		it_ = core_.begin();
	}
	size_t Total() override {
		return core_.size();
	}
	Slice<uint8_t> Read() override {
		if (it_ == core_.end()) {
			return {};
		}
		auto& key = it_->first;
		++it_;
		return {reinterpret_cast<const uint8_t*>(&key), sizeof(K)};
	}

private:
	const std::unordered_map<K,V>& core_;
	typename std::unordered_map<K,V>::const_iterator it_;
};

template <typename V>
class _MapKeyReader<std::string, V> : public KeyReader {
public:
	explicit _MapKeyReader(const std::unordered_map<std::string,V>& core)
			: core_(core), it_(core.begin()) {}

	void Reset() override {
		it_ = core_.begin();
	}
	size_t Total() override {
		return core_.size();
	}
	Slice<uint8_t> Read() override {
		if (it_ == core_.end()) {
			return {};
		}
		auto& key = it_->first;
		++it_;
		return {reinterpret_cast<const uint8_t*>(key.data()), key.size()};
	}

private:
	const std::unordered_map<std::string,V>& core_;
	typename std::unordered_map<std::string,V>::const_iterator it_;
};

template <typename Key, typename Val>
class MapEX final {
private:
	using KeyEX = typename Adapter<Key>::TypeEX;
	using ValEX = typename Adapter<Val>::TypeEX;
	using Iterator = typename std::unordered_map<KeyEX,ValEX>::iterator;
	using ConstIterator = typename std::unordered_map<KeyEX,ValEX>::const_iterator;
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
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		return MapT<Key,Val>::Detect(ptr, end);
	}
	Data Serialize() const {
		_MapKeyReader<KeyEX,ValEX> reader(core_);
		auto index = PerfectHash::Build(reader, true);
		if (!index) {
			return {};
		}
		std::vector<Data> keys(core_.size());
		std::vector<Data> values(core_.size());
		reader.Reset();
		for (auto& pair : core_) {
			auto key = reader.Read();
			auto pos = index.Locate(key.data(), key.size());
			keys[pos] = ::protocache::Serialize(pair.first);
			if (keys[pos].empty()) {
				return {};
			}
			values[pos] = ::protocache::Serialize(pair.second);
			if (values[pos].empty()) {
				return {};
			}
		}
		return ::protocache::SerializeMap(index.Data(), keys, values);
	}

	size_t size() const noexcept { return core_.size(); }
	bool empty() const noexcept { return core_.empty(); }
	void clear() noexcept { core_.clear(); }
	void reserve(size_t n) { core_.reserve(n); }
	Iterator find(const KeyEX& key) { return core_.find(key); }
	ConstIterator find(const KeyEX& key) const { return core_.find(key); }
	Iterator begin() noexcept { return core_.begin(); }
	ConstIterator begin() const noexcept { return core_.begin(); }
	Iterator end() noexcept { return core_.end(); }
	ConstIterator end() const noexcept { return core_.end(); }
	Iterator erase(Iterator pos) { return core_.erase(pos); }
	bool erase(const KeyEX& key) { return core_.erase(key) != 0; }

	template< class... Args >
	std::pair<Iterator , bool> emplace(Args&&... args) {
		return core_.emplace(std::forward<Args>(args)...);
	}
};

template <size_t N>
class MessageEX final {
public:
	MessageEX() = default;
	MessageEX(const uint32_t* ptr, const uint32_t* end) : core_(ptr, end) {}

	template <typename T>
	T& GetField(unsigned id, const uint32_t* end, T& field) {
		if (!_accessed.test(id)) {
			_accessed.set(id);
			ExtractField(core_, id, end, field);
		}
		return field;
	}

	Slice<uint32_t> SerializeField(unsigned id, const uint32_t* end, const std::string& field, Data& data) const {
		if (!_accessed.test(id)) {
			return DetectField<Slice<char>>(core_, id, end);
		} else if (field.empty()) {
			return {};
		}
		data = Serialize(field);
		return Slice<uint32_t>(data);
	}

	template<typename T, std::enable_if_t<std::is_scalar<T>::value, bool> = true>
	Slice<uint32_t> SerializeField(unsigned id, const uint32_t* end, const T& field, Data& data) const {
		if (!_accessed.test(id)) {
			return DetectField<T>(core_, id, end);
		} else if (field == 0) {
			return {};
		}
		data = Serialize(field);
		return Slice<uint32_t>(data);
	}

	template <typename T, std::enable_if_t<!std::is_scalar<T>::value, bool> = true>
	Slice<uint32_t> SerializeField(unsigned id, const uint32_t* end, const T& field, Data& data) const {
		if (!_accessed.test(id)) {
			return DetectField<T>(core_, id, end);
		}
		data = Serialize(field);
		if (data.size() == 1) {
			data.clear();
		}
		return Slice<uint32_t>(data);
	}

	template <typename T>
	Slice<uint32_t> SerializeField(unsigned id, const uint32_t* end, const ArrayEX<T>& field, Data& data) const {
		if (!_accessed.test(id)) {
			return DetectField<ArrayT<T>>(core_, id, end);
		} else if (field.empty()) {
			return {};
		}
		data = Serialize(field);
		return Slice<uint32_t>(data);
	}

	template <typename K, typename V>
	Slice<uint32_t> SerializeField(unsigned id, const uint32_t* end, const MapEX<K,V>& field, Data& data) const {
		if (!_accessed.test(id)) {
			return DetectField<MapT<K,V>>(core_, id, end);
		} else if (field.empty()) {
			return {};
		}
		data = Serialize(field);
		return Slice<uint32_t>(data);
	}

private:
	Message core_;
	std::bitset<N> _accessed;

	template <typename T>
	static void ExtractField(const Message& message, unsigned id, const uint32_t* end, T& out) {
		out = FieldT<T>(message.GetField(id, end)).Get(end);
	}

	static void ExtractField(const Message& message, unsigned id, const uint32_t* end, std::string& out) {
		auto view = FieldT<Slice<char>>(message.GetField(id, end)).Get(end);
		out.assign(view.data(), view.size());
	}
};


} // protocache
#endif // PROTOCACHE_ACCESS_EX_H_