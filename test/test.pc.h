#pragma once
#ifndef PROTOCACHE_INCLUDED_test_proto
#define PROTOCACHE_INCLUDED_test_proto

#include <protocache/access.h>

namespace test {

enum Mode : int32_t {
	MODE_A = 0,
	MODE_B = 1,
	MODE_C = 2
};

class Small final {
private:
	protocache::Message core_;
public:
	struct _ {
		static constexpr unsigned i32 = 0;
		static constexpr unsigned flag = 1;
		static constexpr unsigned str = 2;
	};

	explicit Small(const protocache::Message& message) : core_(message) {}
	explicit Small(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}
	bool operator!() const noexcept { return !core_; }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return core_.HasField(id,end); }

	int32_t i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _::i32, end);
	}
	bool flag(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetBool(core_, _::flag, end);
	}
	protocache::Slice<char> str(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetString(core_, _::str, end);
	}
};

struct Vec2D {
	struct Vec1D {
		using ALIAS = protocache::ArrayT<float>;
	};

	using ALIAS = protocache::ArrayT<::test::Vec2D::Vec1D::ALIAS>;
};

struct ArrMap {
	struct Array {
		using ALIAS = protocache::ArrayT<float>;
	};

	using ALIAS = protocache::MapT<protocache::Slice<char>,::test::ArrMap::Array::ALIAS>;
};

class Main final {
private:
	protocache::Message core_;
public:
	struct _ {
		static constexpr unsigned i32 = 0;
		static constexpr unsigned u32 = 1;
		static constexpr unsigned i64 = 2;
		static constexpr unsigned u64 = 3;
		static constexpr unsigned flag = 4;
		static constexpr unsigned mode = 5;
		static constexpr unsigned str = 6;
		static constexpr unsigned data = 7;
		static constexpr unsigned f32 = 8;
		static constexpr unsigned f64 = 9;
		static constexpr unsigned object = 10;
		static constexpr unsigned i32v = 11;
		static constexpr unsigned u64v = 12;
		static constexpr unsigned strv = 13;
		static constexpr unsigned datav = 14;
		static constexpr unsigned f32v = 15;
		static constexpr unsigned f64v = 16;
		static constexpr unsigned flags = 17;
		static constexpr unsigned objectv = 18;
		static constexpr unsigned t_u32 = 19;
		static constexpr unsigned t_i32 = 20;
		static constexpr unsigned t_s32 = 21;
		static constexpr unsigned t_u64 = 22;
		static constexpr unsigned t_i64 = 23;
		static constexpr unsigned t_s64 = 24;
		static constexpr unsigned index = 25;
		static constexpr unsigned objects = 26;
		static constexpr unsigned matrix = 27;
		static constexpr unsigned vector = 28;
		static constexpr unsigned arrays = 29;
	};

	explicit Main(const protocache::Message& message) : core_(message) {}
	explicit Main(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}
	bool operator!() const noexcept { return !core_; }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return core_.HasField(id,end); }

	int32_t i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _::i32, end);
	}
	uint32_t u32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt32(core_, _::u32, end);
	}
	int64_t i64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt64(core_, _::i64, end);
	}
	uint64_t u64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt64(core_, _::u64, end);
	}
	bool flag(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetBool(core_, _::flag, end);
	}
	protocache::EnumValue mode(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetEnumValue(core_, _::mode, end);
	}
	protocache::Slice<char> str(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetString(core_, _::str, end);
	}
	protocache::Slice<uint8_t> data(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetBytes(core_, _::data, end);
	}
	float f32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetFloat(core_, _::f32, end);
	}
	double f64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetDouble(core_, _::f64, end);
	}
	::test::Small object(const uint32_t* end=nullptr) const noexcept {
		return ::test::Small(core_.GetField(_::object, end).GetObject(end), end);
	}
	protocache::ArrayT<int32_t> i32v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<int32_t>(core_, _::i32v, end);
	}
	protocache::ArrayT<uint64_t> u64v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<uint64_t>(core_, _::u64v, end);
	}
	protocache::ArrayT<protocache::Slice<char>> strv(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<protocache::Slice<char>>(core_, _::strv, end);
	}
	protocache::ArrayT<protocache::Slice<uint8_t>> datav(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<protocache::Slice<uint8_t>>(core_, _::datav, end);
	}
	protocache::ArrayT<float> f32v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<float>(core_, _::f32v, end);
	}
	protocache::ArrayT<double> f64v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<double>(core_, _::f64v, end);
	}
	protocache::ArrayT<bool> flags(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<bool>(core_, _::flags, end);
	}
	protocache::ArrayT<::test::Small> objectv(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<::test::Small>(core_, _::objectv, end);
	}
	uint32_t t_u32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt32(core_, _::t_u32, end);
	}
	int32_t t_i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _::t_i32, end);
	}
	int32_t t_s32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _::t_s32, end);
	}
	uint64_t t_u64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt64(core_, _::t_u64, end);
	}
	int64_t t_i64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt64(core_, _::t_i64, end);
	}
	int64_t t_s64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt64(core_, _::t_s64, end);
	}
	protocache::MapT<protocache::Slice<char>,int32_t> index(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetMap<protocache::Slice<char>,int32_t>(core_, _::index, end);
	}
	protocache::MapT<int32_t,::test::Small> objects(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetMap<int32_t,::test::Small>(core_, _::objects, end);
	}
	::test::Vec2D::ALIAS matrix(const uint32_t* end=nullptr) const noexcept {
		return ::test::Vec2D::ALIAS(core_.GetField(_::matrix, end).GetObject(end), end);
	}
	protocache::ArrayT<::test::ArrMap::ALIAS> vector(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<::test::ArrMap::ALIAS>(core_, _::vector, end);
	}
	::test::ArrMap::ALIAS arrays(const uint32_t* end=nullptr) const noexcept {
		return ::test::ArrMap::ALIAS(core_.GetField(_::arrays, end).GetObject(end), end);
	}
};

} // test
#endif // PROTOCACHE_INCLUDED_test_proto
