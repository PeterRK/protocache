#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_test_proto
#define PROTOCACHE_INCLUDED_EX_test_proto

#include <protocache/access-ex.h>

namespace ex {
namespace test {

struct Small final {
	Small() = default;
	explicit Small(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Small(const protocache::Slice<uint32_t>& data) : Small(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::test::Small::Detect(ptr, end);
	}
	protocache::Data Serialize(const uint32_t* end=nullptr) const {
		std::vector<protocache::Data> raw(3);
		std::vector<protocache::Slice<uint32_t>> parts(3);
		parts[_::i32] = __view__.SerializeField(_::i32, end, _i32, raw[_::i32]);
		parts[_::flag] = __view__.SerializeField(_::flag, end, _flag, raw[_::flag]);
		parts[_::str] = __view__.SerializeField(_::str, end, _str, raw[_::str]);
		return protocache::SerializeMessage(parts);
	}

	int32_t& i32(const uint32_t* end=nullptr) { return __view__.GetField(_::i32, end, _i32); }
	bool& flag(const uint32_t* end=nullptr) { return __view__.GetField(_::flag, end, _flag); }
	std::string& str(const uint32_t* end=nullptr) { return __view__.GetField(_::str, end, _str); }

private:
	using _ = ::test::Small::_;
	protocache::MessageEX<3> __view__;
	int32_t _i32;
	bool _flag;
	std::string _str;
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
	explicit Main(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Main(const protocache::Slice<uint32_t>& data) : Main(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::test::Main::Detect(ptr, end);
	}
	protocache::Data Serialize(const uint32_t* end=nullptr) const {
		std::vector<protocache::Data> raw(30);
		std::vector<protocache::Slice<uint32_t>> parts(30);
		parts[_::i32] = __view__.SerializeField(_::i32, end, _i32, raw[_::i32]);
		parts[_::u32] = __view__.SerializeField(_::u32, end, _u32, raw[_::u32]);
		parts[_::i64] = __view__.SerializeField(_::i64, end, _i64, raw[_::i64]);
		parts[_::u64] = __view__.SerializeField(_::u64, end, _u64, raw[_::u64]);
		parts[_::flag] = __view__.SerializeField(_::flag, end, _flag, raw[_::flag]);
		parts[_::mode] = __view__.SerializeField(_::mode, end, _mode, raw[_::mode]);
		parts[_::str] = __view__.SerializeField(_::str, end, _str, raw[_::str]);
		parts[_::data] = __view__.SerializeField(_::data, end, _data, raw[_::data]);
		parts[_::f32] = __view__.SerializeField(_::f32, end, _f32, raw[_::f32]);
		parts[_::f64] = __view__.SerializeField(_::f64, end, _f64, raw[_::f64]);
		parts[_::object] = __view__.SerializeField(_::object, end, _object, raw[_::object]);
		parts[_::i32v] = __view__.SerializeField(_::i32v, end, _i32v, raw[_::i32v]);
		parts[_::u64v] = __view__.SerializeField(_::u64v, end, _u64v, raw[_::u64v]);
		parts[_::strv] = __view__.SerializeField(_::strv, end, _strv, raw[_::strv]);
		parts[_::datav] = __view__.SerializeField(_::datav, end, _datav, raw[_::datav]);
		parts[_::f32v] = __view__.SerializeField(_::f32v, end, _f32v, raw[_::f32v]);
		parts[_::f64v] = __view__.SerializeField(_::f64v, end, _f64v, raw[_::f64v]);
		parts[_::flags] = __view__.SerializeField(_::flags, end, _flags, raw[_::flags]);
		parts[_::objectv] = __view__.SerializeField(_::objectv, end, _objectv, raw[_::objectv]);
		parts[_::t_u32] = __view__.SerializeField(_::t_u32, end, _t_u32, raw[_::t_u32]);
		parts[_::t_i32] = __view__.SerializeField(_::t_i32, end, _t_i32, raw[_::t_i32]);
		parts[_::t_s32] = __view__.SerializeField(_::t_s32, end, _t_s32, raw[_::t_s32]);
		parts[_::t_u64] = __view__.SerializeField(_::t_u64, end, _t_u64, raw[_::t_u64]);
		parts[_::t_i64] = __view__.SerializeField(_::t_i64, end, _t_i64, raw[_::t_i64]);
		parts[_::t_s64] = __view__.SerializeField(_::t_s64, end, _t_s64, raw[_::t_s64]);
		parts[_::index] = __view__.SerializeField(_::index, end, _index, raw[_::index]);
		parts[_::objects] = __view__.SerializeField(_::objects, end, _objects, raw[_::objects]);
		parts[_::matrix] = __view__.SerializeField(_::matrix, end, _matrix, raw[_::matrix]);
		parts[_::vector] = __view__.SerializeField(_::vector, end, _vector, raw[_::vector]);
		parts[_::arrays] = __view__.SerializeField(_::arrays, end, _arrays, raw[_::arrays]);
		return protocache::SerializeMessage(parts);
	}

	int32_t& i32(const uint32_t* end=nullptr) { return __view__.GetField(_::i32, end, _i32); }
	uint32_t& u32(const uint32_t* end=nullptr) { return __view__.GetField(_::u32, end, _u32); }
	int64_t& i64(const uint32_t* end=nullptr) { return __view__.GetField(_::i64, end, _i64); }
	uint64_t& u64(const uint32_t* end=nullptr) { return __view__.GetField(_::u64, end, _u64); }
	bool& flag(const uint32_t* end=nullptr) { return __view__.GetField(_::flag, end, _flag); }
	protocache::EnumValue& mode(const uint32_t* end=nullptr) { return __view__.GetField(_::mode, end, _mode); }
	std::string& str(const uint32_t* end=nullptr) { return __view__.GetField(_::str, end, _str); }
	std::string& data(const uint32_t* end=nullptr) { return __view__.GetField(_::data, end, _data); }
	float& f32(const uint32_t* end=nullptr) { return __view__.GetField(_::f32, end, _f32); }
	double& f64(const uint32_t* end=nullptr) { return __view__.GetField(_::f64, end, _f64); }
	::ex::test::Small& object(const uint32_t* end=nullptr) { return __view__.GetField(_::object, end, _object); }
	protocache::ArrayEX<int32_t>& i32v(const uint32_t* end=nullptr) { return __view__.GetField(_::i32v, end, _i32v); }
	protocache::ArrayEX<uint64_t>& u64v(const uint32_t* end=nullptr) { return __view__.GetField(_::u64v, end, _u64v); }
	protocache::ArrayEX<protocache::Slice<char>>& strv(const uint32_t* end=nullptr) { return __view__.GetField(_::strv, end, _strv); }
	protocache::ArrayEX<protocache::Slice<uint8_t>>& datav(const uint32_t* end=nullptr) { return __view__.GetField(_::datav, end, _datav); }
	protocache::ArrayEX<float>& f32v(const uint32_t* end=nullptr) { return __view__.GetField(_::f32v, end, _f32v); }
	protocache::ArrayEX<double>& f64v(const uint32_t* end=nullptr) { return __view__.GetField(_::f64v, end, _f64v); }
	protocache::ArrayEX<bool>& flags(const uint32_t* end=nullptr) { return __view__.GetField(_::flags, end, _flags); }
	protocache::ArrayEX<::ex::test::Small>& objectv(const uint32_t* end=nullptr) { return __view__.GetField(_::objectv, end, _objectv); }
	uint32_t& t_u32(const uint32_t* end=nullptr) { return __view__.GetField(_::t_u32, end, _t_u32); }
	int32_t& t_i32(const uint32_t* end=nullptr) { return __view__.GetField(_::t_i32, end, _t_i32); }
	int32_t& t_s32(const uint32_t* end=nullptr) { return __view__.GetField(_::t_s32, end, _t_s32); }
	uint64_t& t_u64(const uint32_t* end=nullptr) { return __view__.GetField(_::t_u64, end, _t_u64); }
	int64_t& t_i64(const uint32_t* end=nullptr) { return __view__.GetField(_::t_i64, end, _t_i64); }
	int64_t& t_s64(const uint32_t* end=nullptr) { return __view__.GetField(_::t_s64, end, _t_s64); }
	protocache::MapEX<protocache::Slice<char>,int32_t>& index(const uint32_t* end=nullptr) { return __view__.GetField(_::index, end, _index); }
	protocache::MapEX<int32_t,::ex::test::Small>& objects(const uint32_t* end=nullptr) { return __view__.GetField(_::objects, end, _objects); }
	::ex::test::Vec2D::ALIAS& matrix(const uint32_t* end=nullptr) { return __view__.GetField(_::matrix, end, _matrix); }
	protocache::ArrayEX<::ex::test::ArrMap::ALIAS>& vector(const uint32_t* end=nullptr) { return __view__.GetField(_::vector, end, _vector); }
	::ex::test::ArrMap::ALIAS& arrays(const uint32_t* end=nullptr) { return __view__.GetField(_::arrays, end, _arrays); }

private:
	using _ = ::test::Main::_;
	protocache::MessageEX<30> __view__;
	int32_t _i32;
	uint32_t _u32;
	int64_t _i64;
	uint64_t _u64;
	bool _flag;
	protocache::EnumValue _mode;
	std::string _str;
	std::string _data;
	float _f32;
	double _f64;
	::ex::test::Small _object;
	protocache::ArrayEX<int32_t> _i32v;
	protocache::ArrayEX<uint64_t> _u64v;
	protocache::ArrayEX<protocache::Slice<char>> _strv;
	protocache::ArrayEX<protocache::Slice<uint8_t>> _datav;
	protocache::ArrayEX<float> _f32v;
	protocache::ArrayEX<double> _f64v;
	protocache::ArrayEX<bool> _flags;
	protocache::ArrayEX<::ex::test::Small> _objectv;
	uint32_t _t_u32;
	int32_t _t_i32;
	int32_t _t_s32;
	uint64_t _t_u64;
	int64_t _t_i64;
	int64_t _t_s64;
	protocache::MapEX<protocache::Slice<char>,int32_t> _index;
	protocache::MapEX<int32_t,::ex::test::Small> _objects;
	::ex::test::Vec2D::ALIAS _matrix;
	protocache::ArrayEX<::ex::test::ArrMap::ALIAS> _vector;
	::ex::test::ArrMap::ALIAS _arrays;
};

} // test
} //ex
#endif // PROTOCACHE_INCLUDED_test_proto
