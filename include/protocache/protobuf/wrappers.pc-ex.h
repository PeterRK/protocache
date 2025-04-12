#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_wrappers_proto
#define PROTOCACHE_INCLUDED_EX_wrappers_proto

#include <protocache/access-ex.h>
#include "wrappers.pc.h"

namespace ex {
namespace google {
namespace protobuf {

class DoubleValue;
class FloatValue;
class Int64Value;
class UInt64Value;
class Int32Value;
class UInt32Value;
class BoolValue;
class StringValue;
class BytesValue;

struct DoubleValue final {
	DoubleValue() = default;
	explicit DoubleValue(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit DoubleValue(const protocache::Slice<uint32_t>& data) : DoubleValue(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::DoubleValue::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	double& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::DoubleValue::_;
	protocache::MessageEX<1> __view__;
	double _value;
};

struct FloatValue final {
	FloatValue() = default;
	explicit FloatValue(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit FloatValue(const protocache::Slice<uint32_t>& data) : FloatValue(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::FloatValue::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	float& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::FloatValue::_;
	protocache::MessageEX<1> __view__;
	float _value;
};

struct Int64Value final {
	Int64Value() = default;
	explicit Int64Value(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Int64Value(const protocache::Slice<uint32_t>& data) : Int64Value(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::Int64Value::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	int64_t& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::Int64Value::_;
	protocache::MessageEX<1> __view__;
	int64_t _value;
};

struct UInt64Value final {
	UInt64Value() = default;
	explicit UInt64Value(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit UInt64Value(const protocache::Slice<uint32_t>& data) : UInt64Value(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::UInt64Value::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	uint64_t& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::UInt64Value::_;
	protocache::MessageEX<1> __view__;
	uint64_t _value;
};

struct Int32Value final {
	Int32Value() = default;
	explicit Int32Value(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Int32Value(const protocache::Slice<uint32_t>& data) : Int32Value(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::Int32Value::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	int32_t& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::Int32Value::_;
	protocache::MessageEX<1> __view__;
	int32_t _value;
};

struct UInt32Value final {
	UInt32Value() = default;
	explicit UInt32Value(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit UInt32Value(const protocache::Slice<uint32_t>& data) : UInt32Value(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::UInt32Value::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	uint32_t& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::UInt32Value::_;
	protocache::MessageEX<1> __view__;
	uint32_t _value;
};

struct BoolValue final {
	BoolValue() = default;
	explicit BoolValue(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit BoolValue(const protocache::Slice<uint32_t>& data) : BoolValue(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::BoolValue::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	bool& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::BoolValue::_;
	protocache::MessageEX<1> __view__;
	bool _value;
};

struct StringValue final {
	StringValue() = default;
	explicit StringValue(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit StringValue(const protocache::Slice<uint32_t>& data) : StringValue(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::StringValue::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	std::string& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::StringValue::_;
	protocache::MessageEX<1> __view__;
	std::string _value;
};

struct BytesValue final {
	BytesValue() = default;
	explicit BytesValue(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit BytesValue(const protocache::Slice<uint32_t>& data) : BytesValue(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::google::protobuf::BytesValue::Detect(ptr, end);
	}
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept {
		return __view__.HasField(id, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(1);
		std::vector<protocache::Slice<uint32_t>> parts(1);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	std::string& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }

private:
	using _ = ::google::protobuf::BytesValue::_;
	protocache::MessageEX<1> __view__;
	std::string _value;
};

} // protobuf
} // google
} //ex
#endif // PROTOCACHE_INCLUDED_wrappers_proto
