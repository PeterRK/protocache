#include "test.pc-ex.h"

namespace test {

inline void Small_EX::touch(unsigned id) {
	mask_[id] = true;
	if (fields_ == nullptr) {
		fields_.reset(new _Fields);
	}
}

Small_EX::Small_EX(const protocache::SharedData& src, size_t offset) : MessageEX(src, offset) {}

int32_t Small_EX::get_i32() const {
	if (mask_[_::i32]) { return fields_->i32; }
	return protocache::GetInt32(view_, _::i32,data_->data()+data_->size());
}
void Small_EX::set_i32(int32_t val) {
	touch(_::i32);
	fields_->i32 = val;
}

bool Small_EX::get_flag() const {
	if (mask_[_::flag]) { return fields_->flag; }
	return  protocache::GetBool(view_, _::flag,data_->data()+data_->size());
}
void Small_EX::set_flag(bool val) {
	touch(_::flag);
	fields_->flag = val;
}

protocache::Slice<char> Small_EX::get_str() const {
	if (mask_[_::str]) { return {fields_->str.data(), fields_->str.size()}; }
	return protocache::GetString(view_, _::str,data_->data()+data_->size());
}
void Small_EX::set_str(const protocache::Slice<char>& val) {
	touch(_::str);
	fields_->str.assign(val.begin(), val.end());
}

inline void Main_EX::touch(unsigned id) {
	mask_[id] = true;
	if (fields_ == nullptr) {
		fields_.reset(new _Fields);
	}
}

Main_EX::Main_EX(const protocache::SharedData& src, size_t offset) : MessageEX(src, offset) {}

int32_t Main_EX::get_i32() const {
	if (mask_[_::i32]) { return fields_->i32; }
	return protocache::GetInt32(view_, _::i32,data_->data()+data_->size());
}
void Main_EX::set_i32(int32_t val) {
	touch(_::i32);
	fields_->i32 = val;
}

uint32_t Main_EX::get_u32() const {
	if (mask_[_::u32]) { return fields_->u32; }
	return protocache::GetUInt32(view_, _::u32,data_->data()+data_->size());
}
void Main_EX::set_u32(uint32_t val) {
	touch(_::u32);
	fields_->u32 = val;
}

int64_t Main_EX::get_i64() const {
	if (mask_[_::i64]) { return fields_->i64; }
	return protocache::GetInt64(view_, _::i64,data_->data()+data_->size());
}
void Main_EX::set_i64(int64_t val) {
	touch(_::i64);
	fields_->i64 = val;
}

uint64_t Main_EX::get_u64() const {
	if (mask_[_::u64]) { return fields_->u64; }
	return protocache::GetUInt64(view_, _::u64,data_->data()+data_->size());
}
void Main_EX::set_u64(uint64_t val) {
	touch(_::u64);
	fields_->u64 = val;
}

bool Main_EX::get_flag() const {
	if (mask_[_::flag]) { return fields_->flag; }
	return protocache::GetBool(view_, _::flag,data_->data()+data_->size());
}
void Main_EX::set_flag(bool val) {
	touch(_::flag);
	fields_->flag = val;
}

protocache::EnumValue Main_EX::get_mode() const {
	if (mask_[_::mode]) { return fields_->mode; }
	return protocache::GetEnumValue(view_, _::mode,data_->data()+data_->size());
}
void Main_EX::set_mode(protocache::EnumValue val) {
	touch(_::mode);
	fields_->mode = val;
}

protocache::Slice<char> Main_EX::get_str() const {
	if (mask_[_::str]) { return {fields_->str.data(), fields_->str.size()}; }
	return protocache::GetString(view_, _::str,data_->data()+data_->size());
}
void Main_EX::set_str(const protocache::Slice<char>& val) {
	touch(_::str);
	fields_->str.assign(val.begin(), val.end());
}

protocache::Slice<uint8_t> Main_EX::get_data() const {
	if (mask_[_::data]) { return {fields_->data.data(), fields_->data.size()}; }
	return protocache::GetBytes(view_, _::data,data_->data()+data_->size());
}
void Main_EX::set_data(const protocache::Slice<uint8_t>& val) {
	touch(_::data);
	fields_->data.assign(val.begin(), val.end());
}

float Main_EX::get_f32() const {
	if (mask_[_::f32]) { return fields_->f32; }
	return protocache::GetFloat(view_, _::f32,data_->data()+data_->size());
}
void Main_EX::set_f32(float val) {
	touch(_::f32);
	fields_->f32 = val;
}

double Main_EX::get_f64() const {
	if (mask_[_::f64]) { return fields_->f64; }
	return protocache::GetDouble(view_, _::f64,data_->data()+data_->size());
}
void Main_EX::set_f64(double val) {
	touch(_::f64);
	fields_->f64 = val;
}

::test::Small_EX* Main_EX::get_object() {
	if (!mask_[_::object]) {
		touch(_::object);
		auto ptr = view_.GetField(_::object, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->object.reset(new ::test::Small_EX(data_, ptr - data_->data()));
		}
	}
	return fields_->object.get();
}

protocache::ArrayEX<int32_t>* Main_EX::get_i32v() {
	if (!mask_[_::i32v]) {
		touch(_::i32v);
		auto ptr = view_.GetField(_::i32v, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->i32v = protocache::ArrayEX<int32_t>(data_, ptr - data_->data());
		}
	}
	return &fields_->i32v;
}

protocache::ArrayEX<uint64_t>* Main_EX::get_u64v() {
	if (!mask_[_::u64v]) {
		touch(_::u64v);
		auto ptr = view_.GetField(_::u64v, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->u64v = protocache::ArrayEX<uint64_t>(data_, ptr - data_->data());
		}
	}
	return &fields_->u64v;
}

protocache::ArrayEX<protocache::Slice<char>>* Main_EX::get_strv() {
	if (!mask_[_::strv]) {
		touch(_::strv);
		auto ptr = view_.GetField(_::strv, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->strv = protocache::ArrayEX<protocache::Slice<char>>(data_, ptr - data_->data());
		}
	}
	return &fields_->strv;
}

protocache::ArrayEX<protocache::Slice<uint8_t>>* Main_EX::get_datav() {
	if (!mask_[_::datav]) {
		touch(_::datav);
		auto ptr = view_.GetField(_::datav, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->datav = protocache::ArrayEX<protocache::Slice<uint8_t>>(data_, ptr - data_->data());
		}
	}
	return &fields_->datav;
}

protocache::ArrayEX<float>* Main_EX::get_f32v() {
	if (!mask_[_::f32v]) {
		touch(_::f32v);
		auto ptr = view_.GetField(_::f32v, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->f32v = protocache::ArrayEX<float>(data_, ptr - data_->data());
		}
	}
	return &fields_->f32v;
}

protocache::ArrayEX<double>* Main_EX::get_f64v() {
	if (!mask_[_::f64v]) {
		touch(_::f64v);
		auto ptr = view_.GetField(_::f64v, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->f64v = protocache::ArrayEX<double>(data_, ptr - data_->data());
		}
	}
	return &fields_->f64v;
}

protocache::ArrayEX<bool>* Main_EX::get_flags() {
	if (!mask_[_::flags]) {
		touch(_::flags);
		auto ptr = view_.GetField(_::flags, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->flags = protocache::ArrayEX<bool>(data_, ptr - data_->data());
		}
	}
	return &fields_->flags;
}

protocache::ArrayEX<::test::Small_EX>* Main_EX::get_objectv() {
	if (!mask_[_::objectv]) {
		touch(_::objectv);
		auto ptr = view_.GetField(_::objectv, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->objectv = protocache::ArrayEX<::test::Small_EX>(data_, ptr - data_->data());
		}
	}
	return &fields_->objectv;
}

protocache::MapEX<protocache::Slice<char>,int32_t>* Main_EX::get_index() {
	if (!mask_[_::index]) {
		touch(_::index);
		auto ptr = view_.GetField(_::index, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->index = protocache::MapEX<protocache::Slice<char>,int32_t>(data_, ptr - data_->data());
		}
	}
	return &fields_->index;
}

protocache::MapEX<int32_t,::test::Small_EX>* Main_EX::get_objects() {
	if (!mask_[_::objects]) {
		touch(_::objects);
		auto ptr = view_.GetField(_::objects, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->objects = protocache::MapEX<int32_t,::test::Small_EX>(data_, ptr - data_->data());
		}
	}
	return &fields_->objects;
}

::test::Vec2D_EX::_* Main_EX::get_matrix() {
	if (!mask_[_::matrix]) {
		touch(_::matrix);
		auto ptr = view_.GetField(_::matrix, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->matrix = ::test::Vec2D_EX::_(data_, ptr - data_->data());
		}
	}
	return &fields_->matrix;
}

protocache::ArrayEX<::test::ArrMap_EX::_>* Main_EX::get_vector() {
	if (!mask_[_::vector]) {
		touch(_::vector);
		auto ptr = view_.GetField(_::vector, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->vector = protocache::ArrayEX<::test::ArrMap_EX::_>(data_, ptr - data_->data());
		}
	}
	return &fields_->vector;
}

::test::ArrMap_EX::_* Main_EX::get_arrays() {
	if (!mask_[_::arrays]) {
		touch(_::arrays);
		auto ptr = view_.GetField(_::arrays, data_->data() + data_->size()).GetObject();
		if (ptr != nullptr) {
			fields_->arrays = ::test::ArrMap_EX::_(data_, ptr - data_->data());
		}
	}
	return &fields_->arrays;
}

} // test