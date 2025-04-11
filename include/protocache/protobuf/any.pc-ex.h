#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_any_proto
#define PROTOCACHE_INCLUDED_EX_any_proto

#include <protocache/access-ex.h>
#include "any.pc.h"

namespace ex {
namespace google {
namespace protobuf {

class Any;

struct Any final {
	Any() = default;
	explicit Any(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Any(const protocache::Slice<uint32_t>& data) : Any(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::Any::Detect(ptr, end);
	}
	bool Serialize(protocache::Buffer* buf, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			buf->Put(Detect(clean_head, end));
			return true;
		}
		std::vector<protocache::Buffer::Seg> parts(2, {0,0});
		auto tail = buf->Size();
		if (!__view__.SerializeField(_::value, end, _value, buf, parts[_::value])) return false;
		if (!__view__.SerializeField(_::type_url, end, _type_url, buf, parts[_::type_url])) return false;
		return protocache::SerializeMessage(parts, *buf, tail);
	}

	std::string& type_url(const uint32_t* end=nullptr) { return __view__.GetField(_::type_url, end, _type_url); }
	std::string& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::Any::_;
	protocache::MessageEX<2> __view__;
	std::string _type_url;
	std::string _value;
};

} // protobuf
} // google
} //ex
#endif // PROTOCACHE_INCLUDED_any_proto
