// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstring>
#include <vector>
#include <string>
#include "protocache/perfect_hash.h"
#include "protocache/serialize.h"

namespace protocache {

static unsigned WriteVarInt(uint8_t* buf, uint32_t n) noexcept {
	unsigned w = 0;
	while ((n & ~0x7fU) != 0) {
		buf[w++] = 0x80U | (n & 0x7fU);
		n >>= 7U;
	}
	buf[w++] = n;
	return w;
}

static inline uint32_t Offset(uint32_t off) noexcept {
	return (off<<2U) | 3U;
}

bool Serialize(const Slice<char>& str, Buffer& buf, Unit& unit) {
	if (str.size() >= (1U << 30U)) {
		return false;
	}
	uint32_t mark = str.size() << 2U;
	uint8_t header[5];
	auto sz = WriteVarInt(header, mark);
	auto size = WordSize(sz+str.size());

	auto last = buf.Size();
	auto dest = unit.data;
	if (size == 1) {
		unit.len = 1;
	} else {
		unit.len = 0;
		dest = buf.Expand(size);
	}
	dest[size-1] = 0;
	auto p = reinterpret_cast<uint8_t*>(dest);
	for (unsigned i = 0; i < sz; i++) {
		*p++ = header[i];
	}
	memcpy(p, str.data(), str.size());
	if (unit.len == 0) {
		unit = Segment(last, buf.Size());
	}
	return true;
}

static size_t BestArraySize(const std::vector<Unit>& elements, unsigned& m) {
	size_t sizes[3] = {0, 0, 0};
	for (auto& one : elements) {
		sizes[0] += 1;
		sizes[1] += 2;
		sizes[2] += 3;
		auto len = one.len;
		if (len == 0) {
			len = one.seg.len;
		}
		if (len <= 1) {
			continue;
		}
		sizes[0] += len;
		if (len <= 2) {
			continue;
		}
		sizes[1] += len;
		if (len <= 3) {
			continue;
		}
		sizes[2] += len;
	}
	unsigned mode = 0;
	for (unsigned i = 1; i < 3; i++) {
		if (sizes[i] < sizes[mode]) {
			mode = i;
		}
	}
	m = mode + 1;
	return sizes[mode];
}

static inline void Pick(Unit& unit, Buffer& buf, uint32_t*& tail, unsigned m) {
	if (unit.len != 0) {
		return;
	}
	if (unit.seg.len <= m) {
		unit.len = unit.seg.len;
		auto src = buf.AtSize(unit.seg.pos);
		for (unsigned j = 0; j < unit.len; j++) {
			unit.data[j] = src[j];
		}
	} else {	// move
		auto src = buf.AtSize(unit.seg.end());
		if (tail > src) {
			for (unsigned j = 0; j < unit.seg.len; j++) {
				*--tail = *--src;
			}
			unit.seg.pos -= tail - src;
		} else {
			tail -= unit.seg.len;
		}
	}
}

static inline void Mark(const Unit& unit, Buffer& buf, unsigned m) {
	auto cell = buf.Expand(m);
	if (unit.len == 0) {
		*cell = Offset(buf.Size() - unit.seg.pos);
		for (unsigned i = 1; i < m; i++) {
			cell[i] = 0;
		}
	} else {
		assert(unit.len < 4);
		for (unsigned i = 0; i < unit.len; i++) {
			cell[i] = unit.data[i];
		}
		for (unsigned i = unit.len; i < m; i++) {
			cell[i] = 0;
		}
	}
}

bool SerializeArray(std::vector<Unit>& elements, Buffer& buf, size_t last, Unit& unit) {
	if (elements.empty()) {
		unit.len = 1;
		unit.data[0] = 1U;
		return true;
	}

	unsigned m = 0;
	size_t size = BestArraySize(elements, m);
	if (size >= (1U<<30U)) {
		return false;
	}

	auto tail = buf.AtSize(last);
	for (int i = static_cast<int>(elements.size())-1; i >= 0; i--) {
		Pick(elements[i], buf, tail, m);
	}
	buf.Shrink(tail - buf.Head());
	for (int i = static_cast<int>(elements.size())-1; i >= 0; i--) {
		Mark(elements[i], buf, m);
	}
	buf.Put((elements.size() << 2U) | m);
	unit = Segment(last, buf.Size());
	return true;
}

bool SerializeMap(const Slice<uint8_t>& index, std::vector<Unit>& keys,
				  std::vector<Unit>& values, Buffer& buf, size_t last, Unit& unit) {
	if (keys.size() != values.size()) {
		return false;
	}
	if (keys.empty()) {
		unit.len = 1;
		unit.data[0] = 5U << 28U;
		return true;
	}

	unsigned m1 = 0;
	auto key_size = BestArraySize(keys, m1);
	unsigned m2 = 0;
	auto value_size = BestArraySize(values, m2);

	auto index_size = WordSize(index.size());
	auto size = index_size + key_size + value_size;
	if (size >= (1U<<30U)) {
		return false;
	}

	auto tail = buf.AtSize(last);
	for (int i = static_cast<int>(keys.size())-1; i >= 0; i--) {
		Pick(values[i], buf, tail, m2);
		Pick(keys[i], buf, tail, m1);
	}
	buf.Shrink(tail - buf.Head());
	for (int i = static_cast<int>(keys.size())-1; i >= 0; i--) {
		Mark(values[i], buf, m2);
		Mark(keys[i], buf, m1);
	}
	auto head = buf.Expand(index_size);
	head[index_size-1] = 0;
	memcpy(head, index.data(), index.size());
	*head |= (m1<<30U) | (m2<<28U);
	unit = Segment(last, buf.Size());
	return true;
}

bool SerializeMessage(std::vector<Unit>& fields, Buffer& buf, size_t last, Unit& unit) {
	if (fields.empty()) {
		return false;
	}
	auto tail = buf.AtSize(last);
	size_t size = 0;
	for (int i = static_cast<int>(fields.size())-1; i >= 0; i--) {
		auto& field = fields[i];
		if (field.len == 0) {
			size += field.seg.len;
			Pick(field, buf, tail, 3);
		} else {
			size += field.len;
		}
	}
	if (size >= (1U<<30U)) {
		return false;
	}
	while (!fields.empty() && fields.back().size() == 0) {
		fields.pop_back();
	}
	if (fields.empty()) {
		unit.len = 1;
		unit.data[0] = 0U;
		return true;
	}
	auto section = (fields.size() + 12) / 25;
	if (section > 0xff) {
		return false;
	}

	buf.Shrink(tail - buf.Head());
	for (int i = static_cast<int>(fields.size())-1; i >= 0; i--) {
		auto& field = fields[i];
		if (field.len == 0) {
			if (field.seg.len != 0) {
				buf.Put(Offset(buf.Size() + 1 - field.seg.pos));
			}
		} else {
			assert(field.len < 4);
			auto cell = buf.Expand(field.len);
			for (unsigned j = 0; j < field.len; j++) {
				cell[j] = field.data[j];
			}
		}
	}

	auto head = buf.Expand(1 + section*2);
	*head = section;
	uint32_t cnt = 0;
	for (unsigned i = 0; i < std::min(12UL, fields.size()); i++) {
		auto& field = fields[i];
		if (field.len == 0) {
			if (field.seg.len != 0) {
				*head |= 1U << (8U+i*2U);
				cnt += 1;
			}
		} else {
			assert(field.len < 4);
			*head |= field.len << (8U + i * 2U);
			cnt += field.len;
		}
	}
	auto blk = reinterpret_cast<uint64_t*>(head+1);
	for (unsigned i = 12; i < fields.size(); ) {
		if (cnt >= (1U<<14U)) {
			return false;
		}
		auto next = std::min(i + 25, static_cast<unsigned>(fields.size()));
		auto mark = static_cast<uint64_t>(cnt) << 50U;
		for (unsigned j = 0; i < next; j+=2) {
			auto& field = fields[i++];
			if (field.len == 0) {
				if (field.seg.len != 0) {
					mark |= 1ULL << j;
					cnt += 1;
				}
			} else {
				mark |= static_cast<uint64_t>(field.len) << j;
				cnt += field.len;
			}
		}
		*blk++ = mark;
	}
	unit = Segment(last, buf.Size());
	return true;
}

} // protocache