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

bool Serialize(const google::protobuf::Message& message, Buffer& buf_, Unit& unit);

class SerializeContext final {
public:
	explicit SerializeContext(Buffer& buf) : buf_(buf) {}

	bool Serialize(const google::protobuf::Message& message, Unit& unit);

private:
	std::string tmp_str_;
	Buffer& buf_;

	template<typename T>
	bool SerializeArray(const google::protobuf::RepeatedFieldRef<T>& array, Unit& unit);

	bool SerializeSimpleField(const google::protobuf::Message& message,
						const google::protobuf::Reflection* reflection,
						const google::protobuf::FieldDescriptor* field, Unit& unit);

	bool SerializeArrayField(const google::protobuf::Message& message,
							 const google::protobuf::Reflection* reflection,
							 const google::protobuf::FieldDescriptor* field, Unit& unit);

	bool SerializeMapField(const google::protobuf::Message& message,
						   const google::protobuf::Reflection* reflection,
						   const google::protobuf::FieldDescriptor* field, Unit& unit);

	template <typename T, auto Getter>
	bool SerializeMapFieldByKey(
			const google::protobuf::RepeatedFieldRef<google::protobuf::Message>& pairs,
			const google::protobuf::FieldDescriptor* key_field,
			const google::protobuf::FieldDescriptor* value_field, Unit& unit);

	bool SerializeField(const google::protobuf::Message& message,
						const google::protobuf::Reflection* reflection,
						const google::protobuf::FieldDescriptor* field, Unit& unit);
};

bool Serialize(const google::protobuf::Message& message, Buffer* buf) {
	Unit dummy;
	SerializeContext ctx(*buf);
	return ctx.Serialize(message, dummy);
}

template<typename T>
bool SerializeContext::SerializeArray(const google::protobuf::RepeatedFieldRef<T>& array, Unit& unit) {
	static_assert(std::is_scalar_v<T>);
	static_assert(sizeof(T) == 4 || sizeof(T) == 8);
	constexpr unsigned m = sizeof(T) / 4;
	if (array.size() == 0) {
		unit.len = 1;
		unit.data[0] = m;
		return true;
	} else if (m*array.size() >= (1U << 30U)) {
		return false;
	}
	auto last = buf_.Size();
	for (int i = array.size(); i-- > 0;) {
		*reinterpret_cast<T*>(buf_.Expand(m)) = array.Get(i);
	}
	buf_.Put((array.size() << 2U) | m);
	unit = Segment(last, buf_.Size());
	return true;
}

