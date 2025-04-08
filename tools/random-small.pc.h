#pragma once
#ifndef PROTOCACHE_INCLUDED_random_small_proto
#define PROTOCACHE_INCLUDED_random_small_proto

#include <protocache/access.h>


class Dummy;

class Dummy final {
private:
	Dummy() = default;
public:
	struct _ {
		static constexpr unsigned ivec = 0;
		static constexpr unsigned imap = 1;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<protocache::MapT<protocache::Slice<char>,int64_t>>(core, _::imap, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<int32_t>>(core, _::ivec, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	protocache::ArrayT<int32_t> ivec(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<int32_t>>(protocache::Message::Cast(this), _::ivec, end);
	}
	protocache::MapT<protocache::Slice<char>,int64_t> imap(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::MapT<protocache::Slice<char>,int64_t>>(protocache::Message::Cast(this), _::imap, end);
	}
};

#endif // PROTOCACHE_INCLUDED_random_small_proto
