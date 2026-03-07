// Copyright (c) 2025, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <type_traits>
#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/descriptor.pb.h>
#include "protocache/access.h"

namespace protocache {

static bool Deserialize(const uint32_t* data, const uint32_t* end, google::protobuf::Message* out);

bool Deserialize(const Slice<uint32_t>& raw, google::protobuf::Message* message) {
	return Deserialize(raw.data(), raw.end(), message);
}

static std::string ToString(const Slice<char>& view) {
	return std::string(view.data(), view.size());
}

template <typename T, auto Setter>
static inline bool DeserializeSingleScalar(const Field& src, const uint32_t* end,
										   const google::protobuf::FieldDescriptor* field,
										   google::protobuf::Message* out) {
	auto reflection = out->GetReflection();
	(reflection->*Setter)(out, field, FieldT<T>(src).Get(end));
	return true;
}

template <typename T, auto Adder>
static inline bool DeserializeArrayScalar(const uint32_t* data, const uint32_t* end,
										  const google::protobuf::FieldDescriptor* field,
										  google::protobuf::Message* out) {
	auto reflection = out->GetReflection();
	for (auto one: ArrayT<T>(data, end)) {
		(reflection->*Adder)(out, field, one);
	}
	return true;
}

template <typename T, auto Setter>
static inline void SetMapKeyScalar(const Pair& pair, const google::protobuf::FieldDescriptor* key_field,
								   const uint32_t* end, google::protobuf::Message* unit) {
	auto reflection = unit->GetReflection();
	(reflection->*Setter)(unit, key_field, FieldT<T>(pair.Key()).Get(end));
}

static bool DeserializeSingle(const Field& src, const uint32_t* end,
							  const google::protobuf::FieldDescriptor* field, google::protobuf::Message* out) {
	auto reflection = out->GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return Deserialize(src.GetObject(end), end,
							 reflection->MutableMessage(out, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			reflection->SetString(out, field, ToString(FieldT<Slice<char>>(src).Get(end)));
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return DeserializeSingleScalar<double, &google::protobuf::Reflection::SetDouble>(src, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return DeserializeSingleScalar<float, &google::protobuf::Reflection::SetFloat>(src, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return DeserializeSingleScalar<uint64_t, &google::protobuf::Reflection::SetUInt64>(src, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return DeserializeSingleScalar<uint32_t, &google::protobuf::Reflection::SetUInt32>(src, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return DeserializeSingleScalar<int64_t, &google::protobuf::Reflection::SetInt64>(src, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return DeserializeSingleScalar<int32_t, &google::protobuf::Reflection::SetInt32>(src, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			return DeserializeSingleScalar<bool, &google::protobuf::Reflection::SetBool>(src, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			return DeserializeSingleScalar<EnumValue, &google::protobuf::Reflection::SetEnumValue>(src, end, field, out);
		default:
			return false;
	}
}

static bool DeserializeArray(const uint32_t* data, const uint32_t* end,
							 const google::protobuf::FieldDescriptor* field, google::protobuf::Message* out) {
	auto reflection = out->GetReflection();
	reflection->ClearField(out, field);
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			for (auto one: Array(data, end)) {
				if (!Deserialize(one.GetObject(end), end,
								 reflection->AddMessage(out, field))) {
					return false;
				}
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			for (const auto one: ArrayT<protocache::Slice<char>>(data, end)) {
				reflection->AddString(out, field, ToString(one));
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return DeserializeArrayScalar<double, &google::protobuf::Reflection::AddDouble>(data, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return DeserializeArrayScalar<float, &google::protobuf::Reflection::AddFloat>(data, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return DeserializeArrayScalar<uint64_t, &google::protobuf::Reflection::AddUInt64>(data, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return DeserializeArrayScalar<uint32_t, &google::protobuf::Reflection::AddUInt32>(data, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return DeserializeArrayScalar<int64_t, &google::protobuf::Reflection::AddInt64>(data, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return DeserializeArrayScalar<int32_t, &google::protobuf::Reflection::AddInt32>(data, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			return DeserializeArrayScalar<bool, &google::protobuf::Reflection::AddBool>(data, end, field, out);
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			return DeserializeArrayScalar<EnumValue, &google::protobuf::Reflection::AddEnumValue>(data, end, field, out);
		default:
			return false;
	}
}

static bool DeserializeMap(const uint32_t* data, const uint32_t* end,
						   const google::protobuf::FieldDescriptor* field, google::protobuf::Message* out) {
	Map map(data, end);
	if (!map) {
		return false;
	}
	auto reflection = out->GetReflection();
	reflection->ClearField(out, field);
	auto key_field = field->message_type()->map_key();
	auto value_field = field->message_type()->map_value();
	for (auto pair : map) {
		auto unit = reflection->AddMessage(out, field);
		switch (key_field->type()) {
			case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
			case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
				unit->GetReflection()->SetString(unit, key_field, ToString(FieldT<Slice<char>>(pair.Key()).Get(end)));
				break;
			case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
			case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
				SetMapKeyScalar<uint64_t, &google::protobuf::Reflection::SetUInt64>(pair, key_field, end, unit);
				break;
			case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
			case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
				SetMapKeyScalar<uint32_t, &google::protobuf::Reflection::SetUInt32>(pair, key_field, end, unit);
				break;
			case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
			case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
			case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
				SetMapKeyScalar<int64_t, &google::protobuf::Reflection::SetInt64>(pair, key_field, end, unit);
				break;
			case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
			case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
			case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
				SetMapKeyScalar<int32_t, &google::protobuf::Reflection::SetInt32>(pair, key_field, end, unit);
				break;
			default:
				return false;
		}
		if (!DeserializeSingle(pair.Value(), end, value_field, unit)) {
			return false;
		}
	}
	return true;
}

static bool Deserialize(const uint32_t* data, const uint32_t* end, google::protobuf::Message* out) {
	const auto* descriptor = out->GetDescriptor();
	const auto field_count = descriptor->field_count();
	if (field_count == 1) {
		if (const auto* field = descriptor->field(0); field->name() == "_" && field->number() == 1) {	// wrapper
			if (!field->is_repeated()) {
				return false;
			} else if (field->is_map()) {
				return DeserializeMap(data, end, field, out);
			} else {
				return DeserializeArray(data, end, field, out);
			}
		}
	}

	Message src(data, end);
	if (!src) {
		return false;
	}

	for (int i = 0; i < field_count; i++) {
		const auto* field = descriptor->field(i);
		if (field->options().deprecated()) {
			continue;
		}
		const auto id = field->number() - 1;
		if (!src.HasField(id, end)) {
			continue;
		}
		bool ok = false;
		if (!field->is_repeated()) {
			ok = DeserializeSingle(src.GetField(id,end), end, field, out);
		} else if (field->is_map()) {
			ok = DeserializeMap(src.GetField(id,end).GetObject(end), end, field, out);
		} else {
			ok =  DeserializeArray(src.GetField(id,end).GetObject(end), end, field, out);
		}
		if (!ok) {
			return false;
		}
	}
	return true;
}

} // protocache
