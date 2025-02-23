#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_random_small_proto
#define PROTOCACHE_INCLUDED_EX_random_small_proto

#include <protocache/access-ex.h>

namespace ex {

struct Dummy final {
	Dummy() = default;
	Dummy(const uint32_t* data, const uint32_t* end);
	explicit Dummy(const protocache::Slice<uint32_t>& data) : Dummy(data.begin(), data.end()) {}
	protocache::Data Serialize() const;

	protocache::ArrayEX<int32_t> ivec;
	protocache::MapEX<protocache::Slice<char>,int64_t> imap;
};

} //ex
#endif // PROTOCACHE_INCLUDED_random_small_proto
