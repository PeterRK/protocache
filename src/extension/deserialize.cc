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
			reflection->SetDouble(out, field, FieldT<double>(src).Get());
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			reflection->SetFloat(out, field, FieldT<float>(src).Get());
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			reflection->SetUInt64(out, field, FieldT<uint64_t>(src).Get());
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			reflection->SetUInt32(out, field, FieldT<uint32_t>(src).Get());
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			reflection->SetInt64(out, field, FieldT<int64_t>(src).Get());
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			reflection->SetInt32(out, field, FieldT<int32_t>(src).Get());
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			reflection->SetBool(out, field, FieldT<bool>(src).Get());
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			reflection->SetEnumValue(out, field, FieldT<EnumValue>(src).Get());
			return true;
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
			for (auto one: ArrayT<double>(data, end)) {
				reflection->AddDouble(out, field, one);
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			for (auto one: ArrayT<float>(data, end)) {
				reflection->AddFloat(out, field, one);
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			for (auto one: ArrayT<uint64_t>(data, end)) {
				reflection->AddUInt64(out, field, one);
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			for (auto one: ArrayT<uint32_t>(data, end)) {
				reflection->AddUInt32(out, field, one);
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			for (auto one: ArrayT<int64_t>(data, end)) {
				reflection->AddInt64(out, field, one);
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			for (auto one: ArrayT<int32_t>(data, end)) {
				reflection->AddInt32(out, field, one);
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			for (auto one: ArrayT<bool>(data, end)) {
				reflection->AddBool(out, field, one);
			}
			return true;
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			for (auto one: ArrayT<EnumValue>(data, end)) {
				reflection->AddEnumValue(out, field, one);
			}
			return true;
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
				unit->GetReflection()->SetUInt64(unit, key_field, FieldT<uint64_t>(pair.Key()).Get());
				break;
			case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
			case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
				unit->GetReflection()->SetUInt32(unit, key_field, FieldT<uint32_t>(pair.Key()).Get());
				break;
			case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
			case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
			case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
				unit->GetReflection()->SetInt64(unit, key_field, FieldT<int64_t>(pair.Key()).Get());
				break;
			case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
			case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
			case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
				unit->GetReflection()->SetInt32(unit, key_field, FieldT<int32_t>(pair.Key()).Get());
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
	auto descriptor = out->GetDescriptor();
	auto field_count = descriptor->field_count();
	if (field_count == 1) {
		auto field = descriptor->field(0);
		if (field->name() == "_" && field->number() == 1) {	// wrapper
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
		auto field = descriptor->field(i);
		if (field->options().deprecated()) {
			continue;
		}
		auto id = field->number() - 1;
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