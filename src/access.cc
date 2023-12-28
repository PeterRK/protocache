// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cassert>
#include "hash.h"
#include "access.h"

namespace protocache {

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "little endian only"
#endif

Slice<uint8_t> String::Get(const uint32_t* end) const noexcept {
	auto send = reinterpret_cast<const uint8_t*>(end);
	auto ptr = ptr_;

	size_t mark = 0;
	for (unsigned sft = 0; sft < 5; sft += 7U) {
		if (send != nullptr && ptr >= send) {
			return {};
		}
		uint8_t b = *ptr++;
		if (b & 0x80U) {
			mark |= static_cast<size_t>(b & 0x7fU) << sft;
		} else {
			mark |= static_cast<size_t>(b) << sft;
			if (send != nullptr && ptr+(mark>>2U) > send) {
				return {};
			}
			return {ptr, mark>>2U};
		}
	}
	return {};
}

Field Message::GetField(unsigned id, const uint32_t* end) const noexcept {
	uint32_t section = *ptr_ & 0xff;
	auto body = ptr_ + 1 + section*2;
	if (end != nullptr && body >= end) {
		return {};
	}
	unsigned off = 0;
	unsigned width = 0;
	if (id < 12) {
		uint32_t v = *ptr_ >> 8U;
		width = (v >> id*2) & 3;
		if (width == 0) {
			return {};
		}
		v &= ~(0xffffffffU << id*2);
		v = (v&0x33333333U) + ((v>>2U)&0x33333333U);
		v = (v&0xf0f0f0fU) + ((v>>4U)&0xf0f0f0fU);
		v = (v&0xff00ffU) + ((v>>8U)&0xff00ffU);
		v = (v&0xffffU) + ((v>>16U)&0xffffU);
		off = v;
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
		uint64_t v = vec[a] & mask;
		v = (v&0x3333333333333333ULL) + ((v>>2U)&0x3333333333333333ULL);
		v = (v&0xf0f0f0f0f0f0f0fULL) + ((v>>4U)&0xf0f0f0f0f0f0f0fULL);
		v = (v&0xff00ff00ff00ffULL) + ((v>>8U)&0xff00ff00ff00ffULL);
		v = (v&0xffff0000ffffULL) + ((v>>16U)&0xffff0000ffffULL);
		v = (v&0xffffffffULL) + ((v>>32U)&0xffffffffULL);
		off = v + (vec[a] >> 50U);
	}
	if (end != nullptr && body + off + width > end) {
		return {};
	}
	return Field(body + off, width);
}

Array::Array(const uint32_t* ptr, const uint32_t* end) noexcept : ptr_(ptr) {
	if (ptr_ == nullptr) {
		return;
	}
	size_ = *ptr_ >> 2U;
	width_ = *ptr_ & 3U;
	if (width_ == 0 || (end != nullptr && ptr_ + 1 + width_ * size_ > end)) {
		ptr_ = nullptr;
	}
}

Map::Map(const uint32_t* ptr, const uint32_t* end) noexcept {
	if (ptr == nullptr) {
		return;
	}
	key_width_ = (*ptr >> 30U) & 3U;
	value_width_ = (*ptr >> 28U) & 3U;
	if (key_width_ == 0 || value_width_ == 0) {
		return;
	}
	if (end == nullptr) {
		index_ = PerfectHash(reinterpret_cast<const uint8_t*>(ptr));
		if (!index_) {
			return;
		}
		body_ = ptr + WordSize(index_.Data().size());
	} else {
		index_ = PerfectHash(reinterpret_cast<const uint8_t*>(ptr), (end - ptr) * 4);
		if (!index_) {
			return;
		}
		body_ = ptr + WordSize(index_.Data().size());
		if (body_ + (key_width_ + value_width_) * Size() > end) {
			body_ = nullptr;
		}
	}
}

Map::Iterator Map::Find(const char* str, unsigned len, const uint32_t* end) const noexcept {
	auto pos = index_.Locate(reinterpret_cast<const uint8_t*>(str), len);
	if (pos >= index_.Size()) {
		return this->end();
	}
	Iterator it(body_ + (key_width_ + value_width_) * pos, key_width_, value_width_);
	String key((*it).Key().GetObject(end));
	if (!key) {
		return this->end();
	}
	auto view = key.Get(end);
	if (!view || view.cast<char>() != Slice<char>(str, len)) {
		return this->end();
	}
	return it;
}

Map::Iterator Map::Find(const uint32_t* val, unsigned len, const uint32_t* end) const noexcept {
	auto pos = index_.Locate(reinterpret_cast<const uint8_t *>(val), len * 4);
	if (pos >= index_.Size()) {
		return this->end();
	}
	Iterator it(body_ + (key_width_ + value_width_) * pos, key_width_, value_width_);
	auto view= (*it).Key().GetValue();
	if (!view || view.size() < len) {
		return this->end();
	}
	assert(len == 1 || len == 2);
	if (len == 2) {
		if (*reinterpret_cast<const uint64_t*>(val) == *reinterpret_cast<const uint64_t*>(view.data())) {
			return it;
		}
	} else {
		if (*val == *view.data()) {
			return it;
		}
	}
	return this->end();
}

} // protocache