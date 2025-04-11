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

bool Serialize(const Slice<char>& str, Buffer* buf) {
	if (str.size() >= (1U << 30U)) {
		return false;
	}
	uint32_t mark = str.size() << 2U;
	uint8_t header[5];
	auto sz = WriteVarInt(header, mark);
	auto size = WordSize(sz+str.size());
	auto dest = buf->Expand(size);
	dest[size-1] = 0;
	auto p = reinterpret_cast<uint8_t*>(dest);
	for (unsigned i = 0; i < sz; i++) {
		*p++ = header[i];
	}
	memcpy(p, str.data(), str.size());
	return true;
}

static size_t BestArraySize(const std::vector<Buffer::Seg>& elements, unsigned& m) {
	size_t sizes[3] = {0, 0, 0};
	for (auto& one : elements) {
		sizes[0] += 1;
		sizes[1] += 2;
		sizes[2] += 3;
		if (one.len <= 1) {
			continue;
		}
		sizes[0] += one.len;
		if (one.len <= 2) {
			continue;
		}
		sizes[1] += one.len;
		if (one.len <= 3) {
			continue;
		}
		sizes[2] += one.len;
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

static inline Part Pick(Buffer& buf, uint32_t*& tail, unsigned m, const Buffer::Seg& seg) {
	Part out;
	if (seg.len <= m) {
		out.len = seg.len;
		auto src = buf.AtSize(seg.pos);
		for (unsigned j = 0; j < seg.len; j++) {
			out.data[j] = src[j];
		}
	} else {
		out.len = 0;
		out.seg = seg;
		auto src = buf.AtSize(seg.end());
		if (tail > src) {
			for (unsigned j = 0; j < seg.len; j++) {
				*--tail = *--src;
			}
			out.seg.pos -= tail - src;
		} else {
			tail -= seg.len;
		}
	}
	return out;
}

static inline void Mark(const Part& part, Buffer& buf, unsigned m) {
	auto cell = buf.Expand(m);
	if (part.len == 0) {
		*cell = Offset(buf.Size() - part.seg.pos);
		for (unsigned i = 1; i < m; i++) {
			cell[i] = 0;
		}
	} else {
		for (unsigned i = 0; i < part.len; i++) {
			cell[i] = part.data[i];
		}
		for (unsigned i = part.len; i < m; i++) {
			cell[i] = 0;
		}
	}
}

bool SerializeArray(const std::vector<Buffer::Seg>& elements, Buffer& buf, size_t end) {
	if (elements.empty()) {
		buf.Put(1U);
		return true;
	}

	unsigned m = 0;
	size_t size = BestArraySize(elements, m);
	if (size >= (1U<<30U)) {
		return false;
	}

	std::vector<Part> parts(elements.size());
	auto tail = buf.AtSize(end);
	for (int i = static_cast<int>(parts.size())-1; i >= 0; i--) {
		auto& one = elements[i];
		if (one.len == 0) {
			return false;
		}
		parts[i] = Pick(buf, tail, m, one);
	}
	buf.Shrink(tail - buf.Head());
	for (int i = static_cast<int>(parts.size())-1; i >= 0; i--) {
		Mark(parts[i], buf, m);
	}
	uint32_t head = (elements.size() << 2U) | m;
	buf.Put(head);
	return true;
}

bool SerializeMap(const Slice<uint8_t>& index, const std::vector<Buffer::Seg>& keys,
				  const std::vector<Buffer::Seg>& values, Buffer& buf, size_t end) {
	if (keys.size() != values.size()) {
		return false;
	}
	if (keys.empty()) {
		buf.Put(5U << 28U);
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

	struct KvPart {
		Part key;
		Part val;
	};
	std::vector<KvPart> parts(keys.size());
	auto tail = buf.AtSize(end);
	for (int i = static_cast<int>(parts.size())-1; i >= 0; i--) {
		auto& key = keys[i];
		auto& val = values[i];
		if (key.len == 0 || val.len == 0) {
			return false;
		}
		parts[i].val = Pick(buf, tail, m2, val);
		parts[i].key = Pick(buf, tail, m1, key);
	}
	buf.Shrink(tail - buf.Head());
	for (int i = static_cast<int>(parts.size())-1; i >= 0; i--) {
		Mark(parts[i].val, buf, m2);
		Mark(parts[i].key, buf, m1);
	}
	auto head = buf.Expand(index_size);
	head[index_size-1] = 0;
	memcpy(head, index.data(), index.size());
	*head |= (m1<<30U) | (m2<<28U);
	return true;
}

bool SerializeMessage(const std::vector<Buffer::Seg>& fields, Buffer& buf, size_t end) {
	if (fields.empty()) {
		return false;
	}
	std::vector<Part> parts(fields.size());
	auto tail = buf.AtSize(end);
	size_t size = 0;
	for (int i = static_cast<int>(parts.size())-1; i >= 0; i--) {
		auto& one = fields[i];
		auto& part = parts[i];
		if (one.len == 0) {
			part.len = 4;	// skip
			continue;
		}
		size += one.len;
		part = Pick(buf, tail, 3, one);
	}
	if (size >= (1U<<30U)) {
		return false;
	}
	while (!parts.empty() && parts.back().len > 3) {
		parts.pop_back();
	}
	if (parts.empty()) {
		buf.Put(0U);
		return true;
	}
	auto section = (parts.size() + 12) / 25;
	if (section > 0xff) {
		return false;
	}

	buf.Shrink(tail - buf.Head());
	for (int i = static_cast<int>(parts.size())-1; i >= 0; i--) {
		auto& part = parts[i];
		if (part.len == 0) {
			buf.Put(Offset(buf.Size()+1 - part.seg.pos));
		} else if (part.len < 4) {
			auto cell = buf.Expand(part.len);
			for (unsigned j = 0; j < part.len; j++) {
				cell[j] = part.data[j];
			}
		}
	}

	auto head = buf.Expand(1 + section*2);
	*head = section;
	uint32_t cnt = 0;
	for (unsigned i = 0; i < std::min(12UL, parts.size()); i++) {
		auto& part = parts[i];
		if (part.len == 0) {
			*head |= 1U << (8U+i*2U);
			cnt += 1;
		} else if (part.len < 4) {
			*head |= part.len << (8U+i*2U);
			cnt += part.len;
		}
	}
	auto blk = reinterpret_cast<uint64_t*>(head+1);
	for (unsigned i = 12; i < parts.size(); ) {
		if (cnt >= (1U<<14U)) {
			return false;
		}
		auto next = std::min(i + 25, static_cast<unsigned>(parts.size()));
		auto mark = static_cast<uint64_t>(cnt) << 50U;
		for (unsigned j = 0; i < next; j+=2) {
			auto& part = parts[i++];
			if (part.len == 0) {
				mark |= 1ULL << j;
				cnt += 1;
			} else if (part.len < 4) {
				mark |= static_cast<uint64_t>(part.len) << j;
				cnt += part.len;
			}
		}
		*blk++ = mark;
	}
	return true;
}

} // protocache