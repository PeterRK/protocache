// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <memory>
#include <algorithm>
#ifdef __AVX2__
#include <immintrin.h>
#endif
#include "protocache/perfect_hash.h"
#include "hash.h"

namespace protocache {


//Robison
class Divisor final {
private:
	uint32_t val_ = 0;
	uint32_t fac_ = 0;
	uint32_t tip_ = 0;
	unsigned sft_ = 0;

public:
	uint32_t value() const noexcept { return val_; }
	Divisor() noexcept = default;
	explicit Divisor(uint32_t n) noexcept { *this = n; }

	Divisor& operator=(uint32_t n) noexcept {
		val_ = n;
		fac_ = 0;
		sft_ = 0;
		tip_ = 0;
		if (n == 0) {
			return *this;
		}
		sft_ = 31;
		constexpr uint32_t one = 1;
		auto m = one << sft_;
		for (; m > n; m >>= 1U) {
			sft_--;
		}
		constexpr uint32_t zero = 0;
		fac_ = ~zero;
		tip_ = ~zero;
		if (m == n) {
			return *this;
		}
		fac_ = (((uint64_t)m) << 32) / n;
		uint32_t r = fac_ * n + n;
		if (r <= m) {
			fac_ += 1;
			tip_ = 0;
		} else {
			tip_ = fac_;
		}
		return *this;
	}

	uint32_t div(uint32_t m) const noexcept {
		return (fac_ * (uint64_t)m + tip_) >> (32 + sft_);
	}

	uint32_t mod(uint32_t m) const noexcept {
		return m - val_ * div(m);
	}

#ifdef __AVX2__
	V128 mod(const V128& m) const noexcept {
		auto x = _mm256_zextsi128_si256(_mm_loadu_si128((const __m128i*)m.u32));
		x = _mm256_permutevar8x32_epi32(x, _mm256_set_epi32(7,3,6,2,5,1,4,0));
		auto y = _mm256_mul_epu32(x, _mm256_set1_epi64x(fac_));
		y = _mm256_add_epi64(y, _mm256_set1_epi64x(tip_));
		y = _mm256_srlv_epi64(y, _mm256_set1_epi64x(32 + sft_));
		y = _mm256_mul_epu32(y, _mm256_set1_epi64x(val_));
		auto t = _mm256_set_epi32(7,5,3,1,6,4,2,0);
		x = _mm256_permutevar8x32_epi32(x, t);
		y = _mm256_permutevar8x32_epi32(y, t);
		auto z = _mm_sub_epi32(_mm256_castsi256_si128(x), _mm256_castsi256_si128(y));
		V128 out;
		_mm_storeu_si128((__m128i*)out.u32, z);
		assert(out.u32[0] == mod(m.u32[0]) && out.u32[1] == mod(m.u32[1]) && out.u32[2] == mod(m.u32[2]) && out.u32[3] == mod(m.u32[3]));
		return out;
	}
#endif
};

static inline uint32_t operator/(uint32_t m, const Divisor& d) noexcept {
	return d.div(m);
}

static inline uint32_t operator%(uint32_t m, const Divisor& d) noexcept {
	return d.mod(m);
}

#ifdef __AVX2__
static inline V128 operator%(const V128& m, const Divisor& d) noexcept {
	return d.mod(m);
}
#endif

static inline unsigned Bit2(const uint8_t* vec, size_t pos) noexcept {
	return (vec[pos>>2] >> ((pos&3)<<1)) & 3;
}

static inline void SetBit2on11(uint8_t* vec, size_t pos, uint8_t val) noexcept {
	vec[pos>>2] ^= ((~val & 3) << ((pos&3)<<1));
}

static inline unsigned CountValidSlot(uint64_t v) noexcept {
	v &= (v >> 1);
	v = (v & 0x1111111111111111ULL) + ((v >> 2U) & 0x1111111111111111ULL);
	v = v + (v >> 4U);
	v = v + (v >> 8U);
	v = (v & 0xf0f0f0f0f0f0f0fULL) + ((v >> 16U) & 0xf0f0f0f0f0f0f0fULL);
	v = v + (v >> 32);
	return 32U - (v & 0xffU);
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
	return std::max(10ULL, (size*105ULL + 255U) / 256U);
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
		return 0;
	}
	auto code = Hash128(key, key_len, header->seed);
	uint32_t slots[3] = {
			code.u32[0] % section_,
			code.u32[1] % section_ + section_,
			code.u32[2] % section_ + section_ * 2,
	};
	return Locate(slots);
}

