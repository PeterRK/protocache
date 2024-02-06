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

Data Serialize(const std::vector<std::string>& array) {
	std::vector<Data> elements;
	elements.reserve(array.size());
	for (auto& one : array) {
		elements.push_back(Serialize(one));
		if (elements.back().empty()) {
			return {};
		}
	}
	return SerializeArray(elements);
}

Data Serialize(const Slice<char>& str) {
	if (str.size() >= (1U << 30U)) {
		return {};
	}
	uint32_t mark = str.size() << 2U;
	uint8_t header[5];
	auto sz = WriteVarInt(header, mark);
	Data out(WordSize(sz+str.size()), 0);
	auto p = Cast<uint8_t>(out.data());
	for (unsigned i = 0; i < sz; i++) {
		*p++ = header[i];
	}
	memcpy(p, str.data(), str.size());
	return out;
}

Data SerializeMessage(std::vector<Data>& parts) {
	while (!parts.empty() && parts.back().empty()) {
		parts.pop_back();
	}
	Data out;
	if (parts.empty()) {
		Data out = {0};
		return out;
	}
	auto section = (parts.size() + 12) / 25;
	if (section > 0xff) {
		return {};
	}
	size_t size = 1 + section*2;
	uint32_t cnt = 0;
	uint32_t head = section;
	for (unsigned i = 0; i < std::min(12UL, parts.size()); i++) {
		auto& one = parts[i];
		if (one.size() < 4) {
			head |= one.size() << (8U+i*2U);
			size += one.size();
			cnt += one.size();
		} else {
			head |= 1U << (8U+i*2U);
			size += 1 + one.size();
			cnt += 1;
		}
	}
	for (unsigned i = 12; i < parts.size(); i++) {
		auto& one = parts[i];
		if (one.size() < 4) {
			size += one.size();
		} else {
			size += 1 + one.size();
		}
	}
	if (size >= (1U<<30U)) {
		return {};
	}
	out.reserve(size);
	out.push_back(head);
	out.resize(1+section*2, 0);

	auto blk = Cast<uint64_t>(out.data()+1);
	for (unsigned i = 12; i < parts.size(); ) {
		auto next = i + 25;
		if (next > parts.size()) {
			next = parts.size();
		}
		if (cnt >= (1U<<14U)) {
			return {};
		}
		auto mark = static_cast<uint64_t>(cnt) << 50U;
		for (unsigned j = 0; i < next; j+=2) {
			auto& one = parts[i++];
			if (one.size() < 4) {
				mark |= static_cast<uint64_t>(one.size()) << j;
				cnt += one.size();
			} else {
				mark |= 1ULL << j;
				cnt += 1;
			}
		}
		*blk++ = mark;
	}

	size_t off = out.size();
	for (auto& one : parts) {
		if (one.empty()) {
			continue;
		}
		if (one.size() < 4) {
			out += one;
		} else {
			out.push_back(0);
		}
	}
	for (auto& one : parts) {
		if (one.size() < 4) {
			off += one.size();
		} else {
			out[off] = Offset(out.size()-off);
			out += one;
			off++;
		}
	}
	assert(out.size() == size);
	return out;
}

static size_t BestArraySize(const std::vector<Data>& parts, unsigned& m) {
	size_t sizes[3] = {0, 0, 0};
	for (auto one : parts) {
		sizes[0] += 1;
		sizes[1] += 2;
		sizes[2] += 3;
		if (one.size() <= 1) {
			continue;
		}
		sizes[0] += one.size();
		if (one.size() <= 2) {
			continue;
		}
		sizes[1] += one.size();
		if (one.size() <= 3) {
			continue;
		}
		sizes[2] += one.size();
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

Data SerializeArray(const std::vector<Data>& elements) {
	unsigned m = 0;
	size_t size = 1 + BestArraySize(elements, m);
	if (size >= (1U<<30U)) {
		return {};
	}
	Data out;
	out.reserve(size);
	out.push_back((elements.size() << 2U) | m);

	for (auto& one : elements) {
		auto next = out.size() + m;
		if (one.size() <= m) {
			out += one;
		}
		out.resize(next, 0);
	}
	unsigned off = 1;
	for (auto& one : elements) {
		if (one.size() > m) {
			out[off] = Offset(out.size()-off);
			out += one;
		}
		off += m;
	}
	assert(out.size() == size);
	return out;
}

Data SerializeMap(const Slice<uint8_t>& index,
				  const std::vector<Data>& keys, const std::vector<Data>& values) {
	auto index_size = WordSize(index.size());

	unsigned m1 = 0;
	auto key_size = BestArraySize(keys, m1);
	unsigned m2 = 0;
	auto value_size = BestArraySize(values, m2);

	auto size = index_size + key_size + value_size;
	if (size >= (1U<<30U)) {
		return {};
	}
	Data out;
	out.reserve(size);
	out.resize(index_size, 0);
	memcpy(const_cast<uint32_t*>(out.data()), index.data(), index.size());
	out[0] |= (m1<<30U) | (m2<<28U);

	for (unsigned i = 0; i < keys.size(); i++) {
		auto next = out.size() + m1;
		if (keys[i].size() <= m1) {
			out += keys[i];
		}
		out.resize(next, 0);
		next = out.size() + m2;
		if (values[i].size() <= m2) {
			out += values[i];
		}
		out.resize(next, 0);
	}
	unsigned off = index_size;
	for (unsigned i = 0; i < keys.size(); i++) {
		if (keys[i].size() > m1) {
			out[off] = Offset(out.size()-off);
			out += keys[i];
		}
		off += m1;
		if (values[i].size() > m2) {
			out[off] = Offset(out.size()-off);
			out += values[i];
		}
		off += m2;
	}
	assert(out.size() == size);
	return out;
}

} // protocache