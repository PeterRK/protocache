#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_random_small_proto
#define PROTOCACHE_INCLUDED_EX_random_small_proto

#include <protocache/access-ex.h>
#include "random-small.pc.h"

namespace ex {

class Dummy;

struct Dummy final {
	Dummy() = default;
	explicit Dummy(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Dummy(const protocache::Slice<uint32_t>& data) : Dummy(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::Dummy::Detect(ptr, end);
	}
	protocache::Data Serialize(const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			return {view.data(), view.size()};
		}
		std::vector<protocache::Data> raw(2);
		std::vector<protocache::Slice<uint32_t>> parts(2);
		parts[_::ivec] = __view__.SerializeField(_::ivec, end, _ivec, raw[_::ivec]);
		parts[_::imap] = __view__.SerializeField(_::imap, end, _imap, raw[_::imap]);
		return protocache::SerializeMessage(parts);
	}

	protocache::ArrayEX<int32_t>& ivec(const uint32_t* end=nullptr) { return __view__.GetField(_::ivec, end, _ivec); }
	protocache::MapEX<protocache::Slice<char>,int64_t>& imap(const uint32_t* end=nullptr) { return __view__.GetField(_::imap, end, _imap); }

private:
	using _ = ::Dummy::_;
	protocache::MessageEX<2> __view__;
	protocache::ArrayEX<int32_t> _ivec;
	protocache::MapEX<protocache::Slice<char>,int64_t> _imap;
};

} //ex
#endif // PROTOCACHE_INCLUDED_random_small_proto