uint32_t PerfectHashObject::Locate(const uint8_t* key, unsigned key_len) const noexcept {
	if (buffer_ == nullptr) {
		return UINT32_MAX;
	}
	auto header = reinterpret_cast<const Header*>(data_);
	if (RealSize(header->size) < 2) {
		return 0;
	}
	auto& m = *reinterpret_cast<Divisor*>(buffer_.get());
	auto code = Hash128(key, key_len, header->seed);
#ifdef __AVX2__
	code = code % m;
	uint32_t slots[3] = {
			code.u32[0],
			code.u32[1] + m.value(),
			code.u32[2] + m.value() * 2,
	};
#else
	uint32_t slots[3] = {
			code.u32[0] % m,
			code.u32[1] % m + m.value(),
			code.u32[2] % m + m.value() * 2,
	};
#endif
	return PerfectHash::Locate(slots);
}

uint32_t PerfectHash::Locate(uint32_t slots[]) const noexcept {
	auto header = reinterpret_cast<const Header*>(data_);
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
	Word prev;
	Word next;
};

template <typename Word>
using Edge = Vertex<Word>[3];

template <typename Word>
struct Graph {
	Edge<Word>* edges = nullptr;
	Word* nodes = nullptr;
};

template <typename Word>
static bool CreateGraph(KeyReader& source, uint32_t seed, Graph<Word>& g, uint32_t n, const Divisor& m) noexcept {
	constexpr Word kEnd = ~0;
	assert(m.value() == Section(n));
	assert(m.value() <= kEnd && n <= kEnd);
	auto slot_cnt = m.value() * 3;
	source.Reset();
	memset(g.nodes, ~0, slot_cnt*sizeof(Word));
	uint32_t shift[3] = { 0, m.value(), m.value()*2 };
	for (uint32_t i = 0; i < n; i++) {
		auto key = source.Read();
		if (!key) {
			return false;
		}
		auto code = Hash128(key.data(), key.size(), seed) % m;
		auto& edge = g.edges[i];
		for (unsigned j = 0; j < 3; j++) {
			auto& v = edge[j];
			v.slot = code.u32[j];
			auto& head = g.nodes[v.slot + shift[j]];
			v.prev = kEnd;
			v.next = head;
			head = i;
			if (v.next != kEnd) {
				g.edges[v.next][j].prev = i;
			}
		}
	}
	return true;
}

template <typename Word>
static bool TearGraph(Graph<Word>& g, uint32_t n, Word free[], uint8_t* book) noexcept {
	constexpr Word kEnd = ~0;
	memset(book, 0, (n+7)/8);
	uint32_t tail = 0;
	for (uint32_t i = n; i > 0;) {
		auto &edge = g.edges[--i];
		for (unsigned j = 0; j < 3; j++) {
			auto& v = edge[j];
			if (v.prev == kEnd && v.next == kEnd
				&& TestAndSetBit(book, i)) {
				free[tail++] = i;
			}
		}
	}
	for (uint32_t head = 0; head < tail; head++) {
		auto curr = free[head];
		auto& edge = g.edges[curr];
		for (unsigned j = 0; j < 3; j++) {
			auto& v = edge[j];	// may be last one
			Word i = kEnd;
			if (v.prev != kEnd) {
				i = v.prev;
				g.edges[i][j].next = v.next;
			}
			if (v.next != kEnd) {
				i = v.next;
				g.edges[i][j].prev = v.prev;
			}
			auto& u = g.edges[i][j];
			if (i != kEnd && u.prev == kEnd && u.next == kEnd
				&& TestAndSetBit(book, i)) {
				free[tail++] = i;
			}
		}
	}
	return tail == n;
}

