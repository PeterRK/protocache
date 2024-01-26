// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/descriptor.pb.h>
#include "protocache/perfect_hash.h"
#include "protocache/access.h"
#include "protocache/serialize.h"

namespace protocache {

static unsigned WriteVarInt(uint8_t* buf, uint32_t n) noexcept {
	unsigned w = 0;
	while ((n & ~0x7fU) != 0) {
		buf[w++] = 0x80U | (n & 0x7fU);
		n >>= 7U;
	}
	buf[w++] = n;
	return w;
}

static inline uint32_t Offset(uint32_t off) noexcept {
	return (off<<2U) | 3U;
}

template <typename T>
static inline T* Cast(const void* p) noexcept {
	return const_cast<T*>(reinterpret_cast<const T*>(p));
}

template <typename T>
static Data Serialize(T v) {
	static_assert(std::is_scalar<T>::value, "");
	Data out(WordSize(sizeof(T)), 0);
	*Cast<T>(out.data()) = v;
	return out;
}

static Data Serialize(const std::string& str) {
	if (str.size() >= (1U << 30U)) {
		return {};
	}
	uint32_t mark = str.size() << 2U;
	uint8_t header[5];
	auto sz = WriteVarInt(header, mark);
	Data out(WordSize(sz+str.size()), 0);
	auto p = Cast<uint8_t>(out.data());
	for (unsigned i = 0; i < sz; i++) {
		*p++ = header[i];
	}
	memcpy(p, str.data(), str.size());
	return out;
}

struct ArraySize {
	unsigned mode;
	size_t size;
};

static size_t BestArraySize(const std::vector<Data>& parts, unsigned& m) {
	size_t sizes[3] = {0, 0, 0};
	for (auto& one : parts) {
		sizes[0] += 1;
		sizes[1] += 2;
		sizes[2] += 3;
		if (one.size() <= 1) {
			continue;
		}
		sizes[0] += one.size();
		if (one.size() <= 2) {
			continue;
		}
		sizes[1] += one.size();
		if (one.size() <= 3) {
			continue;
		}
		sizes[2] += one.size();
	}
	unsigned mode = 0;
	for (unsigned i = 1; i < 3; i++) {
		if (sizes[i] < sizes[mode]) {
			mode = i;
		}
	}
	m = mode + 1;
	return sizes[mode];
}

template <typename T>
static Data SerializeArray(const google::protobuf::RepeatedFieldRef<T>& array) {
	Data out;
	std::vector<Data> parts;
	parts.reserve(array.size());
	for (auto& one : array) {
		parts.push_back(Serialize(one));
		if (parts.back().empty()) {
			return out;
		}
	}

	unsigned m = 0;
	size_t size = 1 + BestArraySize(parts, m);
	if (size >= (1U<<30U)) {
		return {};
	}
	out.reserve(size);
	out.push_back((array.size() << 2U) | m);

	for (auto& one : parts) {
		auto next = out.size() + m;
		if (one.size() <= m) {
			out += one;
		}
		out.resize(next, 0);
	}
	unsigned off = 1;
	for (auto& one : parts) {
		if (one.size() > m) {
			out[off] = Offset(out.size()-off);
			out += one;
		}
		off += m;
	}
	assert(out.size() == size);
	return out;
}

template <typename T>
static Data SerializeScalarArray(const google::protobuf::RepeatedFieldRef<T>& array) {
	static_assert(std::is_scalar<T>::value, "");
	auto m = WordSize(sizeof(T));
	Data out(1 + m*array.size(), 0);
	if (out.size() >= (1U << 30U)) {
		return {};
	}
	out[0] = (array.size() << 2U) | m;
	auto p = out.data() + 1;
	for (auto v : array) {
		*Cast<T>(p) = v;
		p += m;
	}
	return out;
}

static Data SerializeArrayField(const google::protobuf::Message& message, const google::protobuf::FieldDescriptor* field) {
	assert(field->is_repeated());
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return SerializeArray(reflection->GetRepeatedFieldRef<google::protobuf::Message>(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return SerializeArray(reflection->GetRepeatedFieldRef<std::string>(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<double>(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<float>(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<uint64_t>(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<uint32_t>(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<int64_t>(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return SerializeScalarArray(reflection->GetRepeatedFieldRef<int32_t>(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
		{
			auto n = reflection->FieldSize(message, field);
			std::string tmp(n, 0);
			for (int i = 0; i < n; i++) {
				tmp[i] = reflection->GetRepeatedBool(message, field, i);
			}
			return Serialize(tmp);
		}
		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
		{
			auto n = reflection->FieldSize(message, field);
			Data out(1 + n, 0);
			out[0] = (n << 2U) | 1;
			auto vec = Cast<int32_t>(out.data()+1);
			for (int i = 0; i < n; i++) {
				vec[i] = reflection->GetRepeatedEnumValue(message, field, i);
			}
			return out;
		}

		default:
			return {};
	}
}

static Data SerializeField(const google::protobuf::Message& message, const google::protobuf::FieldDescriptor* field) {
	auto reflection = message.GetReflection();
	switch (field->type()) {
		case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
			return Serialize(reflection->GetMessage(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
		case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
			return Serialize(reflection->GetString(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
			return Serialize(reflection->GetDouble(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
			return Serialize(reflection->GetFloat(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
			return Serialize(reflection->GetUInt64(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
			return Serialize(reflection->GetUInt32(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
			return Serialize(reflection->GetInt64(message, field));
		case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
			return Serialize(reflection->GetInt32(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
			return Serialize(reflection->GetBool(message, field));

		case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
			return Serialize<int32_t>(reflection->GetEnumValue(message, field));

		default:
			return {};
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

static Data SerializeMapField(const google::protobuf::Message& message, const google::protobuf::FieldDescriptor* field) {
	assert(field->is_map());
	auto key_field = field->message_type()->field(0);
	auto value_field = field->message_type()->field(1);
	auto elements = message.GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(message, field);

	if (key_field->is_repeated() || value_field->is_repeated()) {
		return {};
	}

	std::vector<Data> keys, values;
	keys.reserve(elements.size());
	values.reserve(elements.size());
	for (auto& one : elements) {
		auto reflection = one.GetReflection();
		if (!reflection->HasField(one, key_field) || !reflection->HasField(one, value_field)) {
			return {};
		}
		keys.push_back(SerializeField(one, key_field));
		if (keys.back().empty()) {
			return {};
		}
		values.push_back(SerializeField(one, value_field));
		if (values.back().empty()) {
			return {};
		}
	}

	PerfectHash index;
	auto build = [&index, &keys, &values](KeyReader& reader) {
		index = PerfectHash::Build(reader, true);
		if (!index) {
			return;
		}
		std::vector<Data> new_keys(keys.size());
		std::vector<Data> new_values(values.size());
		reader.Reset();
		for (unsigned i = 0; i < keys.size(); i++) {
			auto key = reader.Read();
			auto pos = index.Locate(key.data(), key.size());
			new_keys[pos] = std::move(keys[i]);
			new_values[pos] = std::move(values[i]);
		}
		keys.swap(new_keys);
		values.swap(new_values);
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
			return {};;
	}

	if (!index) {
		return {};
	}
	auto index_size = WordSize(index.Data().size());

	unsigned m1 = 0;
	auto key_size = BestArraySize(keys, m1);
	unsigned m2 = 0;
	auto value_size = BestArraySize(values, m2);

	auto size = index_size + key_size + value_size;
	if (size >= (1U<<30U)) {
		return {};
	}
	Data out;
	out.reserve(size);
	out.resize(index_size, 0);
	memcpy(const_cast<uint32_t*>(out.data()), index.Data().data(), index.Data().size());
	out[0] |= (m1<<30U) | (m2<<28U);

	for (unsigned i = 0; i < keys.size(); i++) {
		auto next = out.size() + m1;
		if (keys[i].size() <= m1) {
			out += keys[i];
		}
		out.resize(next, 0);
		next = out.size() + m2;
		if (values[i].size() <= m2) {
			out += values[i];
		}
		out.resize(next, 0);
	}
	unsigned off = index_size;
	for (unsigned i = 0; i < keys.size(); i++) {
		if (keys[i].size() > m1) {
			out[off] = Offset(out.size()-off);
			out += keys[i];
		}
		off += m1;
		if (values[i].size() > m2) {
			out[off] = Offset(out.size()-off);
			out += values[i];
		}
		off += m2;
	}
	assert(out.size() == size);
	return out;
}

Data Serialize(const google::protobuf::Message& message) {
	auto descriptor = message.GetDescriptor();
	auto field_count = descriptor->field_count();
	if (field_count <= 0) {
		return {};
	}
	std::vector<const google::protobuf::FieldDescriptor*> fields(field_count);
	for (int i = 0; i < field_count; i++) {
		fields[i] = descriptor->field(i);
	}
	std::sort(fields.begin(), fields.end(),
			  [](const google::protobuf::FieldDescriptor* a, const google::protobuf::FieldDescriptor* b)->bool{
				  return a->number() < b->number();
			  });
	if (fields[0]->number() <= 0) {
		return {};
	}
	for (int i = 1; i < field_count; i++) {
		if (fields[i]->number() == fields[i-1]->number()) {
			return {};
		}
	}
	auto max_id = fields.back()->number();
	if (max_id - field_count > 6 && max_id > field_count*2) {
		return {};
	}
	if (max_id > (12 + 25*255)) {
		return {};
	}
	fields.resize(max_id, nullptr);
	for (int i = field_count-1; i >= 0; i--) {
		auto j = fields[i]->number() - 1;
		if (i != j) {
			std::swap(fields[i], fields[j]);
		}
	}

	std::vector<Data> parts(fields.size());
	auto reflection = message.GetReflection();
	for (unsigned i = 0; i < fields.size(); i++) {
		auto field = fields[i];
		auto& unit = parts[i];
		auto name = field->name().c_str();
		if (field->options().deprecated()) {
			continue;
		}
		if (field->is_repeated()) {
			if (reflection->FieldSize(message, field) == 0) {
				continue;
			}
			if (field->is_map()) {
				unit = SerializeMapField(message, field);
			} else {
				unit = SerializeArrayField(message, field);
			}
		} else {
			if (!reflection->HasField(message, field)) {
				continue;
			}
			unit = SerializeField(message, field);
		}
		auto size = unit.size();
		if (unit.empty()) {
			return {};
		}
	}

	while (!parts.empty() && parts.back().empty()) {
		parts.pop_back();
	}

	Data out;
	if (parts.empty()) {
		out.push_back(0);
		return out;
	}
	if (fields.size() == 1 && fields[0]->name() == "_") {
		return parts[0];	// trim message wrapper
	}

	auto section = (parts.size() + 12) / 25;
	if (section > 0xff) {
		return {};
	}
	size_t size = 1 + section*2;
	uint32_t cnt = 0;
	uint32_t head = section;
	for (unsigned i = 0; i < std::min(12UL, parts.size()); i++) {
		auto& one = parts[i];
		if (one.size() < 4) {
			head |= one.size() << (8U+i*2U);
			size += one.size();
			cnt += one.size();
		} else {
			head |= 1U << (8U+i*2U);
			size += 1 + one.size();
			cnt += 1;
		}
	}
	for (unsigned i = 12; i < parts.size(); i++) {
		auto& one = parts[i];
		if (one.size() < 4) {
			size += one.size();
		} else {
			size += 1 + one.size();
		}
	}
	if (size >= (1U<<30U)) {
		return {};
	}
	out.reserve(size);
	out.push_back(head);
	out.resize(1+section*2, 0);

	auto blk = Cast<uint64_t>(out.data()+1);
	for (unsigned i = 12; i < parts.size(); ) {
		auto next = i + 25;
		if (next > parts.size()) {
			next = parts.size();
		}
		if (cnt >= (1U<<14U)) {
			return {};
		}
		auto mark = static_cast<uint64_t>(cnt) << 50U;
		for (unsigned j = 0; i < next; j+=2) {
			auto& one = parts[i++];
			if (one.size() < 4) {
				mark |= static_cast<uint64_t>(one.size()) << j;
				cnt += one.size();
			} else {
				mark |= 1ULL << j;
				cnt += 1;
			}
		}
		*blk++ = mark;
	}

	size_t off = out.size();
	for (auto& one : parts) {
		if (one.empty()) {
			continue;
		}
		if (one.size() < 4) {
			out += one;
		} else {
			out.push_back(0);
		}
	}
	for (auto& one : parts) {
		if (one.size() < 4) {
			off += one.size();
		} else {
			out[off] = Offset(out.size()-off);
			out += one;
			off++;
		}
	}
	assert(out.size() == size);
	return out;
}

} // protocache