bool SerializeContext::SerializeArrayField(const google::protobuf::Message& message,
										   const google::protobuf::Reflection* reflection,
										   const google::protobuf::FieldDescriptor* field, Unit& unit) {
	assert(field->is_repeated());
	auto last = buf_.Size();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
		{
			auto n = reflection->FieldSize(message, field);
			std::vector<Unit> elements(n);
			for (int i = n-1; i >= 0; i--) {
				if (!Serialize(reflection->GetRepeatedMessage(message, field, i), elements[i])) {
					return false;
				}
			}
			return ::protocache::SerializeArray(elements, buf_, last, unit);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
		{
			auto n = reflection->FieldSize(message, field);
			std::vector<Unit> elements(n);
			for (int i = n-1; i >= 0; i--) {
				if (!::protocache::Serialize(
						reflection->GetRepeatedStringReference(message, field, i, &tmp_str_),
						buf_, elements[i])) {
					return false;
				}
			}
			return ::protocache::SerializeArray(elements, buf_, last, unit);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return SerializeArray(reflection->GetRepeatedFieldRef<double>(message, field), unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return SerializeArray(reflection->GetRepeatedFieldRef<float>(message, field), unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return SerializeArray(reflection->GetRepeatedFieldRef<uint64_t>(message, field), unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return SerializeArray(reflection->GetRepeatedFieldRef<uint32_t>(message, field), unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return SerializeArray(reflection->GetRepeatedFieldRef<int64_t>(message, field), unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return SerializeArray(reflection->GetRepeatedFieldRef<int32_t>(message, field), unit);

		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
		{
			auto n = reflection->FieldSize(message, field);
			std::basic_string<bool> tmp(n, false);
			for (int i = 0; i < n; i++) {
				tmp[i] = reflection->GetRepeatedBool(message, field, i);
			}
			return ::protocache::Serialize(Slice<bool>(tmp), buf_, unit);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
		{
			auto n = reflection->FieldSize(message, field);
			if (n >= (1U << 30U)) {
				return false;
			}
			for (int i = n-1; i >= 0; i--) {
				*reinterpret_cast<int32_t *>(buf_.Expand(1)) = reflection->GetRepeatedEnumValue(message, field, i);
			}
			buf_.Put((static_cast<uint32_t>(n) << 2U) | 1U);
			unit = Segment(last, buf_.Size());
			return true;
		}
		default:
			return false;
	}
}

bool SerializeContext::SerializeSimpleField(const google::protobuf::Message& message,
									  const google::protobuf::Reflection* reflection,
									  const google::protobuf::FieldDescriptor* field, Unit& unit) {
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return Serialize(reflection->GetMessage(message, field), unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return ::protocache::Serialize(reflection->GetStringReference(message, field, &tmp_str_), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return ::protocache::Serialize(reflection->GetDouble(message, field), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return ::protocache::Serialize(reflection->GetFloat(message, field), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return ::protocache::Serialize(reflection->GetUInt64(message, field), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return ::protocache::Serialize(reflection->GetUInt32(message, field), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return ::protocache::Serialize(reflection->GetInt64(message, field), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return ::protocache::Serialize(reflection->GetInt32(message, field), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			return ::protocache::Serialize(reflection->GetBool(message, field), buf_, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			return ::protocache::Serialize(static_cast<int32_t>(reflection->GetEnumValue(message, field)), buf_, unit);
		default:
			return false;
	}
}

template <typename T>
class VectorReader : public KeyReader {
public:
	explicit VectorReader(const std::vector<T>& keys) : keys_(keys) {}

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
		if constexpr (std::is_same_v<T, std::string>) {
			return {reinterpret_cast<const uint8_t*>(key.data()), key.size()};
		} else {
			return {reinterpret_cast<const uint8_t*>(&key), sizeof(T)};
		}
	}

private:
	const std::vector<T>& keys_;
	size_t idx_ = 0;
};

template <typename T, auto Getter>
bool SerializeContext::SerializeMapFieldByKey(
		const google::protobuf::RepeatedFieldRef<google::protobuf::Message>& pairs,
		const google::protobuf::FieldDescriptor* key_field,
		const google::protobuf::FieldDescriptor* value_field, Unit& unit) {
	std::vector<T> keys;
	keys.reserve(pairs.size());
	for (auto& pair : pairs) {
		auto reflection = pair.GetReflection();
		keys.push_back((reflection->*Getter)(pair, key_field));
	}

	VectorReader<T> reader(keys);
	auto index = PerfectHashObject::Build(reader, true);
	if (!index) {
		return false;
	}

	reader.Reset();
	std::vector<int> book(keys.size());
	for (size_t i = 0; i < book.size(); i++) {
		auto key = reader.Read();
		auto pos = index.Locate(key.data(), key.size());
		assert(pos < book.size());
		book[pos] = static_cast<int>(i);
	}

	auto last = buf_.Size();
	std::vector<std::pair<Unit,Unit>> units(book.size());
	std::unique_ptr<google::protobuf::Message> tmp(pairs.NewMessage());
	for (size_t i = book.size(); i-- > 0;) {
		auto pair_index = book[i];
		auto& pair = pairs.Get(pair_index, tmp.get());
		auto pair_reflection = pair.GetReflection();
		if (!SerializeSimpleField(pair, pair_reflection, value_field, units[i].second)
			|| !::protocache::Serialize(keys[pair_index], buf_, units[i].first)) {
			return false;
		}
	}
	return SerializeMap(index.Data(), units, buf_, last, unit);
}

bool SerializeContext::SerializeMapField(const google::protobuf::Message& message,
										 const google::protobuf::Reflection* reflection,
										 const google::protobuf::FieldDescriptor* field, Unit& unit) {
	assert(field->is_map());
	auto key_field = field->message_type()->field(0);
	auto value_field = field->message_type()->field(1);
	auto pairs = reflection->GetRepeatedFieldRef<google::protobuf::Message>(message, field);

	switch (key_field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return SerializeMapFieldByKey<std::string, &google::protobuf::Reflection::GetString>(
					pairs, key_field, value_field, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return SerializeMapFieldByKey<uint64_t, &google::protobuf::Reflection::GetUInt64>(
					pairs, key_field, value_field, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return SerializeMapFieldByKey<uint32_t, &google::protobuf::Reflection::GetUInt32>(
					pairs, key_field, value_field, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return SerializeMapFieldByKey<int64_t, &google::protobuf::Reflection::GetInt64>(
					pairs, key_field, value_field, unit);
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return SerializeMapFieldByKey<int32_t, &google::protobuf::Reflection::GetInt32>(
					pairs, key_field, value_field, unit);
		default:
			return false;
	}
}

bool SerializeContext::SerializeField(const google::protobuf::Message& message,
									  const google::protobuf::Reflection* reflection,
									  const google::protobuf::FieldDescriptor* field, Unit& unit) {
	if (field->is_repeated()) {
		if (reflection->FieldSize(message, field) == 0) {
			unit = {};
			return true;
		}
		if (field->is_map()) {
			if (!SerializeMapField(message, reflection, field, unit)) {
				return false;
			}
		} else {
			if (!SerializeArrayField(message, reflection, field, unit)) {
				return false;
			}
		}
	} else {
		if (!reflection->HasField(message, field)) {
			unit = {};
			return true;
		}
		if (!SerializeSimpleField(message, reflection, field, unit)) {
			return false;
		} else if (unit.size() == 1 &&
				   field->type() == google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE) {
			// skip empty message field
			if (unit.len == 0) {
				unit.seg.len = 0;
				buf_.Shrink(1);
			} else {
				unit.len = 0;
			}
		}
	}
	return true;
}

bool SerializeContext::Serialize(const google::protobuf::Message& message, Unit& unit) {
	auto reflection = message.GetReflection();
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

	if (fields.size() == 1 && fields.front() != nullptr && fields.front()->name() == "_") {
		if (!fields.front()->is_repeated()
			|| !SerializeField(message, reflection, fields.front(), unit)) {
			return false;
		}
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

	auto last = buf_.Size();
	std::vector<Unit> parts(fields.size());
	for (size_t i = fields.size(); i-- > 0;) {
		auto field = fields[i];
		if (field == nullptr) {
			continue;
		}
		//auto name = field->name().c_str();
		auto& part = parts[i];
		if (!SerializeField(message, reflection, field, part)) {
			return false;
		}
		FoldField(buf_, part);
	}
	return ::protocache::SerializeMessage(parts, buf_, last, unit);
}

} // protocache
