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

bool Serialize(const google::protobuf::Message& message, Buffer* buf);

template<typename T, typename std::enable_if<std::is_scalar<T>::value, bool>::type = true>
static bool SerializeArray(const google::protobuf::RepeatedFieldRef<T>& array, Buffer* buf) {
	auto m = WordSize(sizeof(T));
	if (m*array.size() >= (1U << 30U)) {
		return false;
	}
	for (int i = array.size()-1; i >= 0; i--) {
		buf->Put(array.Get(i));
	}
	uint32_t head = (array.size() << 2U) | m;
	buf->Put(head);
	return true;
}

static bool SerializeArrayField(const google::protobuf::Message& message,
								const google::protobuf::FieldDescriptor* field, Buffer* buf) {
	assert(field->is_repeated());
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
		{
			auto n = reflection->FieldSize(message, field);
			std::vector<Buffer::Seg> elements(n);
			auto end = buf->Size();
			auto last = end;
			for (int i = n-1; i >= 0; i--) {
				if (!Serialize(reflection->GetRepeatedMessage(message, field, i), buf)) {
					return false;
				}
				elements[i] = Segment(last, buf->Size());
				last = buf->Size();
			}
			return SerializeArray(elements, *buf, end);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
		{
			auto n = reflection->FieldSize(message, field);
			std::vector<Buffer::Seg> elements(n);
			auto end = buf->Size();
			auto last = end;
			std::string tmp;
			for (int i = n-1; i >= 0; i--) {
				if (!Serialize(reflection->GetRepeatedStringReference(message, field, i, &tmp), buf)) {
					return false;
				}
				elements[i] = Segment(last, buf->Size());
				last = buf->Size();
			}
			return SerializeArray(elements, *buf, end);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return SerializeArray(reflection->GetRepeatedFieldRef<double>(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return SerializeArray(reflection->GetRepeatedFieldRef<float>(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return SerializeArray(reflection->GetRepeatedFieldRef<uint64_t>(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return SerializeArray(reflection->GetRepeatedFieldRef<uint32_t>(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return SerializeArray(reflection->GetRepeatedFieldRef<int64_t>(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return SerializeArray(reflection->GetRepeatedFieldRef<int32_t>(message, field), buf);

		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
		{
			auto n = reflection->FieldSize(message, field);
			std::basic_string<bool> tmp(n, false);
			for (int i = 0; i < n; i++) {
				tmp[i] = reflection->GetRepeatedBool(message, field, i);
			}
			return Serialize(Slice<bool>(tmp), buf);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
		{
			auto n = reflection->FieldSize(message, field);
			if (n >= (1U << 30U)) {
				return false;
			}
			for (int i = n-1; i >= 0; i--) {
				Serialize(static_cast<int32_t>(reflection->GetRepeatedEnumValue(message, field, i)), buf);
			}
			uint32_t head = (static_cast<uint32_t>(n) << 2U) | 1U;
			buf->Put(head);
			return true;
		}
		default:
			return false;
	}
}

static bool SerializeField(const google::protobuf::Message& message,
						   const google::protobuf::FieldDescriptor* field, Buffer* buf) {
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return Serialize(reflection->GetMessage(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return Serialize(reflection->GetString(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return Serialize(reflection->GetDouble(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return Serialize(reflection->GetFloat(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return Serialize(reflection->GetUInt64(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return Serialize(reflection->GetUInt32(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return Serialize(reflection->GetInt64(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return Serialize(reflection->GetInt32(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			return Serialize(reflection->GetBool(message, field), buf);
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			return Serialize(static_cast<int32_t>(reflection->GetEnumValue(message, field)), buf);
		default:
			return false;
	}
}

template <typename T>
class VectorReader : public KeyReader {
public:
	explicit VectorReader(std::vector<T>&& keys) : keys_(std::move(keys)) {}

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
		return {reinterpret_cast<const uint8_t*>(&key), sizeof(T)};
	}

private:
	std::vector<T> keys_;
	size_t idx_ = 0;
};

template <>
class VectorReader<std::string> : public KeyReader {
public:
	explicit VectorReader(std::vector<std::string>&& keys) : keys_(std::move(keys)) {}

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
		return {reinterpret_cast<const uint8_t*>(key.data()), key.size()};
	}

private:
	std::vector<std::string> keys_;
	size_t idx_ = 0;
};

static bool SerializeMapField(const google::protobuf::Message& message,
							  const google::protobuf::FieldDescriptor* field, Buffer* buf) {
	assert(field->is_map());
	auto key_field = field->message_type()->field(0);
	auto value_field = field->message_type()->field(1);
	auto units = message.GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(message, field);

	std::unique_ptr<KeyReader> reader;

#define CREATE_KEY_READER(src_type,dest_type) \
{																	\
	std::vector<dest_type> tmp;										\
	tmp.reserve(units.size());										\
	for (auto& unit : units) {										\
		auto reflection = unit.GetReflection();						\
		tmp.push_back(reflection->Get##src_type(unit, key_field));	\
	}																\
	reader.reset(new VectorReader<dest_type>(std::move(tmp)));		\
	break;                                       					\
}
	switch (key_field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			CREATE_KEY_READER(String, std::string)
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			CREATE_KEY_READER(UInt64, uint64_t)
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			CREATE_KEY_READER(UInt32, uint32_t)
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			CREATE_KEY_READER(Int64, int64_t)
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			CREATE_KEY_READER(Int32, int32_t)
		default:
			return false;
	}
#undef CREATE_KEY_READER

	auto index = PerfectHash::Build(*reader, true);
	if (!index) {
		return false;
	}
	reader->Reset();
	std::vector<int> book(units.size());
	for (int i = 0; i < book.size(); i++) {
		auto key = reader->Read();
		auto pos = index.Locate(key.data(), key.size());
		assert(pos < book.size());
		book[pos] = i;
	}

	std::vector<Buffer::Seg> keys(book.size());
	std::vector<Buffer::Seg> values(book.size());
	auto end = buf->Size();
	auto last = end;
	std::unique_ptr<google::protobuf::Message> tmp(units.NewMessage());
	for (int i = static_cast<int>(book.size())-1; i >= 0; i--) {
		int j = book[i];
		auto& unit = units.Get(j, tmp.get());
		SerializeField(unit, value_field, buf);
		values[i] = Segment(last, buf->Size());
		last = buf->Size();
		SerializeField(unit, key_field, buf);
		keys[i] = Segment(last, buf->Size());
		last = buf->Size();
	}
	return SerializeMap(index.Data(), keys, values, *buf, end);
}

bool Serialize(const google::protobuf::Message& message, Buffer* buf) {
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
		if (field->options().deprecated()) {
			continue;
		}
		auto j = field->number() - 1;
		if (fields[j] != nullptr) {
			return false;
		}
		fields[j] = field;
	}

	auto end = buf->Size();
	auto last = end;
	std::vector<Buffer::Seg> parts(fields.size());
	auto reflection = message.GetReflection();
	for (int i = static_cast<int>(fields.size())-1; i >= 0; i--) {
		auto field = fields[i];
		if (field == nullptr) {
			parts[i] = {0, 0};
			continue;
		}
		auto name = field->name().c_str();
		if (field->is_repeated()) {
			if (reflection->FieldSize(message, field) == 0) {
				parts[i] = {0, 0};
				continue;
			}
			if (field->is_map()) {
				if (!SerializeMapField(message, field, buf)) {
					return false;
				}
			} else {
				if (!SerializeArrayField(message, field, buf)) {
					return false;
				}
			}
		} else {
			if (!reflection->HasField(message, field)) {
				parts[i] = {0, 0};
				continue;
			}
			if (!SerializeField(message, field, buf)) {
				return false;
			}
		}
		parts[i] = Segment(last, buf->Size());
		last = buf->Size();
	}

	if (fields.size() == 1 && fields[0]->name() == "_") {
		// trim message wrapper
		return true;
	}
	return SerializeMessage(parts, *buf, end);
}

} // protocache