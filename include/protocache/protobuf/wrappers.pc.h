#pragma once
#ifndef PROTOCACHE_INCLUDED_wrappers_proto
#define PROTOCACHE_INCLUDED_wrappers_proto

#include <protocache/access.h>

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

class DoubleValue final {
private:
	DoubleValue() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		return view;
	}

	double value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<double>(protocache::Message::Cast(this), _::value, end);
	}
};

class FloatValue final {
private:
	FloatValue() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		return view;
	}

	float value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<float>(protocache::Message::Cast(this), _::value, end);
	}
};

class Int64Value final {
private:
	Int64Value() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		return view;
	}

	int64_t value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int64_t>(protocache::Message::Cast(this), _::value, end);
	}
};

class UInt64Value final {
private:
	UInt64Value() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		return view;
	}

	uint64_t value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<uint64_t>(protocache::Message::Cast(this), _::value, end);
	}
};

class Int32Value final {
private:
	Int32Value() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		return view;
	}

	int32_t value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::value, end);
	}
};

class UInt32Value final {
private:
	UInt32Value() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		return view;
	}

	uint32_t value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<uint32_t>(protocache::Message::Cast(this), _::value, end);
	}
};

class BoolValue final {
private:
	BoolValue() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		return view;
	}

	bool value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<bool>(protocache::Message::Cast(this), _::value, end);
	}
};

class StringValue final {
private:
	StringValue() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<protocache::Slice<char>>(core, _::value, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	protocache::Slice<char> value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::Slice<char>>(protocache::Message::Cast(this), _::value, end);
	}
};

class BytesValue final {
private:
	BytesValue() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr, end);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<protocache::Slice<uint8_t>>(core, _::value, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	protocache::Slice<uint8_t> value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::Slice<uint8_t>>(protocache::Message::Cast(this), _::value, end);
	}
};

} // protobuf
} // google
#endif // PROTOCACHE_INCLUDED_wrappers_proto
