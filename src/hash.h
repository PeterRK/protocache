// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_HASH_H
#define PROTOCACHE_HASH_H

#include <cstdint>

namespace protocache {

union V128 {
	uint64_t u64[2];
	uint32_t u32[4];
	uint8_t u8[16];
};

extern V128 Hash128(const uint8_t* msg, unsigned len, uint64_t seed=0) noexcept;

class XorShift  {
public:
	XorShift();
	explicit XorShift(uint32_t seed) noexcept;
	uint32_t Next() noexcept;
private:
	uint32_t m_state[4];
};

} //protocache
#endif //PROTOCACHE_HASH_H