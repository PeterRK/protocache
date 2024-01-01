#pragma once
#ifndef PROTOCACHE_INCLUDED_test_2eproto
#define PROTOCACHE_INCLUDED_test_2eproto

#include <cstdint>
#include "access.h"

namespace test {

enum Mode : int32_t {
	MODE_A = 0,
	MODE_B = 1,
	MODE_C = 2
};

class Small final {
public:
	explicit Small(const protocache::Message& message) : core_(message) {}
	explicit Small(const uint32_t* ptr) : core_(ptr) {}
	bool operator!() const noexcept {
		return !core_;
	}
	int32_t i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _Fields::i32, end);
	}
	bool flag(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetBool(core_, _Fields::flag, end);
	}
	protocache::Slice<char> str(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetString(core_, _Fields::str, end);
	}

private:
	struct _Fields {
		static constexpr unsigned i32 = 0;
		static constexpr unsigned flag = 1;
		static constexpr unsigned str = 2;
	};
	protocache::Message core_;
};

using Vec1D = protocache::ArrayT<float>;
using Vec2D = protocache::ArrayT<Vec1D>;
using VecMap = protocache::MapT<protocache::Slice<char>,Vec1D>;

class Main final {
public:
	explicit Main(const protocache::Message& message) : core_(message) {}
	explicit Main(const uint32_t* ptr) : core_(ptr) {}
	bool operator!() const noexcept {
		return !core_;
	}
	int32_t i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _Fields::i32, end);
	}
	uint32_t u32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt32(core_, _Fields::u32, end);
	}
	int64_t i64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt64(core_, _Fields::i64, end);
	}
	uint64_t u64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt64(core_, _Fields::u64, end);
	}
	bool flag(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetBool(core_, _Fields::flag, end);
	}
	protocache::EnumValue mode(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetEnumValue(core_, _Fields::mode, end);
	}
	protocache::Slice<char> str(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetString(core_, _Fields::str, end);
	}
	protocache::Slice<uint8_t> data(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetBytes(core_, _Fields::data, end);
	}
	float f32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetFloat(core_, _Fields::f32, end);
	}
	double f64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetDouble(core_, _Fields::f64, end);
	}
	Small object(const uint32_t* end=nullptr) const noexcept {
		return Small(protocache::GetMessage(core_, _Fields::object, end));
	}
	protocache::Slice<int32_t> i32v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32Array(core_, _Fields::i32v, end);
	}
	protocache::Slice<uint64_t> u64v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt64Array(core_, _Fields::u64v, end);
	}
	protocache::ArrayT<protocache::Slice<char>> strv(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<protocache::Slice<char>>(core_, _Fields::strv, end);
	}
	protocache::ArrayT<protocache::Slice<uint8_t>> datav(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<protocache::Slice<uint8_t>>(core_, _Fields::datav, end);
	}
	protocache::Slice<float> f32v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetFloatArray(core_, _Fields::f32v, end);
	}
	protocache::Slice<double> f64v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetDoubleArray(core_, _Fields::f64v, end);
	}
	protocache::Slice<bool> flags(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetBoolArray(core_, _Fields::flags, end);
	}
	protocache::ArrayT<Small> objectv(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<Small>(core_, _Fields::objectv, end);
	}
	uint32_t t_u32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt32(core_, _Fields::t_u32, end);
	}
	int32_t t_i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _Fields::t_i32, end);
	}
	int32_t t_s32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt32(core_, _Fields::t_s32, end);
	}
	uint64_t t_u64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetUInt64(core_, _Fields::t_u64, end);
	}
	int64_t t_i64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt64(core_, _Fields::t_i64, end);
	}
	int64_t t_s64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetInt64(core_, _Fields::t_s64, end);
	}
	protocache::MapT<protocache::Slice<char>,int32_t> index(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetMap<protocache::Slice<char>,int32_t>(core_, _Fields::index, end);
	}
	protocache::MapT<int32_t,Small> objects(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetMap<int32_t,Small>(core_, _Fields::objects, end);
	}
	protocache::ArrayT<protocache::ArrayT<float>> matrix(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<protocache::ArrayT<float>>(core_, _Fields::matrix, end);
	}
	protocache::ArrayT<VecMap> vector(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetArray<VecMap>(core_, _Fields::vector, end);
	}

private:
	struct _Fields {
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
	};
	protocache::Message core_;
};

} // test

template<>
inline test::Small protocache::FieldT<test::Small>::Get(const uint32_t *end) const noexcept {
	return test::Small(core_.GetObject(end));
}

template<>
inline test::Main protocache::FieldT<test::Main>::Get(const uint32_t *end) const noexcept {
	return test::Main(core_.GetObject(end));
}

#ifndef PROTOCACHE_V_f32
#define PROTOCACHE_V_f32
template<>
inline test::Vec1D protocache::FieldT<test::Vec1D>::Get(const uint32_t *end) const noexcept {
	return test::Vec1D(protocache::Array(core_.GetObject(end)));
}
#endif // PROTOCACHE_V_f32

#ifndef PROTOCACHE_VV_f32
#define PROTOCACHE_VV_f32
template<>
inline test::Vec2D protocache::FieldT<test::Vec2D>::Get(const uint32_t *end) const noexcept {
	return test::Vec2D(protocache::Array(core_.GetObject(end)));
}
#endif // PROTOCACHE_VV_f32

#ifndef PROTOCACHE_MstrV_f32
#define PROTOCACHE_MstrV_f32
template<>
inline test::VecMap protocache::FieldT<test::VecMap>::Get(const uint32_t *end) const noexcept {
	return test::VecMap(protocache::Map(core_.GetObject(end)));
}
#endif // PROTOCACHE_MstrV_f32

#endif // PROTOCACHE_INCLUDED_test_2eproto