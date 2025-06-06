#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_timestamp_proto
#define PROTOCACHE_INCLUDED_EX_timestamp_proto

#include <protocache/access-ex.h>
#include "timestamp.pc.h"

namespace ex {
namespace google {
namespace protobuf {

class Timestamp;

struct Timestamp final {
	Timestamp() = default;
	explicit Timestamp(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Timestamp(const protocache::Slice<uint32_t>& data) : Timestamp(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::Timestamp::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Buffer* buf, const uint32_t* end=nullptr) const {
		protocache::Unit dummy;
		return Serialize(*buf, dummy, end);
	}
	bool Serialize(protocache::Buffer& buf, protocache::Unit& unit, const uint32_t* end) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			return protocache::Copy(Detect(clean_head, end), buf, unit);
		}
		std::vector<protocache::Unit> parts(2);
		auto last = buf.Size();
		if (!__view__.SerializeField(_::nanos, end, _nanos, buf, parts[_::nanos])) return false;
		if (!__view__.SerializeField(_::seconds, end, _seconds, buf, parts[_::seconds])) return false;
		return protocache::SerializeMessage(parts, buf, last, unit);
	}

	int64_t& seconds(const uint32_t* end=nullptr) { return __view__.GetField(_::seconds, end, _seconds); }
	int32_t& nanos(const uint32_t* end=nullptr) { return __view__.GetField(_::nanos, end, _nanos); }

private:
	using _ = ::google::protobuf::Timestamp::_;
	protocache::MessageEX<2> __view__;
	int64_t _seconds;
	int32_t _nanos;
};

} // protobuf
} // google
} //ex
#endif // PROTOCACHE_INCLUDED_timestamp_proto
