#pragma once
#ifndef PROTOCACHE_INCLUDED_duration_proto
#define PROTOCACHE_INCLUDED_duration_proto

#include <protocache/access.h>

namespace google {
namespace protobuf {

class Duration final {
private:
	protocache::Message core_;
public:
	struct _ {
		static constexpr unsigned seconds = 0;
		static constexpr unsigned nanos = 1;
	};

	explicit Duration(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}
	explicit Duration(const protocache::Slice<uint32_t>& data) : Duration(data.begin(), data.end()) {}
	bool operator!() const noexcept { return !core_; }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return core_.HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr);
		protocache::Slice<uint32_t> t;
		return view;
	}

	int64_t seconds(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int64_t>(core_, _::seconds, end);
	}
	int32_t nanos(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(core_, _::nanos, end);
	}
};

} // protobuf
} // google
#endif // PROTOCACHE_INCLUDED_duration_proto
