#pragma once
#ifndef PROTOCACHE_INCLUDED_random_small_proto
#define PROTOCACHE_INCLUDED_random_small_proto

#include <protocache/access.h>


class Dummy final {
private:
	protocache::Message core_;
public:
	struct _ {
		static constexpr unsigned ivec = 0;
		static constexpr unsigned imap = 1;
	};

	explicit Dummy(const protocache::Message& message) : core_(message) {}
	explicit Dummy(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}
	bool operator!() const noexcept { return !core_; }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return core_.HasField(id,end); }

	protocache::ArrayT<int32_t> ivec(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<int32_t>(core_, _::ivec, end);
	}
	protocache::MapT<protocache::Slice<char>,int64_t> imap(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetMap<protocache::Slice<char>,int64_t>(core_, _::imap, end);
	}
};

#endif // PROTOCACHE_INCLUDED_random_small_proto
