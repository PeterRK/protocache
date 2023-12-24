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

uint32_t CalcMagic(const char* name, unsigned len) noexcept {
	return Hash32(reinterpret_cast<const uint8_t*>(name), len) >> 8U;
}

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
	if (section == 0xff) {
		if (id >= 16) {
			return {};
		}
		if (end != nullptr && ptr_ + 2 >= end) {
			return {};
		}
		unsigned w = (ptr_[1] >> id * 2) & 3;
		if (w == 0) {
			return {};
		}
		uint32_t mask = ~(0xffffffffU << id*2);
		uint32_t v = ptr_[1] & mask;
		v = (v&0x33333333U) + ((v>>2)&0x33333333U);
		v = (v&0xf0f0f0fU) + ((v>>4)&0xf0f0f0fU);
		v = (v&0xff00ffU) + ((v>>8)&0xff00ffU);
		v = (v&0xffffU) + ((v>>16)&0xffffU);
		if (end != nullptr && ptr_ + 2 + v + w > end) {
			return {};
		}
		return Field(ptr_ + 2 + v, w);
	}

	if (end != nullptr && ptr_ + 2 + 2 * section >= end) {
		return {};
	}
	auto vec = reinterpret_cast<const uint64_t*>(ptr_ + 1);
	auto a = id / 25;
	auto b = id % 25;
	if (a >= section) {
		return {};
	}
	unsigned w = (vec[a] >> b*2) & 3;
	if (w == 0) {
		return {};
	}
	uint64_t mask = ~(0xffffffffffffffffULL << b*2);
	uint64_t v = vec[a] & mask;
	v = (v&0x3333333333333333ULL) + ((v>>2)&0x3333333333333333ULL);
	v = (v&0xf0f0f0f0f0f0f0fULL) + ((v>>4)&0xf0f0f0f0f0f0f0fULL);
	v = (v&0xff00ff00ff00ffULL) + ((v>>8)&0xff00ff00ff00ffULL);
	v = (v&0xffff0000ffffULL) + ((v>>16)&0xffff0000ffffULL);
	v = (v&0xffffffffULL) + ((v>>32)&0xffffffffULL);
	v += vec[a] >> 50U;
	if (end != nullptr && ptr_ + 1 + 2 * section + v + w > end) {
		return {};
	}
	return Field(ptr_ + 1 + 2 * section + v, w);
}

Array::Array(const uint32_t* ptr, const uint32_t* end) noexcept : ptr_(ptr) {
	if (ptr_ == nullptr) {
		return;
	}
	switch (*ptr_ & 3) {
		case 1:
			width_ = 1;
			break;
		case 2:
			width_ = 3;
			break;
		case 3:
			width_ = 2;
			break;
		default:
			ptr_ = nullptr;
			return;
	}
	size_ = *ptr_ >> 2U;
	if (end != nullptr && ptr_ + 1 + width_ * size_ > end) {
		ptr_ = nullptr;
		return;
	}
}

Map::Map(const uint32_t* ptr, const uint32_t* end) noexcept {
	if (ptr == nullptr || ((*ptr >> 30U) & 3U) == 0 || ((*ptr >> 28U) & 3U) == 0) {
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
		auto width = GetWidth();
		assert(width.key != 0 && width.value != 0);
		if (body_ + (width.key + width.value) * Size() > end) {
			body_ = nullptr;
		}
	}
}

Map::Iterator Map::Find(const char* str, unsigned len, const uint32_t* end) const noexcept {
	auto pos = index_.Locate(reinterpret_cast<const uint8_t*>(str), len);
	if (pos >= index_.Size()) {
		return this->end();
	}
	auto width = GetWidth();
	Iterator it(body_ + (width.key + width.value) * pos, width.key, width.value);
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
	auto width = GetWidth();
	Iterator it(body_ + (width.key + width.value) * pos, width.key, width.value);
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