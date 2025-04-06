#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_any_proto
#define PROTOCACHE_INCLUDED_EX_any_proto

#include <protocache/access-ex.h>

namespace ex {
namespace google {
namespace protobuf {

struct Any final {
	Any() = default;
	explicit Any(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Any(const protocache::Slice<uint32_t>& data) : Any(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::Any::Detect(ptr, end);
	}
	protocache::Data Serialize(const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			return {view.data(), view.size()};
		}
		std::vector<protocache::Data> raw(2);
		std::vector<protocache::Slice<uint32_t>> parts(2);
		parts[_::type_url] = __view__.SerializeField(_::type_url, end, _type_url, raw[_::type_url]);
		parts[_::value] = __view__.SerializeField(_::value, end, _value, raw[_::value]);
		return protocache::SerializeMessage(parts);
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
