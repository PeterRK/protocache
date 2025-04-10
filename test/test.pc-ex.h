#pragma once
#ifndef PROTOCACHE_INCLUDED_EX_test_proto
#define PROTOCACHE_INCLUDED_EX_test_proto

#include <protocache/access-ex.h>
#include "test.pc.h"

namespace ex {
namespace test {

class Small;
class Main;
class CyclicA;
class CyclicB;
class Deprecated;

struct Small final {
	Small() = default;
	explicit Small(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit Small(const protocache::Slice<uint32_t>& data) : Small(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::test::Small::Detect(ptr, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(4);
		std::vector<protocache::Slice<uint32_t>> parts(4);
		if (!__view__.SerializeField(_::i32, end, _i32, raw[_::i32], parts[_::i32])) return false;
		if (!__view__.SerializeField(_::flag, end, _flag, raw[_::flag], parts[_::flag])) return false;
		if (!__view__.SerializeField(_::str, end, _str, raw[_::str], parts[_::str])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	int32_t& i32(const uint32_t* end=nullptr) { return __view__.GetField(_::i32, end, _i32); }
	bool& flag(const uint32_t* end=nullptr) { return __view__.GetField(_::flag, end, _flag); }
	std::string& str(const uint32_t* end=nullptr) { return __view__.GetField(_::str, end, _str); }

private:
	using _ = ::test::Small::_;
	protocache::MessageEX<4> __view__;
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
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(30);
		std::vector<protocache::Slice<uint32_t>> parts(30);
		if (!__view__.SerializeField(_::i32, end, _i32, raw[_::i32], parts[_::i32])) return false;
		if (!__view__.SerializeField(_::u32, end, _u32, raw[_::u32], parts[_::u32])) return false;
		if (!__view__.SerializeField(_::i64, end, _i64, raw[_::i64], parts[_::i64])) return false;
		if (!__view__.SerializeField(_::u64, end, _u64, raw[_::u64], parts[_::u64])) return false;
		if (!__view__.SerializeField(_::flag, end, _flag, raw[_::flag], parts[_::flag])) return false;
		if (!__view__.SerializeField(_::mode, end, _mode, raw[_::mode], parts[_::mode])) return false;
		if (!__view__.SerializeField(_::str, end, _str, raw[_::str], parts[_::str])) return false;
		if (!__view__.SerializeField(_::data, end, _data, raw[_::data], parts[_::data])) return false;
		if (!__view__.SerializeField(_::f32, end, _f32, raw[_::f32], parts[_::f32])) return false;
		if (!__view__.SerializeField(_::f64, end, _f64, raw[_::f64], parts[_::f64])) return false;
		if (!__view__.SerializeField(_::object, end, _object, raw[_::object], parts[_::object])) return false;
		if (!__view__.SerializeField(_::i32v, end, _i32v, raw[_::i32v], parts[_::i32v])) return false;
		if (!__view__.SerializeField(_::u64v, end, _u64v, raw[_::u64v], parts[_::u64v])) return false;
		if (!__view__.SerializeField(_::strv, end, _strv, raw[_::strv], parts[_::strv])) return false;
		if (!__view__.SerializeField(_::datav, end, _datav, raw[_::datav], parts[_::datav])) return false;
		if (!__view__.SerializeField(_::f32v, end, _f32v, raw[_::f32v], parts[_::f32v])) return false;
		if (!__view__.SerializeField(_::f64v, end, _f64v, raw[_::f64v], parts[_::f64v])) return false;
		if (!__view__.SerializeField(_::flags, end, _flags, raw[_::flags], parts[_::flags])) return false;
		if (!__view__.SerializeField(_::objectv, end, _objectv, raw[_::objectv], parts[_::objectv])) return false;
		if (!__view__.SerializeField(_::t_u32, end, _t_u32, raw[_::t_u32], parts[_::t_u32])) return false;
		if (!__view__.SerializeField(_::t_i32, end, _t_i32, raw[_::t_i32], parts[_::t_i32])) return false;
		if (!__view__.SerializeField(_::t_s32, end, _t_s32, raw[_::t_s32], parts[_::t_s32])) return false;
		if (!__view__.SerializeField(_::t_u64, end, _t_u64, raw[_::t_u64], parts[_::t_u64])) return false;
		if (!__view__.SerializeField(_::t_i64, end, _t_i64, raw[_::t_i64], parts[_::t_i64])) return false;
		if (!__view__.SerializeField(_::t_s64, end, _t_s64, raw[_::t_s64], parts[_::t_s64])) return false;
		if (!__view__.SerializeField(_::index, end, _index, raw[_::index], parts[_::index])) return false;
		if (!__view__.SerializeField(_::objects, end, _objects, raw[_::objects], parts[_::objects])) return false;
		if (!__view__.SerializeField(_::matrix, end, _matrix, raw[_::matrix], parts[_::matrix])) return false;
		if (!__view__.SerializeField(_::vector, end, _vector, raw[_::vector], parts[_::vector])) return false;
		if (!__view__.SerializeField(_::arrays, end, _arrays, raw[_::arrays], parts[_::arrays])) return false;
		return protocache::SerializeMessage(parts, out);
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
	std::unique_ptr<::ex::test::Small>& object(const uint32_t* end=nullptr) { return __view__.GetField(_::object, end, _object); }
	protocache::ArrayEX<int32_t>& i32v(const uint32_t* end=nullptr) { return __view__.GetField(_::i32v, end, _i32v); }
	protocache::ArrayEX<uint64_t>& u64v(const uint32_t* end=nullptr) { return __view__.GetField(_::u64v, end, _u64v); }
	protocache::ArrayEX<protocache::Slice<char>>& strv(const uint32_t* end=nullptr) { return __view__.GetField(_::strv, end, _strv); }
	protocache::ArrayEX<protocache::Slice<uint8_t>>& datav(const uint32_t* end=nullptr) { return __view__.GetField(_::datav, end, _datav); }
	protocache::ArrayEX<float>& f32v(const uint32_t* end=nullptr) { return __view__.GetField(_::f32v, end, _f32v); }
	protocache::ArrayEX<double>& f64v(const uint32_t* end=nullptr) { return __view__.GetField(_::f64v, end, _f64v); }
	protocache::ArrayEX<bool>& flags(const uint32_t* end=nullptr) { return __view__.GetField(_::flags, end, _flags); }
	protocache::ArrayEX<std::unique_ptr<::ex::test::Small>>& objectv(const uint32_t* end=nullptr) { return __view__.GetField(_::objectv, end, _objectv); }
	uint32_t& t_u32(const uint32_t* end=nullptr) { return __view__.GetField(_::t_u32, end, _t_u32); }
	int32_t& t_i32(const uint32_t* end=nullptr) { return __view__.GetField(_::t_i32, end, _t_i32); }
	int32_t& t_s32(const uint32_t* end=nullptr) { return __view__.GetField(_::t_s32, end, _t_s32); }
	uint64_t& t_u64(const uint32_t* end=nullptr) { return __view__.GetField(_::t_u64, end, _t_u64); }
	int64_t& t_i64(const uint32_t* end=nullptr) { return __view__.GetField(_::t_i64, end, _t_i64); }
	int64_t& t_s64(const uint32_t* end=nullptr) { return __view__.GetField(_::t_s64, end, _t_s64); }
	protocache::MapEX<protocache::Slice<char>,int32_t>& index(const uint32_t* end=nullptr) { return __view__.GetField(_::index, end, _index); }
	protocache::MapEX<int32_t,std::unique_ptr<::ex::test::Small>>& objects(const uint32_t* end=nullptr) { return __view__.GetField(_::objects, end, _objects); }
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
	std::unique_ptr<::ex::test::Small> _object;
	protocache::ArrayEX<int32_t> _i32v;
	protocache::ArrayEX<uint64_t> _u64v;
	protocache::ArrayEX<protocache::Slice<char>> _strv;
	protocache::ArrayEX<protocache::Slice<uint8_t>> _datav;
	protocache::ArrayEX<float> _f32v;
	protocache::ArrayEX<double> _f64v;
	protocache::ArrayEX<bool> _flags;
	protocache::ArrayEX<std::unique_ptr<::ex::test::Small>> _objectv;
	uint32_t _t_u32;
	int32_t _t_i32;
	int32_t _t_s32;
	uint64_t _t_u64;
	int64_t _t_i64;
	int64_t _t_s64;
	protocache::MapEX<protocache::Slice<char>,int32_t> _index;
	protocache::MapEX<int32_t,std::unique_ptr<::ex::test::Small>> _objects;
	::ex::test::Vec2D::ALIAS _matrix;
	protocache::ArrayEX<::ex::test::ArrMap::ALIAS> _vector;
	::ex::test::ArrMap::ALIAS _arrays;
};

struct CyclicA final {
	CyclicA() = default;
	explicit CyclicA(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit CyclicA(const protocache::Slice<uint32_t>& data) : CyclicA(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::test::CyclicA::Detect(ptr, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(2);
		std::vector<protocache::Slice<uint32_t>> parts(2);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		if (!__view__.SerializeField(_::cyclic, end, _cyclic, raw[_::cyclic], parts[_::cyclic])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	int32_t& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }
	std::unique_ptr<::ex::test::CyclicB>& cyclic(const uint32_t* end=nullptr) { return __view__.GetField(_::cyclic, end, _cyclic); }

private:
	using _ = ::test::CyclicA::_;
	protocache::MessageEX<2> __view__;
	int32_t _value;
	std::unique_ptr<::ex::test::CyclicB> _cyclic;
};

struct CyclicB final {
	CyclicB() = default;
	explicit CyclicB(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
	explicit CyclicB(const protocache::Slice<uint32_t>& data) : CyclicB(data.begin(), data.end()) {}
	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		return ::test::CyclicB::Detect(ptr, end);
	}
	bool Serialize(protocache::Data* out, const uint32_t* end=nullptr) const {
		auto clean_head = __view__.CleanHead();
		if (clean_head != nullptr) {
			auto view = Detect(clean_head, end);
			out->assign(view.data(), view.size());
			return true;
		}
		std::vector<protocache::Data> raw(2);
		std::vector<protocache::Slice<uint32_t>> parts(2);
		if (!__view__.SerializeField(_::value, end, _value, raw[_::value], parts[_::value])) return false;
		if (!__view__.SerializeField(_::cyclic, end, _cyclic, raw[_::cyclic], parts[_::cyclic])) return false;
		return protocache::SerializeMessage(parts, out);
	}

	int32_t& value(const uint32_t* end=nullptr) { return __view__.GetField(_::value, end, _value); }
	std::unique_ptr<::ex::test::CyclicA>& cyclic(const uint32_t* end=nullptr) { return __view__.GetField(_::cyclic, end, _cyclic); }

private:
	using _ = ::test::CyclicB::_;
	protocache::MessageEX<2> __view__;
	int32_t _value;
	std::unique_ptr<::ex::test::CyclicA> _cyclic;
};

struct Deprecated final {
	class Valid;

	struct Valid final {
		Valid() = default;
		explicit Valid(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}
		explicit Valid(const protocache::Slice<uint32_t>& data) : Valid(data.begin(), data.end()) {}
		static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
			return ::test::Deprecated::Valid::Detect(ptr, end);
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
			if (!__view__.SerializeField(_::val, end, _val, raw[_::val], parts[_::val])) return false;
			return protocache::SerializeMessage(parts, out);
		}

		int32_t& val(const uint32_t* end=nullptr) { return __view__.GetField(_::val, end, _val); }

	private:
		using _ = ::test::Deprecated::Valid::_;
		protocache::MessageEX<1> __view__;
		int32_t _val;
	};

};

} // test
} //ex
#endif // PROTOCACHE_INCLUDED_test_proto
