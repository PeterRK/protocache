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
#include <type_traits>
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
struct Unwrapper { using Type = T; };

template <typename T>
struct Unwrapper<std::unique_ptr<T>> { using Type = T; };

template <typename T>
class BaseArrayEX {
protected:
	using TypeEX = typename Adapter<T>::TypeEX;
	using Iterator = typename std::vector<TypeEX>::iterator;
	using ConstIterator = typename std::vector<TypeEX>::const_iterator;
	std::vector<TypeEX> core_;

public:
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		return ArrayT<typename Unwrapper<T>::Type>::Detect(ptr, end);
	}

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

template <typename T> class ArrayEX;
template <typename K, typename V> class MapEX;

template <typename T>
static inline bool Serialize(const T& obj, const uint32_t*, Buffer* buf) {
	return Serialize(obj, buf);
}

template <typename T>
static inline bool Serialize(const ArrayEX<T>& obj, const uint32_t* end, Buffer* buf) {
	return obj.Serialize(buf, end);
}

template <typename K, typename V>
static inline bool Serialize(const MapEX<K,V>& obj, const uint32_t* end, Buffer* buf) {
	return obj.Serialize(buf, end);
}

template <typename T>
static inline bool Serialize(const std::unique_ptr<T>& obj, const uint32_t* end, Buffer* buf) {
	if (obj == nullptr) {
		buf->Put(0U);
		return true;
	}
	return obj->Serialize(buf, end);
}

template <typename T>
class ArrayEX final : public BaseArrayEX<T> {
private:
	template <typename U>
	static void Extract(const uint32_t* data, const uint32_t* end, U& out) {
		out = U(data, end);
	}

	template <typename U>
	static void Extract(const uint32_t* data, const uint32_t* end, std::unique_ptr<U>& out) {
		out.reset(new U(data, end));
	}

public:
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) {
		auto view = Array(data, end);
		this->core_.resize(view.Size());
		for (unsigned i = 0; i < view.Size(); i++) {
			Extract(view[i].GetObject(end), end, this->core_[i]);
		}
	}
	bool Serialize(Buffer* buf, const uint32_t* end=nullptr) const {
		std::vector<Buffer::Seg> elements(this->size());
		auto tail = buf->Size();
		auto last = tail;
		for (int i = static_cast<int>(this->size())-1; i >= 0; i--) {
			if (!::protocache::Serialize(this->core_[i], end, buf)) {
				return false;
			}
			elements[i] = Segment(last, buf->Size());
			last = buf->Size();
		}
		return SerializeArray(elements, *buf, tail);
	}
};

template<typename T, typename std::enable_if<std::is_scalar<T>::value, bool>::type = true>
struct ScalarArrayEX : public BaseArrayEX<T> {
	ScalarArrayEX() = default;
	ScalarArrayEX(const uint32_t* data, const uint32_t* end) {
		auto view = Array(data, end).Numbers<T>();
		this->core_.assign(view.begin(), view.end());
	};
	bool Serialize(Buffer* buf, const uint32_t*) const {
		auto m = WordSize(sizeof(T));
		if (m*this->size() >= (1U << 30U)) {
			return false;
		}
		for (int i = static_cast<int>(this->size())-1; i >= 0; i--) {
			buf->Put(this->core_[i]);
		}
		uint32_t head = (this->size() << 2U) | m;
		buf->Put(head);
		return true;
	}
};

template <>
struct ArrayEX<bool> final : BaseArrayEX<bool> {
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) {
		auto view = String(data, end).GetBoolArray();
		this->core_.resize(view.size());
		memcpy(this->core_.data(), view.data(), view.size());
	}
	bool Serialize(Buffer* buf, const uint32_t*) const {
		return ::protocache::Serialize(Slice<uint8_t>(core_), buf);
	}
};

template <>
struct ArrayEX<int32_t> final : ScalarArrayEX<int32_t> {
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) : ScalarArrayEX<int32_t>(data, end) {}
};

template <>
struct ArrayEX<uint32_t> final : ScalarArrayEX<uint32_t> {
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) : ScalarArrayEX<uint32_t>(data, end) {}
};

template <>
struct ArrayEX<int64_t> final : ScalarArrayEX<int64_t> {
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) : ScalarArrayEX<int64_t>(data, end) {}
};

template <>
struct ArrayEX<uint64_t> final : ScalarArrayEX<uint64_t> {
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) : ScalarArrayEX<uint64_t>(data, end) {}
};

