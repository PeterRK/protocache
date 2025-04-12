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

bool Serialize(const google::protobuf::Message& message, Buffer& buf, Unit& unit);

bool Serialize(const google::protobuf::Message& message, Buffer* buf) {
	Unit dummy;
	return Serialize(message, *buf,dummy);
}

template<typename T, typename std::enable_if<std::is_scalar<T>::value, bool>::type = true>
static bool SerializeArray(const google::protobuf::RepeatedFieldRef<T>& array, Buffer& buf, Unit& unit) {
	static_assert(sizeof(T) == 4 || sizeof(T) == 8, "");
	constexpr unsigned m = sizeof(T) / 4;
	if (array.size() == 0) {
		unit.len = 1;
		unit.data[0] = m;
		return true;
	} else if (m*array.size() >= (1U << 30U)) {
		return false;
	}
	auto last = buf.Size();
	for (int i = array.size()-1; i >= 0; i--) {
		*reinterpret_cast<T*>(buf.Expand(m)) = array.Get(i);
	}
	buf.Put((array.size() << 2U) | m);
	unit = Segment(last, buf.Size());
	return true;
}

static bool SerializeArrayField(const google::protobuf::Message& message,
								const google::protobuf::FieldDescriptor* field, Buffer& buf, Unit& unit) {
	assert(field->is_repeated());
	auto last = buf.Size();
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
		{
			auto n = reflection->FieldSize(message, field);
			std::vector<Unit> elements(n);
			for (int i = n-1; i >= 0; i--) {
				if (!Serialize(reflection->GetRepeatedMessage(message, field, i), buf, elements[i])) {
					return false;
				}
			}
			return SerializeArray(elements, buf, last, unit);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
		{
			auto n = reflection->FieldSize(message, field);
			std::vector<Unit> elements(n);
			std::string tmp;
			for (int i = n-1; i >= 0; i--) {
				if (!Serialize(reflection->GetRepeatedStringReference(message, field, i, &tmp), buf, elements[i])) {
					return false;
				}
			}
			return SerializeArray(elements, buf, last, unit);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return SerializeArray(reflection->GetRepeatedFieldRef<double>(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return SerializeArray(reflection->GetRepeatedFieldRef<float>(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return SerializeArray(reflection->GetRepeatedFieldRef<uint64_t>(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return SerializeArray(reflection->GetRepeatedFieldRef<uint32_t>(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return SerializeArray(reflection->GetRepeatedFieldRef<int64_t>(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return SerializeArray(reflection->GetRepeatedFieldRef<int32_t>(message, field), buf, unit);

		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
		{
			auto n = reflection->FieldSize(message, field);
			std::basic_string<bool> tmp(n, false);
			for (int i = 0; i < n; i++) {
				tmp[i] = reflection->GetRepeatedBool(message, field, i);
			}
			return Serialize(Slice<bool>(tmp), buf, unit);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
		{
			auto n = reflection->FieldSize(message, field);
			if (n >= (1U << 30U)) {
				return false;
			}
			for (int i = n-1; i >= 0; i--) {
				*reinterpret_cast<int32_t *>(buf.Expand(1)) = reflection->GetRepeatedEnumValue(message, field, i);
			}
			buf.Put((static_cast<uint32_t>(n) << 2U) | 1U);
			unit = Segment(last, buf.Size());
			return true;
		}
		default:
			return false;
	}
}

static bool SerializeField(const google::protobuf::Message& message,
						   const google::protobuf::FieldDescriptor* field, Buffer& buf, Unit& unit) {
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return Serialize(reflection->GetMessage(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return Serialize(reflection->GetString(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return Serialize(reflection->GetDouble(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return Serialize(reflection->GetFloat(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return Serialize(reflection->GetUInt64(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return Serialize(reflection->GetUInt32(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return Serialize(reflection->GetInt64(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return Serialize(reflection->GetInt32(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			return Serialize(reflection->GetBool(message, field), buf, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			return Serialize(static_cast<int32_t>(reflection->GetEnumValue(message, field)), buf, unit);
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
							  const google::protobuf::FieldDescriptor* field, Buffer& buf, Unit& unit) {
	assert(field->is_map());
	auto key_field = field->message_type()->field(0);
	auto value_field = field->message_type()->field(1);
	auto pairs = message.GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(message, field);

	std::unique_ptr<KeyReader> reader;

#define CREATE_KEY_READER(src_type,dest_type) \
{																	\
	std::vector<dest_type> tmp;										\
	tmp.reserve(pairs.size());										\
	for (auto& pair : pairs) {										\
		auto reflection = pair.GetReflection();						\
		tmp.push_back(reflection->Get##src_type(pair, key_field));	\
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
	std::vector<int> book(pairs.size());
	for (int i = 0; i < book.size(); i++) {
		auto key = reader->Read();
		auto pos = index.Locate(key.data(), key.size());
		assert(pos < book.size());
		book[pos] = i;
	}

	auto last = buf.Size();
	std::vector<Unit> keys(book.size());
	std::vector<Unit> values(book.size());
	std::unique_ptr<google::protobuf::Message> tmp(pairs.NewMessage());
	for (int i = static_cast<int>(book.size())-1; i >= 0; i--) {
		auto& pair = pairs.Get(book[i], tmp.get());
		SerializeField(pair, value_field, buf, values[i]);
		SerializeField(pair, key_field, buf, keys[i]);
	}
	return SerializeMap(index.Data(), keys, values, buf, last, unit);
}

bool Serialize(const google::protobuf::Message& message, Buffer& buf, Unit& unit) {
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

	auto last = buf.Size();
	std::vector<Unit> parts(fields.size());
	auto reflection = message.GetReflection();
	for (int i = static_cast<int>(fields.size())-1; i >= 0; i--) {
		auto field = fields[i];
		if (field == nullptr) {
			continue;
		}
		auto name = field->name().c_str();
		auto& part = parts[i];
		if (field->is_repeated()) {
			if (reflection->FieldSize(message, field) == 0) {
				continue;
			}
			if (field->is_map()) {
				if (!SerializeMapField(message, field, buf, part)) {
					return false;
				}
			} else {
				if (!SerializeArrayField(message, field, buf, part)) {
					return false;
				}
			}
		} else {
			if (!reflection->HasField(message, field)) {
				continue;
			}
			if (!SerializeField(message, field, buf, part)) {
				return false;
			} else if (part.size() == 1 &&
				field->type() == google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE) {
				// skip empty message field
				if (part.len == 0) {
					part.seg.len = 0;
					buf.Shrink(1);
				} else {
					part.len = 0;
				}
			}
		}
	}

	if (fields.size() == 1 && fields.front()->name() == "_") {
		if (!fields.front()->is_repeated()) {
			return false;
		}
		unit = parts.front();	// trim message wrapper
		assert(unit.len == 0);
		if (unit.seg.len == 0) {
			if (fields.front()->is_map()) {
				unit.data[0] = 5U << 28U;
			} else {
				unit.data[0] = 1U;
			}
			unit.len = 1;
		}
		return true;
	}
	return SerializeMessage(parts, buf, last, unit);
}

} // protocache