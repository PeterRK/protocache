// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <algorithm>
#include "hash.h"
#include "perfect_hash.h"

namespace protocache {

static inline unsigned Bit2(const uint8_t* vec, size_t pos) noexcept {
	return (vec[pos>>2] >> ((pos&3)<<1)) & 3;
}

static inline void SetBit2on11(uint8_t* vec, size_t pos, uint8_t val) noexcept {
	vec[pos>>2] ^= ((~val & 3) << ((pos&3)<<1));
}

static inline unsigned CountValidSlot(uint64_t block) noexcept {
	block &= (block>>1);
	block = ~block & 0x5555555555555555UL;
	return __builtin_popcountll(block);
}

static inline void SetBit(uint8_t bitmap[], size_t pos) noexcept {
	bitmap[pos>>3U] |= (1U<<(pos&7U));
}

static inline bool TestAndSetBit(uint8_t bitmap[], size_t pos) noexcept {
	auto& b = bitmap[pos>>3U];
	uint8_t m = 1U<<(pos&7U);
	if ((b & m) != 0) {
		return false;
	}
	b |= m;
	return true;
}

static inline uint32_t Section(uint32_t size) noexcept {
	return std::max(10UL, (size*105UL + 255U) / 256U);
}

static inline uint32_t BitmapSize(uint32_t section) noexcept {
	return ((section*3U + 31U) & ~31U) / 4U;
}

static inline uint32_t RealSize(uint32_t size) noexcept {
	return size & 0xfffffffU;
}

uint32_t PerfectHash::Size() const noexcept {
	if (data_ == nullptr) {
		return 0;
	}
	return RealSize(*reinterpret_cast<const uint32_t*>(data_));
}

struct Header {
	uint32_t size;
	uint32_t seed;
};

PerfectHash::PerfectHash(const uint8_t* data, uint32_t size) noexcept {
	if (size != 0 && size < 4) {
		return;
	}
	auto header = reinterpret_cast<const Header*>(data);
	if (RealSize(header->size) > 1) {
		if (size != 0 && size < sizeof(Header)) {
			return;
		}
		auto section = Section(RealSize(header->size));
		auto bytes = BitmapSize(section);
		if (RealSize(header->size) > UINT16_MAX) {
			bytes += bytes / 2;
		} else if (RealSize(header->size) > UINT8_MAX) {
			bytes += bytes / 4;
		} else if (RealSize(header->size) > 24) { // 3*Section(RealSize(header->Size)) > 32
			bytes += bytes / 8;
		}
		bytes += sizeof(Header);
		if (size != 0 && size < bytes) {
			return;
		}
		size = bytes;
		section_ = section;
	} else {
		size = 4;
	}
	data_ = data;
	data_size_ = size;
}

uint32_t PerfectHash::Locate(const uint8_t* key, unsigned key_len) const noexcept {
	if (data_ == nullptr) {
		return UINT32_MAX;
	}
	auto header = reinterpret_cast<const Header*>(data_);
	if (RealSize(header->size) < 2) {
		return RealSize(header->size) == 0? UINT32_MAX : 0;
	}
	auto code = Hash128(key, key_len, header->seed);
	uint32_t slots[3] = {
			code.u32[0] % section_,
			code.u32[1] % section_ + section_,
			code.u32[2] % section_ + section_ * 2,
	};
	auto bitmap = data_ + sizeof(Header);
	auto table = bitmap + BitmapSize(section_);
	auto m = Bit2(bitmap,slots[0]) +
			Bit2(bitmap,slots[1]) + Bit2(bitmap,slots[2]);

	auto slot = slots[m % 3];
	uint32_t a = slot >> 5;
	uint32_t b = slot & 31U;

	uint32_t off = 0;
	if (RealSize(header->size) > UINT16_MAX) {
		off = reinterpret_cast<const uint32_t*>(table)[a];
	} else if (RealSize(header->size) > UINT8_MAX) {
		off = reinterpret_cast<const uint16_t*>(table)[a];
	} else if (RealSize(header->size) > 24) { // 3*Section(RealSize(header->Size)) > 32
		off = reinterpret_cast<const uint8_t*>(table)[a];
	}

	auto block = reinterpret_cast<const uint64_t*>(bitmap)[a];
	block |= 0xffffffffffffffffUL << (b<<1);
	return off + CountValidSlot(block);
}

template <typename Word>
struct Vertex {
	Word slot;
	Word next;
};

template <typename Word>
using Edge = Vertex<Word>[3];

template <typename Word>
struct Graph {
	Edge<Word>* edges = nullptr;
	Word* nodes = nullptr;
	uint8_t* sizes = nullptr;
};

template <typename Word>
struct Queue {
	Word* data = nullptr;
	unsigned head = 0;
	unsigned tail = 0;
};

template <typename Word>
static int CreateGraph(KeyReader& source, uint32_t seed, Graph<Word>& g, uint32_t n) noexcept {
	auto m = Section(n);
	auto slot_cnt = m * 3;
	source.Reset();
	memset(g.nodes, ~0, slot_cnt*sizeof(Word));
	memset(g.sizes, 0, slot_cnt);
	for (uint32_t i = 0; i < n; i++) {
		auto key = source.Read();
		if (!key) {
			return -1;
		}
		auto code = Hash128(key.data(), key.size(), seed);
		uint32_t slots[3] = {
				code.u32[0] % m,
				code.u32[1] % m + m,
				code.u32[2] % m + m * 2,
		};
		auto& edge = g.edges[i];
		for (unsigned j = 0; j < 3; j++) {
			auto& v = edge[j];
			v.slot = slots[j];
			v.next = g.nodes[v.slot];
			g.nodes[v.slot] = i;
			if (++g.sizes[v.slot] > 50) {
				return 1;
			}
		}
	}

	return 0;
}

template <typename Word>
static void TearGraph(Graph<Word>& g, uint32_t n, Queue<Word>& q, uint8_t* book) noexcept {
	constexpr Word END = ~0;
	memset(book, 0, (n+7)/8);
	q.head = 0;
	q.tail = 0;
	for (uint32_t i = n; i > 0;) {
		auto &edge = g.edges[--i];
		for (unsigned j = 0; j < 3; j++) {
			auto& v = edge[j];
			if (g.sizes[v.slot] == 1 && TestAndSetBit(book, i)) {
				assert(g.nodes[v.slot] == i);
				q.data[q.tail++] = i;
			}
		}
	}
	while (q.head < q.tail) {
		auto curr = q.data[q.head++];
		auto& edge = g.edges[curr];
		for (unsigned j = 0; j < 3; j++) {
			auto& v = edge[j];
			auto p = &g.nodes[v.slot];
			while (*p != curr) {
				assert(*p != END);
				p = &g.edges[*p][j].next;
			}
			*p = v.next;
			v.next = END;
			auto i = g.nodes[v.slot];
			auto& size = g.sizes[v.slot];
			if (--size == 1 && TestAndSetBit(book, i)) {
				q.data[q.tail++] = i;
			}
		}
	}
}

template <typename Word>
static void Mapping(Graph<Word>& g, uint32_t n, Queue<Word>& q, uint8_t* book, uint8_t* bitmap) noexcept {
	auto m = Section(n);
	memset(bitmap, ~0, BitmapSize(m));
	m *= 3;
	memset(book, 0, (m+7)/8);
	for (unsigned i = q.tail; i > 0; ) {
		auto idx = q.data[--i];
		auto& edge = g.edges[idx];
		auto a = edge[0].slot;
		auto b = edge[1].slot;
		auto c = edge[2].slot;
		if (TestAndSetBit(book, a)) {
			SetBit(book, b);
			SetBit(book, c);
			auto sum = Bit2(bitmap,b) + Bit2(bitmap,c);
			SetBit2on11(bitmap, a, (6-sum)%3);
		} else if (TestAndSetBit(book, b)) {
			SetBit(book, c);
			auto sum = Bit2(bitmap,a) + Bit2(bitmap,c);
			SetBit2on11(bitmap, b, (7-sum)%3);
		} else if (TestAndSetBit(book, c)) {
			auto sum = Bit2(bitmap,a) + Bit2(bitmap,b);
			SetBit2on11(bitmap, c, (8-sum)%3);
		} else {
			assert(false);
		}
	}
}

template <typename Word>
static int Build(KeyReader& source, uint32_t n, uint32_t seed,
				  Graph<Word>& g, Queue<Word>& q, uint8_t* book, uint8_t* bitmap) noexcept {
#ifndef NDEBUG
	printf("try with seed %08x\n", seed);
#endif
	auto ret = CreateGraph(source, seed, g, n);
	if (ret != 0) {
		return ret;
	}
	TearGraph(g, n,q, book);
	if (q.tail != n) {
		return 2;
	}
	Mapping(g, n,q, book, bitmap);
	return 0;
}

static inline bool operator==(const V128& a, const V128& b) noexcept {
	return a.u64[0] == b.u64[0] && a.u64[1] == b.u64[1];
}

static bool Check(KeyReader& source, uint32_t n, uint32_t seed, V128* space) {
	std::unique_ptr<V128[]> temp;
	auto m = n*2U;
	if (space == nullptr) {
		temp.reset(new V128[m]);
		space = temp.get();
	}
	memset(space, 0, m*sizeof(V128));
	source.Reset();

	auto insert = [space, n, m](V128 code, uint32_t pos) -> bool {
		code.u32[0] |= 1;
		for (unsigned j = 0; j < n; j++) {
			if ((space[pos].u32[0]&1) == 0) {
				space[pos] = code;
				return true;
			}
			if (space[pos] == code) {
				return false;
			}
			if (++pos >= m) {
				pos = 0;
			}
		}
		return false;
	};

	for (uint32_t i = 0; i < n; i++) {
		auto key = source.Read();
		if (!key) {
			return false;
		}
		auto code = Hash128(key.data(), key.size(), seed);
		auto pos = code.u32[0] % m;
		if (!insert(code, pos)) {
			return false;
		}
	}
	return true;
}



template <typename Word>
static std::unique_ptr<uint8_t[]> Build(KeyReader& source, uint32_t& data_size, bool no_check) {
	auto total = source.Total();
	auto section = Section(total);
	auto bmsz = BitmapSize(section);
	auto bytes = sizeof(Header) + bmsz;
	if (bmsz > 8) {
	  bytes += (bmsz/8) * sizeof(Word);
	}
	std::unique_ptr<uint8_t[]> out(new uint8_t[bytes]);
	auto header = reinterpret_cast<Header*>(out.get());
	auto bitmap = out.get() + sizeof(Header);
	auto table = reinterpret_cast<Word*>(bitmap + bmsz);
	header->size = total;

	auto slot_cnt = section * 3;

	auto tmp_sz = sizeof(Edge<Word>)*total;
	tmp_sz += sizeof(Word)*total;
	tmp_sz += sizeof(Word)*slot_cnt + slot_cnt;
	tmp_sz += (slot_cnt+7)/8;
	std::unique_ptr<uint8_t[]> temp(new uint8_t[tmp_sz]);

	Graph<Word> graph;
	Queue<Word> queue;
	auto pt = temp.get();
	graph.edges = reinterpret_cast<Edge<Word>*>(pt);
	pt += sizeof(Edge<Word>)*total;
	queue.data = reinterpret_cast<Word*>(pt);
	pt += sizeof(Word)*total;
	graph.nodes = reinterpret_cast<Word*>(pt);
	pt += sizeof(Word)*slot_cnt;
	graph.sizes = pt;
	pt += slot_cnt;
	auto book = pt;

	constexpr unsigned FIRST_TRIES = sizeof(Word) == 1? 8U : 4U;
	constexpr unsigned SECOND_TRIES = sizeof(Word) == 1? 32U : 12U;

	XorShift rand32;
	for (unsigned i = 0; i < FIRST_TRIES; i++) {
		header->seed = rand32.Next();
		auto ret = Build(source, header->size, header->seed, graph, queue, book, bitmap);
		if (ret == 0) {
			goto L_done;
		} else if (ret < 0) {
			return nullptr;
		}
	}

	if (!no_check) {
		V128* space = nullptr;
		if (tmp_sz / sizeof(V128) > header->size*2U) {
			space = reinterpret_cast<V128*>(temp.get());
		}
		if (!Check(source, header->size, header->seed, space)) {
			return nullptr;
		}
	}

	for (unsigned i = 0; i < SECOND_TRIES; i++) {
		header->seed = rand32.Next();
		auto ret = Build(source, header->size, header->seed, graph, queue, book, bitmap);
		if (ret == 0) {
			goto L_done;
		} else if (ret < 0) {
			return nullptr;
		}
	}
	return nullptr;

L_done:
	if (bmsz > 8) {
		Word cnt = 0;
		for (unsigned i = 0; i < bmsz/8; i++) {
			table[i] = cnt;
			cnt += CountValidSlot(reinterpret_cast<const uint64_t*>(bitmap)[i]);
		}
		assert(cnt == total);
	}
	data_size = bytes;
	return out;
}

PerfectHash PerfectHash::Build(KeyReader& source, bool no_check) {
	PerfectHash out;
	auto total = source.Total();
	if (total >= (1U << 28U)) {
		return out;
	}
	if (total > UINT16_MAX) {
		out.buffer_ = ::protocache::Build<uint32_t>(source, out.data_size_, no_check);
	} else if (total > UINT8_MAX) {
		out.buffer_ = ::protocache::Build<uint16_t>(source, out.data_size_, no_check);
	} else if (total > 1) {
		out.buffer_ = ::protocache::Build<uint8_t>(source, out.data_size_, no_check);
	} else {
		out.buffer_.reset(new uint8_t[4]);
		*reinterpret_cast<uint32_t*>(out.buffer_.get()) = total;
		out.data_size_ = 4;
	}
	if (out.buffer_ != nullptr) {
		out.data_ = out.buffer_.get();
	}
	if (out.data_size_ > sizeof(Header)) {
		auto header = reinterpret_cast<const Header*>(out.buffer_.get());
		out.section_ = Section(header->size);
	}
	return out;
}

} //protocache