template <>
struct ArrayEX<float> final : ScalarArrayEX<float> {
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) : ScalarArrayEX<float>(data, end) {}
};

template <>
struct ArrayEX<double> final : ScalarArrayEX<double> {
	ArrayEX() = default;
	ArrayEX(const uint32_t* data, const uint32_t* end) : ScalarArrayEX<double>(data, end) {}
};

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

template <typename Key, typename Val>
class MapEX final {
private:
	using KeyEX = typename Adapter<Key>::TypeEX;
	using ValEX = typename Adapter<Val>::TypeEX;
	using Iterator = typename std::unordered_map<KeyEX,ValEX>::iterator;
	using ConstIterator = typename std::unordered_map<KeyEX,ValEX>::const_iterator;
	std::unordered_map<KeyEX,ValEX> core_;

	template <typename T>
	static typename Adapter<T>::TypeEX Extract(Field field, const uint32_t* end, const T*) {
		return FieldT<T>(field).Get(end);
	}

	template <typename T>
	static std::unique_ptr<T> Extract(Field field, const uint32_t* end, const std::unique_ptr<T>*) {
		std::unique_ptr<T> out(new T);
		*out = FieldT<T>(field).Get(end);
		return out;
	}

	static std::string Extract(Field field, const uint32_t* end, const Slice<char>*) {
		auto view = FieldT<Slice<char>>(field).Get(end);
		return {view.data(), view.size()};
	}
	static std::string Extract(Field field, const uint32_t* end, const Slice<uint8_t>*) {
		auto view = FieldT<Slice<char>>(field).Get(end);
		return {view.data(), view.size()};
	}

	template <typename K, typename V>
	class MapKeyReader : public KeyReader {
	public:
		explicit MapKeyReader(const std::vector<const std::pair<const K,V>*>& core) : core_(core) {}
		void Reset() override { idx_ = 0; }
		size_t Total() override { return core_.size(); }
		Slice<uint8_t> Read() override {
			if (idx_ >= core_.size()) {
				return {};
			}
			auto pair = core_[idx_++];
			return {reinterpret_cast<const uint8_t*>(&pair->first), sizeof(Key)};
		}

	private:
		const std::vector<const std::pair<const K,V>*>& core_;
		size_t idx_ = 0;
	};

	template <typename V>
	class MapKeyReader<const std::string, V> : public KeyReader {
	public:
		explicit MapKeyReader(const std::vector<const std::pair<std::string,V>*>& core) : core_(core) {}
		void Reset() override { idx_ = 0; }
		size_t Total() override { return core_.size(); }
		Slice<uint8_t> Read() override {
			if (idx_ >= core_.size()) {
				return {};
			}
			auto pair = core_[idx_++];
			return {reinterpret_cast<const uint8_t*>(&pair->first.data()), pair->first.size()};
		}

