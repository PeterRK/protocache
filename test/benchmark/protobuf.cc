// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include <string>
#include "protocache/extension/utils.h"
#include <google/protobuf/reflection.h>
#include "test.pb.h"
#include "common.h"

static inline uint32_t JunkHash(const std::string& data) {
	return JunkHash(data.data(), data.size());
}

struct Junk1 : public Junk {
	void Traverse(const ::test::Small& root);
	void Traverse(const ::test::Vec2D& root);
	void Traverse(const ::test::ArrMap& root);
	void Traverse(const ::test::Main& root);

	void Traverse(const google::protobuf::Message& root);
};

inline void Junk1::Traverse(const ::test::Small& root) {
	u32 += root.i32() + root.flag();
	u32 += JunkHash(root.str());
}

inline void Junk1::Traverse(const ::test::Vec2D& root) {
	for (auto& u : root._()) {
		for (auto v : u._()) {
			f32 += v;
		}
	}
}

inline void Junk1::Traverse(const ::test::ArrMap& root) {
	for (auto& p : root._()) {
		u32 += JunkHash(p.first);
		for (auto v : p.second._()) {
			f32 += v;
		}
	}
}

void Junk1::Traverse(const ::test::Main& root) {
	u32 += root.i32() + root.u32() + root.flag() + root.mode();
	u32 += root.t_i32() + root.t_s32() + root.t_u32();
	for (auto v : root.i32v()) {
		u32 += v;
	}
	for (auto v : root.flags()) {
		u32 += v;
	}
	u32 += JunkHash(root.str());
	u32 += JunkHash(root.data());
	for (auto& v : root.strv()) {
		u32 += JunkHash(v);
	}
	for (auto& v : root.datav()) {
		u32 += JunkHash(v);
	}

	u64 += root.i64() + root.u64();
	u64 += root.t_i64() + root.t_s64() + root.t_u64();
	for (auto v : root.u64v()) {
		u64 += v;
	}

	f32 += root.f32();
	for (auto v : root.f32v()) {
		f32 += v;
	}

	f64 += root.f64();
	for (auto v : root.f64v()) {
		f64 += v;
	}

	Traverse(root.object());
	for (auto& v : root.objectv()) {
		Traverse(v);
	}

	for (auto& p : root.index()) {
		u32 += JunkHash(p.first) + p.second;
	}

	for (auto& p : root.objects()) {
		u32 += p.first;
		Traverse(p.second);
	}

	Traverse(root.matrix());
	for (auto& v : root.vector()) {
		Traverse(v);
	}
	Traverse(root.arrays());
}

template <typename T, typename V>
static inline void CollectScalarVector(const google::protobuf::RepeatedFieldRef<T>& array, V& value) {
	for (auto v : array) {
		value += v;
	}
}

void Junk1::Traverse(const google::protobuf::Message& root) {
	auto descriptor = root.GetDescriptor();
	auto reflection = root.GetReflection();
	for (int i = 0; i < descriptor->field_count(); i++) {
		auto field = descriptor->field(i);
		if (field->is_repeated()) {
			auto n = reflection->FieldSize(root, field);
			if (n == 0) {
				continue;
			}
			switch (field->type()) {
				case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
					for (auto& obj : reflection->GetRepeatedFieldRef<google::protobuf::Message>(root, field)) {
						Traverse(obj);
					}
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
				case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
					for (auto& str : reflection->GetRepeatedFieldRef<std::string>(root, field)) {
						u32 += JunkHash(str);
					}
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
					CollectScalarVector(reflection->GetRepeatedFieldRef<double>(root, field), f64);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
					CollectScalarVector(reflection->GetRepeatedFieldRef<float>(root, field), f32);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
				case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
					CollectScalarVector(reflection->GetRepeatedFieldRef<uint64_t>(root, field), u64);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
				case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
					CollectScalarVector(reflection->GetRepeatedFieldRef<uint32_t>(root, field), u32);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
				case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
				case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
					CollectScalarVector(reflection->GetRepeatedFieldRef<int64_t>(root, field), u64);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
				case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
				case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
					CollectScalarVector(reflection->GetRepeatedFieldRef<int32_t>(root, field), u32);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
					CollectScalarVector(reflection->GetRepeatedFieldRef<bool>(root, field), u32);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
				{
					for (int j = 0; j < n; j++) {
						u32 += reflection->GetRepeatedEnumValue(root, field, j);
					}
				}
					break;
				default:
					break;
			}
		} else {
			if (!reflection->HasField(root, field)) {
				continue;
			}
			switch (field->type()) {
				case google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE:
					Traverse(reflection->GetMessage(root, field));
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_BYTES:
				case google::protobuf::FieldDescriptor::Type::TYPE_STRING:
					u32 += JunkHash(reflection->GetString(root, field));
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
					f64 += reflection->GetDouble(root, field);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_FLOAT:
					f32 += reflection->GetFloat(root, field);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_FIXED64:
				case google::protobuf::FieldDescriptor::Type::TYPE_UINT64:
					u64 += reflection->GetUInt64(root, field);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_FIXED32:
				case google::protobuf::FieldDescriptor::Type::TYPE_UINT32:
					u32 += reflection->GetUInt32(root, field);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED64:
				case google::protobuf::FieldDescriptor::Type::TYPE_SINT64:
				case google::protobuf::FieldDescriptor::Type::TYPE_INT64:
					u64 += reflection->GetInt64(root, field);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_SFIXED32:
				case google::protobuf::FieldDescriptor::Type::TYPE_SINT32:
				case google::protobuf::FieldDescriptor::Type::TYPE_INT32:
					u32 += reflection->GetInt32(root, field);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_BOOL:
					u32 += reflection->GetBool(root, field);
					break;
				case google::protobuf::FieldDescriptor::Type::TYPE_ENUM:
					u32 += reflection->GetEnumValue(root, field);
					break;
				default:
					break;
			}
		}
	}
}

int BenchmarkProtobuf() {
	std::string raw;
	if (!protocache::LoadFile("test.pb", &raw)) {
		puts("fail to load test.pb");
		return -1;
	}
	Junk1 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
#ifdef NO_PB_ARENA
		::test::Main root;
		if (!root.ParseFromString(raw)) {
			puts("fail to deserialize");
			return -2;
		}
		junk.Traverse(root);
#else
		google::protobuf::Arena arena;
		auto root = google::protobuf::Arena::CreateMessage<::test::Main>(&arena);
		if (!root->ParseFromString(raw)) {
			puts("fail to deserialize");
			return -2;
		}
		junk.Traverse(*root);
#endif
	}
	auto delta_ms = DeltaMs(start);

	printf("protobuf: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkProtobufReflect() {
	std::string raw;
	if (!protocache::LoadFile("test.pb", &raw)) {
		puts("fail to load test.pb");
		return -1;
	}
	Junk1 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		google::protobuf::Arena arena;
		google::protobuf::Message* root = google::protobuf::Arena::CreateMessage<::test::Main>(&arena);
		if (!root->ParseFromString(raw)) {
			puts("fail to deserialize");
			return -2;
		}
		junk.Traverse(*root);
	}
	auto delta_ms = DeltaMs(start);

	printf("protobuf: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}
