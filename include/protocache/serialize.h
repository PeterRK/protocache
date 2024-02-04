// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_SERIALIZE_H_
#define PROTOCACHE_SERIALIZE_H_

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <type_traits>
#include "utils.h"
#include "access.h"
#include "perfect_hash.h"

namespace protocache {

template <typename T>
static inline T* Cast(const void* p) noexcept {
	return const_cast<T*>(reinterpret_cast<const T*>(p));
}

template <typename T>
static inline Data Serialize(T v) {
	static_assert(std::is_scalar<T>::value, "");
	Data out(WordSize(sizeof(T)), 0);
	*Cast<T>(out.data()) = v;
	return out;
}

extern Data Serialize(const Slice<char>& str);

static inline Data Serialize(const Slice<uint8_t>& data) {
	return Serialize(SliceCast<char>(data));
}

static inline Data Serialize(const Slice<bool>& data) {
	return Serialize(SliceCast<char>(data));
}

static inline Data Serialize(const std::string& str) {
	return Serialize(Slice<char>(str));
}

extern Data SerializeMessage(const std::vector<Data>& parts);
extern Data SerializeArray(const std::vector<Data>& elements);
extern Data SerializeMap(const Slice<uint8_t>& index, const std::vector<Data>& keys, const std::vector<Data>& values);

class ScalarReader : public KeyReader {
public:
	explicit ScalarReader(const std::vector<Data>& keys) : keys_(keys) {}

	void Reset() override {
		idx_ = 0;
	}
	size_t Total() override {
		return keys_.size();
	}
	Slice<uint8_t> Read() override {
		if (idx_ >= keys_.size()) {
			return {};
		}
		auto& key = keys_[idx_++];
		return {reinterpret_cast<const uint8_t*>(key.data()), key.size()*4};
	}

protected:
	const std::vector<Data>& keys_;
	size_t idx_ = 0;
};

struct StringReader : public ScalarReader {
	explicit StringReader(const std::vector<Data>& keys) : ScalarReader(keys) {}

	Slice<uint8_t> Read() override {
		if (idx_ >= keys_.size()) {
			return {};
		}
		auto& key = keys_[idx_++];
		return String(key.data()).GetBytes();
	}
};

} // protocache
#endif //PROTOCACHE_SERIALIZE_H_