	private:
		const std::vector<const std::pair<const std::string,V>*>& core_;
		size_t idx_ = 0;
	};

public:
	MapEX() = default;
	MapEX(const uint32_t* data, const uint32_t* end) {
		auto view = Map(data, end);
		core_.reserve(view.Size());
		for (auto pair : view) {
			auto key = Extract(pair.Key(), end, (const Key*)nullptr);
			auto val = Extract(pair.Value(), end, (const Val*)nullptr);
			core_.emplace(std::move(key), std::move(val));
		}
	}
	static Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) noexcept {
		return MapT<Key,typename Unwrapper<Val>::Type>::Detect(ptr, end);
	}
	bool Serialize(Buffer* buf, const uint32_t* end=nullptr) const {
		std::vector<const std::pair<const KeyEX,ValEX>*> memo;
		memo.reserve(this->size());
		for (auto& p : core_) {
			memo.push_back(&p);
		}
		MapKeyReader<KeyEX,ValEX> reader(memo);
		auto index = PerfectHash::Build(reader, true);
		if (!index) {
			return false;
		}
		reader.Reset();
		std::vector<const std::pair<const KeyEX,ValEX>*> book(memo.size());
		for (unsigned i = 0; i < memo.size(); i++) {
			auto key = reader.Read();
			auto pos = index.Locate(key.data(), key.size());
			assert(pos < book.size());
			book[pos] = memo[i];
		}

		std::vector<Buffer::Seg> keys(book.size());
		std::vector<Buffer::Seg> values(book.size());
		auto tail = buf->Size();
		auto last = tail;
		for (int i = static_cast<int>(book.size())-1; i >= 0; i--) {
			auto pair = book[i];
			if (!::protocache::Serialize(pair->second, end, buf)) {
				return false;
			}
			values[i] = Segment(last, buf->Size());
			last = buf->Size();
			if (!::protocache::Serialize(pair->first, buf)) {
				return false;
			}
			keys[i] = Segment(last, buf->Size());
			last = buf->Size();
		}
		return ::protocache::SerializeMap(index.Data(), keys, values, *buf, tail);
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
class MessageEX final : private Message {
private:
	std::bitset<N> _accessed;

	template <typename T>
	void ExtractField(unsigned id, const uint32_t* end, T& out) {
		out = FieldT<T>(Message::GetField(id, end)).Get(end);
	}

	template <typename T>
	void ExtractField(unsigned id, const uint32_t* end, std::unique_ptr<T>& out) {
		out.reset(new T);
		*out = FieldT<T>(Message::GetField(id, end)).Get(end);
	}

	void ExtractField(unsigned id, const uint32_t* end, std::string& out) {
		auto view = FieldT<Slice<char>>(Message::GetField(id, end)).Get(end);
		out.assign(view.data(), view.size());
	}

	bool SerializeField(unsigned id, const uint32_t* end, const std::string& field, Buffer* buf) const {
		if (!_accessed.test(id)) {
			buf->Put(DetectField<Slice<char>>(*this, id, end));
			return true;
		} else if (field.empty()) {
			return true;
		}
		return ::protocache::Serialize(field, buf);
	}

	template<typename T, typename std::enable_if<std::is_scalar<T>::value, bool>::type = true>
	bool SerializeField(unsigned id, const uint32_t* end, const T& field, Buffer* buf) const {
		if (!_accessed.test(id)) {
			buf->Put(DetectField<T>(*this, id, end));
			return true;
		} else if (field == 0) {
			return true;
		}
		return ::protocache::Serialize(field, buf);
	}

	template <typename T, typename std::enable_if<!std::is_scalar<T>::value, bool>::type = true>
	bool SerializeField(unsigned id, const uint32_t* end, const T& field, Buffer* buf) const {
		auto pos = buf->Size();
		if (!_accessed.test(id)) {
			buf->Put(DetectField<typename Unwrapper<T>::Type>(*this, id, end));
		} else if (!::protocache::Serialize(field, end, buf)) {
			return false;
		}
		if (buf->Size() - pos == 1) {
			buf->Shrink(1);
		}
		return true;
	}

	template <typename T>
	bool SerializeField(unsigned id, const uint32_t* end, const ArrayEX<T>& field, Buffer* buf) const {
		if (!_accessed.test(id)) {
			buf->Put(DetectField<ArrayT<typename Unwrapper<T>::Type>>(*this, id, end));
			return true;
		} else if (field.empty()) {
			return true;
		}
		return ::protocache::Serialize(field, end, buf);
	}

	template <typename K, typename V>
	bool SerializeField(unsigned id, const uint32_t* end, const MapEX<K,V>& field, Buffer* buf) const {
		if (!_accessed.test(id)) {
			buf->Put(DetectField<MapT<K,typename Unwrapper<V>::Type>>(*this, id, end));
			return true;
		} else if (field.empty()) {
			return true;
		}
		return ::protocache::Serialize(field, end, buf);
	}

public:
	MessageEX() = default;
	MessageEX(const uint32_t* ptr, const uint32_t* end) : Message(ptr, end) {}
	const uint32_t* CleanHead() const noexcept {
		if (_accessed.none()) {
			return ptr_;
		} else {
			return nullptr;
		}
	}
	using Message::HasField;

	template <typename T>
	T& GetField(unsigned id, const uint32_t* end, T& field) {
		if (!_accessed.test(id)) {
			_accessed.set(id);
			ExtractField(id, end, field);
		}
		return field;
	}

	template<typename T>
	bool SerializeField(unsigned id, const uint32_t* end, const T& field,
						Buffer* buf, Buffer::Seg& seg) const {
		auto pos = buf->Size();
		auto done = SerializeField(id, end, field, buf);
		seg = Segment(pos, buf->Size());
		return done;
	}
};

} // protocache
#endif // PROTOCACHE_ACCESS_EX_H_