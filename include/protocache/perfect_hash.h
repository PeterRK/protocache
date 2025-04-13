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
#include "utils.h"

namespace protocache {

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
	explicit PerfectHash(const uint8_t* data, uint32_t size=0) noexcept;

	Slice<uint8_t> Data() const noexcept {
		return {data_, data_size_};
	}
	uint32_t Size() const noexcept;

	uint32_t Locate(const uint8_t* key, unsigned key_len) const noexcept;

protected:
	uint32_t section_ = 0;
	uint32_t data_size_ = 0;
	const uint8_t* data_ = nullptr;

	uint32_t Locate(uint32_t slots[]) const noexcept;
};

class PerfectHashObject final : public PerfectHash {
public:
	PerfectHashObject(PerfectHashObject&&) = default;
	PerfectHashObject& operator=(PerfectHashObject&&) = default;

	uint32_t Locate(const uint8_t* key, unsigned key_len) const noexcept;

	static PerfectHashObject Build(KeyReader& source, bool no_check=false);
private:
	std::unique_ptr<uint8_t[]> buffer_;
	PerfectHashObject() noexcept = default;
};

} //protocache
#endif //PROTOCACHE_PERFECT_HASH_H_
