#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_test_proto
#define PROTOCACHE_INCLUDED_EX_test_proto

#include <cstdint>
#include <string>
#include <new>
#include <memory>
#include <bitset>
#include <vector>
#include <unordered_map>
#include <protocache/access-ex.h>


namespace test {

struct Small_EX final {
	Small_EX() = default;
	Small_EX(const uint32_t* data, const uint32_t* end);
	explicit Small_EX(const protocache::Slice<uint32_t>& data) : Small_EX(data.begin(), data.end()) {};
	explicit Small_EX(const protocache::Data& data) : Small_EX(protocache::Slice<uint32_t>(data)) {};

	protocache::Data Serialize() const;

	int32_t i32 = 0;
	bool flag = false;
	std::string str;
};

struct Vec2D_EX {
	struct Vec1D_EX {
		using ALIAS = protocache::ArrayEX<float>;
	};

	using ALIAS = protocache::ArrayEX<::test::Vec2D_EX::Vec1D_EX::ALIAS>;
};

struct ArrMap_EX {
	struct Array_EX {
		using ALIAS = protocache::ArrayEX<float>;
	};

	using ALIAS = protocache::MapEX<protocache::Slice<char>,::test::ArrMap_EX::Array_EX::ALIAS>;
};

struct Main_EX final {
	Main_EX() = default;
	Main_EX(const uint32_t* data, const uint32_t* end);
	explicit Main_EX(const protocache::Slice<uint32_t>& data) : Main_EX(data.begin(), data.end()) {};
	explicit Main_EX(const protocache::Data& data) : Main_EX(protocache::Slice<uint32_t>(data)) {};

	protocache::Data Serialize() const; //TODO

	int32_t i32 = 0;
	uint32_t u32 = 0;
	int64_t i64 = 0;
	uint64_t u64 = 0;
	bool flag = false;
	protocache::EnumValue mode = 0;
	std::string str;
	std::string data;
	float f32 = 0;
	double f64 = 0;
	Small_EX object;
	protocache::ArrayEX<int32_t> i32v;
	protocache::ArrayEX<uint64_t> u64v;
	protocache::ArrayEX<protocache::Slice<char>> strv;
	protocache::ArrayEX<protocache::Slice<uint8_t>> datav;
	protocache::ArrayEX<float> f32v;
	protocache::ArrayEX<double> f64v;
	protocache::ArrayEX<bool> flags;
	protocache::ArrayEX<::test::Small_EX> objectv;
	uint32_t t_u32 = 0;
	int32_t t_i32 = 0;
	int32_t t_s32 = 0;
	uint64_t t_u64 = 0;
	int64_t t_i64 = 0;
	int64_t t_s64 = 0;
	protocache::MapEX<protocache::Slice<char>,int32_t> index;
	protocache::MapEX<int32_t,::test::Small_EX> objects;
	Vec2D_EX::ALIAS matrix;
	protocache::ArrayEX<::test::ArrMap_EX::ALIAS> vector;
	ArrMap_EX::ALIAS arrays;
};

} // test
#endif // PROTOCACHE_INCLUDED_EX_test_proto