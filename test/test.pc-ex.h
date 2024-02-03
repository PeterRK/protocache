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
#include "test.pc.h"


namespace test {

class Small_EX final : public protocache::MessageEX<3> {
private:
	using _ = ::test::Small::_;
	struct _Fields {
		int32_t i32 = 0;
		bool flag = false;
		std::string str;
	};
	std::unique_ptr<_Fields> fields_;
	void touch(unsigned id);

public:
	Small_EX() = default;
	explicit Small_EX(const protocache::SharedData& src, size_t offset=0);
	int32_t get_i32() const;
	void set_i32(int32_t val);
	bool get_flag() const;
	void set_flag(bool val);
	protocache::Slice<char> get_str() const;
	void set_str(const protocache::Slice<char>& val);
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

class Main_EX final : public protocache::MessageEX<30> {
private:
	using _ = ::test::Main::_;
	struct _Fields {
		int32_t i32 = 0;
		uint32_t u32 = 0;
		int64_t i64 = 0;
		uint64_t u64 = 0;
		bool flag = false;
		protocache::EnumValue mode = 0;
		std::string str;
		std::basic_string<uint8_t> data;
		float f32 = 0;
		double f64 = 0;
		std::unique_ptr<Small_EX> object;
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
	std::unique_ptr<_Fields> fields_;
	void touch(unsigned id);

public:
	Main_EX() = default;
	explicit Main_EX(const protocache::SharedData& src, size_t offset=0);

	int32_t get_i32() const;
	void set_i32(int32_t val);
	uint32_t get_u32() const;
	void set_u32(uint32_t val);
	int64_t get_i64() const;
	void set_i64(int64_t val);
	uint64_t get_u64() const;
	void set_u64(uint64_t val);
	bool get_flag() const;
	void set_flag(bool val);
	protocache::EnumValue get_mode() const;
	void set_mode(protocache::EnumValue val);
	protocache::Slice<char> get_str() const;
	void set_str(const protocache::Slice<char>& val);
	protocache::Slice<uint8_t> get_data() const;
	void set_data(const protocache::Slice<uint8_t>& val);
	float get_f32() const;
	void set_f32(float val);
	double get_f64() const;
	void set_f64(double val);
	::test::Small_EX* get_object();
	protocache::ArrayEX<int32_t>* get_i32v();
	protocache::ArrayEX<uint64_t>* get_u64v();
	protocache::ArrayEX<protocache::Slice<char>>* get_strv();
	protocache::ArrayEX<protocache::Slice<uint8_t>>* get_datav();
	protocache::ArrayEX<float>* get_f32v();
	protocache::ArrayEX<double>* get_f64v();
	protocache::ArrayEX<bool>* get_flags();
	protocache::ArrayEX<::test::Small_EX>* get_objectv();
	protocache::MapEX<protocache::Slice<char>,int32_t>* get_index();
	protocache::MapEX<int32_t,::test::Small_EX>* get_objects();
	::test::Vec2D_EX::ALIAS* get_matrix();
	protocache::ArrayEX<::test::ArrMap_EX::ALIAS>* get_vector();
	::test::ArrMap_EX::ALIAS* get_arrays();
};

} // test
#endif // PROTOCACHE_INCLUDED_EX_test_proto