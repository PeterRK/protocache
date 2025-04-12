// Copyright (c) 2023, Ruan Kunliang.
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
#include "protocache/perfect_hash.h"
#include "protocache/serialize.h"

namespace protocache {

bool Serialize(const google::protobuf::Message& message, Data* out);

template <typename T>
static bool SerializeArray(const google::protobuf::RepeatedFieldRef<T>& array, Data* out) {
	std::vector<Data> elements;
	elements.reserve(array.size());
	for (auto& one : array) {
		elements.emplace_back();
		if (!Serialize(one, &elements.back())) {
			return false;
		}
	}
	return SerializeArray(elements, out);
}

template <typename T>
static bool SerializeScalarArray(const google::protobuf::RepeatedFieldRef<T>& array, Data* out) {
	static_assert(std::is_scalar<T>::value, "");
	auto m = WordSize(sizeof(T));
	if (m*array.size() >= (1U << 30U)) {
		return false;
	}
	out->clear();
	out->resize(1 + m*array.size(), 0);
	out->front() = (array.size() << 2U) | m;
	auto p = out->data() + 1;
	for (auto v : array) {
		*Cast<T>(p) = v;
		p += m;
	}
	return true;
}

static bool SerializeArrayField(const google::protobuf::Message& message,
								const google::protobuf::FieldDescriptor* field, Data* out) {
	assert(field->is_repeated());
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return SerializeArray(reflection->GetRepeatedFieldRef<google::protobuf::Message>(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return SerializeArray(reflection->GetRepeatedFieldRef<std::string>(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<double>(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<float>(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<uint64_t>(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<uint32_t>(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<int64_t>(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<int32_t>(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
		{
			auto n = reflection->FieldSize(message, field);
			std::string tmp(n, 0);
			for (int i = 0; i < n; i++) {
				tmp[i] = reflection->GetRepeatedBool(message, field, i);
			}
			return Serialize(tmp, out);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
		{
			auto n = reflection->FieldSize(message, field);
			if (n >= (1U << 30U)) {
				return false;
			}
			out->clear();
			out->resize(1 + n, 0);
			out->front() = (n << 2U) | 1;
			auto vec = Cast<int32_t>(out->data()+1);
			for (int i = 0; i < n; i++) {
				vec[i] = reflection->GetRepeatedEnumValue(message, field, i);
			}
			return true;
		}
		default:
			return false;
	}
}

static bool SerializeField(const google::protobuf::Message& message,
						   const google::protobuf::FieldDescriptor* field, Data* out) {
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return Serialize(reflection->GetMessage(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return Serialize(reflection->GetString(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return Serialize(reflection->GetDouble(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return Serialize(reflection->GetFloat(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return Serialize(reflection->GetUInt64(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return Serialize(reflection->GetUInt32(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return Serialize(reflection->GetInt64(message, field), out);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return Serialize(reflection->GetInt32(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			return Serialize(reflection->GetBool(message, field), out);

		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			return SerializeScalar<int32_t>(reflection->GetEnumValue(message, field), out);

		default:
			return false;
	}
}

class ScalarReader : public KeyReader {
public:
	explicit ScalarReader(const std::vector<Data>& keys) : keys_(keys) {}

	void Reset() override {
		idx_ = 0;
	}
	size_t Total() override {
		return keys_.size();
	}
	Slice<uint8_t> Read() override {
		if (idx_ >= keys_.size()) {
			return {};
		}
		auto& key = keys_[idx_++];
		return {reinterpret_cast<const uint8_t*>(key.data()), key.size()*4};
	}

protected:
	const std::vector<Data>& keys_;
	size_t idx_ = 0;
};

struct StringReader : public ScalarReader {
	explicit StringReader(const std::vector<Data>& keys) : ScalarReader(keys) {}

	Slice<uint8_t> Read() override {
		if (idx_ >= keys_.size()) {
			return {};
		}
		auto& key = keys_[idx_++];
		return String(key.data()).GetBytes();
	}
};

static bool SerializeMapField(const google::protobuf::Message& message,
							  const google::protobuf::FieldDescriptor* field, Data* out) {
	assert(field->is_map());
	auto key_field = field->message_type()->field(0);
	auto value_field = field->message_type()->field(1);
	auto elements = message.GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(message, field);

	if (key_field->is_repeated() || value_field->is_repeated()) {
		return false;
	}

	std::vector<Data> keys, values;
	keys.reserve(elements.size());
	values.reserve(elements.size());
	for (auto& one : elements) {
		auto reflection = one.GetReflection();
		if (!reflection->HasField(one, key_field) || !reflection->HasField(one, value_field)) {
			return false;
		}
		keys.emplace_back();
		values.emplace_back();
		if (!SerializeField(one, key_field, &keys.back())
			|| !SerializeField(one, value_field, &values.back())) {
			return false;
		}
	}

	PerfectHash index;
	auto build = [&index, &keys, &values](KeyReader& reader) {
		index = PerfectHash::Build(reader, true);
		if (!index) {
			return;
		}
		reader.Reset();
		std::vector<Data> tmp_keys(keys.size());
		std::vector<Data> tmp_values(values.size());
		for (unsigned i = 0; i < keys.size(); i++) {
			auto key = reader.Read();
			auto pos = index.Locate(key.data(), key.size());
			tmp_keys[pos] = std::move(keys[i]);
			tmp_values[pos] = std::move(values[i]);
		}
		keys = std::move(tmp_keys);
		values = std::move(tmp_values);
	};

	switch (key_field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
		{
			StringReader reader(keys);
			build(reader);
			break;
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
		{
			ScalarReader reader(keys);
			build(reader);
			break;
		}
		default:
			return false;
	}

	if (!index) {
		return false;
	}
	return SerializeMap(index.Data(), keys, values, out);
}

bool Serialize(const google::protobuf::Message& message, Data* out) {
	auto descriptor = message.GetDescriptor();
	auto field_count = descriptor->field_count();
	if (field_count <= 0) {
		return false;
	}

	auto max_id = 1;
	for (int i = 0; i < field_count; i++) {
		auto field = descriptor->field(i);
		if (field == nullptr || field->number() <= 0) {
			return false;
		}
		max_id = std::max(max_id, field->number());
	}
	if (max_id > (12 + 25*255) || (max_id - field_count > 6 && max_id > field_count*2)) {
		return false;
	}

	std::vector<const google::protobuf::FieldDescriptor*> fields(max_id, nullptr);
	for (int i = 0; i < field_count; i++) {
		auto field = descriptor->field(i);
		auto j = field->number() - 1;
		if (fields[j] != nullptr) {
			return false;
		}
		fields[j] = field;
	}
	std::vector<Data> parts(fields.size());
	auto reflection = message.GetReflection();
	for (unsigned i = 0; i < fields.size(); i++) {
		auto field = fields[i];
		auto& unit = parts[i];
		if (field == nullptr || field->options().deprecated()) {
			continue;
		}
		//auto name = field->name().c_str();
		if (field->is_repeated()) {
			if (reflection->FieldSize(message, field) == 0) {
				continue;
			}
			if (field->is_map()) {
				if (!SerializeMapField(message, field, &unit)) {
					return false;
				}
			} else {
				if (!SerializeArrayField(message, field, &unit)) {
					return false;
				}
			}
		} else {
			if (!reflection->HasField(message, field)) {
				continue;
			}
			if (!SerializeField(message, field, &unit)) {
				return false;
			} else if (unit.size() == 1 &&
				field->type() == google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE) {
				unit.clear();	// skip empty message field
			}
		}
	}

	if (fields.size() == 1 && fields[0]->name() == "_") {
		if (!fields[0]->is_repeated()) {
			return false;
		}
		*out = parts.front();	// trim message wrapper
		if (out->empty()) {
			if (fields[0]->is_map()) {
				out->push_back(5U << 28U);
			} else {
				out->push_back(1U);
			}
		}
		return true;
	}
	return SerializeMessage(parts, out);
}

} // protocache