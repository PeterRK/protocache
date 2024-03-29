#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_test_proto
#define PROTOCACHE_INCLUDED_EX_test_proto

#include <protocache/access-ex.h>

namespace ex {
namespace test {

struct Small final {
	Small() = default;
	Small(const uint32_t* data, const uint32_t* end);
	explicit Small(const protocache::Slice<uint32_t>& data) : Small(data.begin(), data.end()) {}
	protocache::Data Serialize() const;

	int32_t i32;
	bool flag;
	std::string str;
};

struct Vec2D final {
	struct Vec1D final {
		using ALIAS = protocache::ArrayEX<float>;
	};

	using ALIAS = protocache::ArrayEX<::ex::test::Vec2D::Vec1D::ALIAS>;
};

struct ArrMap final {
	struct Array final {
		using ALIAS = protocache::ArrayEX<float>;
	};

	using ALIAS = protocache::MapEX<protocache::Slice<char>,::ex::test::ArrMap::Array::ALIAS>;
};

struct Main final {
	Main() = default;
	Main(const uint32_t* data, const uint32_t* end);
	explicit Main(const protocache::Slice<uint32_t>& data) : Main(data.begin(), data.end()) {}
	protocache::Data Serialize() const;

	int32_t i32;
	uint32_t u32;
	int64_t i64;
	uint64_t u64;
	bool flag;
	protocache::EnumValue mode;
	std::string str;
	std::string data;
	float f32;
	double f64;
	::ex::test::Small object;
	protocache::ArrayEX<int32_t> i32v;
	protocache::ArrayEX<uint64_t> u64v;
	protocache::ArrayEX<protocache::Slice<char>> strv;
	protocache::ArrayEX<protocache::Slice<uint8_t>> datav;
	protocache::ArrayEX<float> f32v;
	protocache::ArrayEX<double> f64v;
	protocache::ArrayEX<bool> flags;
	protocache::ArrayEX<::ex::test::Small> objectv;
	uint32_t t_u32;
	int32_t t_i32;
	int32_t t_s32;
	uint64_t t_u64;
	int64_t t_i64;
	int64_t t_s64;
	protocache::MapEX<protocache::Slice<char>,int32_t> index;
	protocache::MapEX<int32_t,::ex::test::Small> objects;
	::ex::test::Vec2D::ALIAS matrix;
	protocache::ArrayEX<::ex::test::ArrMap::ALIAS> vector;
	::ex::test::ArrMap::ALIAS arrays;
};

} // test
} //ex
#endif // PROTOCACHE_INCLUDED_test_proto