template <typename Word>
static void Mapping(Graph<Word>& g, uint32_t n, Word free[], uint8_t* book, uint8_t* bitmap) noexcept {
	auto m = Section(n);
	memset(bitmap, ~0, BitmapSize(m));
	memset(book, 0, (m*3+7)/8);
	for (unsigned i = n; i > 0; ) {
		auto idx = free[--i];
		auto& edge = g.edges[idx];
		unsigned a = edge[0].slot;
		unsigned b = edge[1].slot + m;
		unsigned c = edge[2].slot + m*2;
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

static const auto g_time_base = std::chrono::steady_clock::now();

static inline uint32_t GetSeed() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now() - g_time_base).count();
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
	std::unique_ptr<uint8_t[]> out(new uint8_t[bytes+sizeof(Divisor)]);
	auto& divisor = *reinterpret_cast<Divisor*>(out.get());
	divisor = section;
	auto data = out.get() + sizeof(Divisor);
	auto header = reinterpret_cast<Header*>(data);
	auto bitmap = data + sizeof(Header);
	auto table = reinterpret_cast<Word*>(bitmap + bmsz);
	header->size = total;

	auto slot_cnt = section * 3;

	auto tmp_sz = sizeof(Edge<Word>)*total;
	tmp_sz += sizeof(Word)*total;
	tmp_sz += sizeof(Word)*slot_cnt;
	tmp_sz += (slot_cnt+7)/8;
	std::unique_ptr<uint8_t[]> temp(new uint8_t[tmp_sz]);

	Graph<Word> graph;
	auto pt = temp.get();
	graph.edges = reinterpret_cast<Edge<Word>*>(pt);
	pt += sizeof(Edge<Word>)*total;
	auto free = reinterpret_cast<Word*>(pt);
	pt += sizeof(Word)*total;
	graph.nodes = reinterpret_cast<Word*>(pt);
	pt += sizeof(Word)*slot_cnt;
	auto book = pt;

	constexpr unsigned FIRST_TRIES = sizeof(Word) == 1? 8U : 4U;
	constexpr unsigned SECOND_TRIES = sizeof(Word) == 1? 32U : 12U;

	auto build = [&source, n = header->size, &graph,
				  free, book, bitmap, &divisor](uint32_t seed)->bool {
#ifndef NDEBUG
			printf("try with seed %08x\n", seed);
#endif
		if (!CreateGraph(source, seed, graph, n, divisor)
			|| !TearGraph(graph, n, free, book)) {
			return false;
		}
		Mapping(graph, n, free, book, bitmap);
		return true;
	};

	XorShift rand32(GetSeed());
	for (unsigned i = 0; i < FIRST_TRIES; i++) {
		header->seed = rand32.Next();
		if (build(header->seed)) {
			goto L_done;
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
		if (build(header->seed)) {
			goto L_done;
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

PerfectHashObject PerfectHashObject::Build(KeyReader& source, bool no_check) {
	PerfectHashObject out;
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
		out.buffer_.reset(new uint8_t[4+sizeof(Divisor)]);
		*reinterpret_cast<Divisor*>(out.buffer_.get()) = 0;
		*reinterpret_cast<uint32_t*>(out.buffer_.get()+sizeof(Divisor)) = total;
		out.data_size_ = 4;
	}
	if (out.buffer_ != nullptr) {
		out.data_ = out.buffer_.get()+sizeof(Divisor);
	}
	if (out.data_size_ > sizeof(Header)) {
		auto header = reinterpret_cast<const Header*>(out.buffer_.get()+sizeof(Divisor));
		out.section_ = Section(header->size);
	}
	return out;
}

} //protocache
