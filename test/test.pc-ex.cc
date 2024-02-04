#include "test.pc.h"
#include "test.pc-ex.h"

namespace test {

Small_EX::Small_EX(const uint32_t* _data, const uint32_t* _end) {
	using _ = ::test::Small::_;
	protocache::Message _view(_data, _end);
	protocache::ExtractField(_view, _::i32, _end, &i32);
	protocache::ExtractField(_view, _::flag, _end, &flag);
	protocache::ExtractField(_view, _::str, _end, &str);
}

protocache::Data Small_EX::Serialize() const {
	//TODO
	return {};
}

Main_EX::Main_EX(const uint32_t* _data, const uint32_t* _end) {
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

protocache::Data Main_EX::Serialize() const {
	//TODO
	return {};
}

} // test