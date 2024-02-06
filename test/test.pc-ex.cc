#include "test.pc.h"
#include "test.pc-ex.h"

namespace ex {
namespace test {

Small::Small(const uint32_t* _data, const uint32_t* _end) {
	using _ = ::test::Small::_;
	protocache::Message _view(_data, _end);
	protocache::ExtractField(_view, _::i32, _end, &i32);
	protocache::ExtractField(_view, _::flag, _end, &flag);
	protocache::ExtractField(_view, _::str, _end, &str);
}

protocache::Data Small::Serialize() const {
	using _ = ::test::Small::_;
	std::vector<protocache::Data> parts(3);
	if (i32 != 0) {
		parts[_::i32] = protocache::Serialize(i32);
	}
	if (flag) {
		parts[_::flag] = protocache::Serialize(flag);
	}
	if (!str.empty()) {
		parts[_::str] = protocache::Serialize(str);
		if (parts[_::str].empty()) return {};
	}
	return protocache::SerializeMessage(parts);
}

Main::Main(const uint32_t* _data, const uint32_t* _end) {
	using _ = ::test::Main::_;
	protocache::Message _view(_data, _end);
	protocache::ExtractField(_view, _::i32, _end, &i32);
	protocache::ExtractField(_view, _::u32, _end, &u32);
	protocache::ExtractField(_view, _::i64, _end, &i64);
	protocache::ExtractField(_view, _::u64, _end, &u64);
	protocache::ExtractField(_view, _::flag, _end, &flag);
	protocache::ExtractField(_view, _::mode, _end, &mode);
	protocache::ExtractField(_view, _::str, _end, &str);
	protocache::ExtractField(_view, _::data, _end, &data);
	protocache::ExtractField(_view, _::f32, _end, &f32);
	protocache::ExtractField(_view, _::f64, _end, &f64);
	protocache::ExtractField(_view, _::object, _end, &object);
	protocache::ExtractField(_view, _::i32v, _end, &i32v);
	protocache::ExtractField(_view, _::u64v, _end, &u64v);
	protocache::ExtractField(_view, _::strv, _end, &strv);
	protocache::ExtractField(_view, _::datav, _end, &datav);
	protocache::ExtractField(_view, _::f32v, _end, &f32v);
	protocache::ExtractField(_view, _::f64v, _end, &f64v);
	protocache::ExtractField(_view, _::flags, _end, &flags);
	protocache::ExtractField(_view, _::objectv, _end, &objectv);
	protocache::ExtractField(_view, _::t_u32, _end, &t_u32);
	protocache::ExtractField(_view, _::t_i32, _end, &t_i32);
	protocache::ExtractField(_view, _::t_s32, _end, &t_s32);
	protocache::ExtractField(_view, _::t_u64, _end, &t_u64);
	protocache::ExtractField(_view, _::t_i64, _end, &t_i64);
	protocache::ExtractField(_view, _::t_s64, _end, &t_s64);
	protocache::ExtractField(_view, _::index, _end, &index);
	protocache::ExtractField(_view, _::datav, _end, &datav);
	protocache::ExtractField(_view, _::f32v, _end, &f32v);
	protocache::ExtractField(_view, _::f64v, _end, &f64v);
	protocache::ExtractField(_view, _::flags, _end, &flags);
	protocache::ExtractField(_view, _::objectv, _end, &objectv);
	protocache::ExtractField(_view, _::t_u32, _end, &t_u32);
	protocache::ExtractField(_view, _::t_i32, _end, &t_i32);
	protocache::ExtractField(_view, _::t_s32, _end, &t_s32);
	protocache::ExtractField(_view, _::t_u64, _end, &t_u64);
	protocache::ExtractField(_view, _::t_i64, _end, &t_i64);
	protocache::ExtractField(_view, _::t_s64, _end, &t_s64);
	protocache::ExtractField(_view, _::index, _end, &index);
	protocache::ExtractField(_view, _::objects, _end, &objects);
	protocache::ExtractField(_view, _::matrix, _end, &matrix);
	protocache::ExtractField(_view, _::vector, _end, &vector);
	protocache::ExtractField(_view, _::arrays, _end, &arrays);
}

protocache::Data Main::Serialize() const {
	using _ = ::test::Main::_;
	std::vector<protocache::Data> parts(30);
	if (i32 != 0) {
		parts[_::i32] = protocache::Serialize(i32);
	}
	if (u32 != 0) {
		parts[_::u32] = protocache::Serialize(u32);
	}
	if (i64 != 0) {
		parts[_::i64] = protocache::Serialize(i64);
	}
	if (u64 != 0) {
		parts[_::u64] = protocache::Serialize(u64);
	}
	if (flag) {
		parts[_::flag] = protocache::Serialize(flag);
	}
	if (mode != 0) {
		parts[_::mode] = protocache::Serialize(mode);
	}
	if (!str.empty()) {
		parts[_::str] = protocache::Serialize(str);
		if (parts[_::str].empty()) return {};
	}
	if (!data.empty()) {
		parts[_::data] = protocache::Serialize(data);
		if (parts[_::data].empty()) return {};
	}
	if (f32 != 0) {
		parts[_::f32] = protocache::Serialize(f32);
	}
	if (f64 != 0) {
		parts[_::f64] = protocache::Serialize(f64);
	}
	parts[_::object] = protocache::Serialize(object);
	if (parts[_::object].empty()) return {};
	if (parts[_::object].size() <= 1) {
		parts[_::object].clear();
	}
	if (!i32v.empty()) {
		parts[_::i32v] = protocache::Serialize(i32v);
		if (parts[_::i32v].empty()) return {};
	}
	if (!u64v.empty()) {
		parts[_::u64v] = protocache::Serialize(u64v);
		if (parts[_::u64v].empty()) return {};
	}
	if (!strv.empty()) {
		parts[_::strv] = protocache::Serialize(strv);
		if (parts[_::strv].empty()) return {};
	}
	if (!datav.empty()) {
		parts[_::datav] = protocache::Serialize(datav);
		if (parts[_::datav].empty()) return {};
	}
	if (!f32v.empty()) {
		parts[_::f32v] = protocache::Serialize(f32v);
		if (parts[_::f32v].empty()) return {};
	}
	if (!f64v.empty()) {
		parts[_::f64v] = protocache::Serialize(f64v);
		if (parts[_::f64v].empty()) return {};
	}
	if (!flags.empty()) {
		parts[_::flags] = protocache::Serialize(flags);
		if (parts[_::flags].empty()) return {};
	}
	if (!objectv.empty()) {
		parts[_::objectv] = protocache::Serialize(objectv);
		if (parts[_::objectv].empty()) return {};
	}
	if (t_u32 != 0) {
		parts[_::t_u32] = protocache::Serialize(t_u32);
	}
	if (t_i32 != 0) {
		parts[_::t_i32] = protocache::Serialize(t_i32);
	}
	if (t_s32 != 0) {
		parts[_::t_s32] = protocache::Serialize(t_s32);
	}
	if (t_u64 != 0) {
		parts[_::t_u64] = protocache::Serialize(t_u64);
	}
	if (t_i64 != 0) {
		parts[_::t_i64] = protocache::Serialize(t_i64);
	}
	if (t_s64 != 0) {
		parts[_::t_s64] = protocache::Serialize(t_s64);
	}
	if (!index.empty()) {
		parts[_::index] = protocache::Serialize(index);
		if (parts[_::index].empty()) return {};
	}
	if (!objects.empty()) {
		parts[_::objects] = protocache::Serialize(objects);
		if (parts[_::objects].empty()) return {};
	}
	if (!matrix.empty()) {
		parts[_::matrix] = protocache::Serialize(matrix);
		if (parts[_::matrix].empty()) return {};
	}
	if (!vector.empty()) {
		parts[_::vector] = protocache::Serialize(vector);
		if (parts[_::vector].empty()) return {};
	}
	if (!arrays.empty()) {
		parts[_::arrays] = protocache::Serialize(arrays);
		if (parts[_::arrays].empty()) return {};
	}
	return protocache::SerializeMessage(parts);
}

} // test
} // ex