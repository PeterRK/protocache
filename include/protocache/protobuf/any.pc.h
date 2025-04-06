#pragma once
#ifndef PROTOCACHE_INCLUDED_any_proto
#define PROTOCACHE_INCLUDED_any_proto

#include <protocache/access.h>

namespace google {
namespace protobuf {

class Any final {
private:
	protocache::Message core_;
public:
	struct _ {
		static constexpr unsigned type_url = 0;
		static constexpr unsigned value = 1;
	};

	explicit Any(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}
	explicit Any(const protocache::Slice<uint32_t>& data) : Any(data.begin(), data.end()) {}
	bool operator!() const noexcept { return !core_; }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return core_.HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<protocache::Slice<uint8_t>>(core, _::value, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::Slice<char>>(core, _::type_url, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	protocache::Slice<char> type_url(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::Slice<char>>(core_, _::type_url, end);
	}
	protocache::Slice<uint8_t> value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::Slice<uint8_t>>(core_, _::value, end);
	}
};

} // protobuf
} // google
#endif // PROTOCACHE_INCLUDED_any_proto
