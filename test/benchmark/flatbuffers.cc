// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include "protocache/extension/utils.h"
#include <flatbuffers/idl.h>
#include <flatbuffers/reflection.h>
#include "test_generated.h"
#include "common.h"

static inline uint32_t JunkHash(const flatbuffers::String& data) {
	return JunkHash(data.data(), data.size());
}

static inline uint32_t JunkHash(const flatbuffers::Vector<int8_t>& data) {
	return JunkHash(data.data(), data.size());
}

struct Junk3 : public Junk {
	void Traverse(const ::test::Small& root);
	void Traverse(const ::test::Vec2D& root);
	void Traverse(const ::test::ArrMap& root);
	void Traverse(const ::test::Main& root);

	void Traverse(const reflection::Schema& schema, const reflection::Object& descriptor, const flatbuffers::Table& table);
};

inline void Junk3::Traverse(const ::test::Small& root) {
	u32 += root.i32() + root.flag();
	auto str = root.str();
	if (str != nullptr) {
		u32 += JunkHash(*str);
	}
}

inline void Junk3::Traverse(const ::test::Vec2D& root) {
	for (auto u : *root._()) {
		for (auto v : *u->_()) {
			f32 += v;
		}
	}
}

inline void Junk3::Traverse(const ::test::ArrMap& root) {
	for (auto p : *root._()) {
		u32 += JunkHash(*p->key());
		for (auto v : *p->value()->_()) {
			f32 += v;
		}
	}
}

void Junk3::Traverse(const ::test::Main& root) {
	u32 += root.i32() + root.u32() + root.flag() + root.mode();
	u32 += root.t_i32() + root.t_s32() + root.t_u32();
	auto i32v = root.i32v();
	if (i32v != nullptr) {
		for (auto v : *i32v) {
			u32 += v;
		}
	}
	auto flags = root.flags();
	if (flags != nullptr) {
		for (auto v : *flags) {
			u32 += v;
		}
	}
	auto str = root.str();
	if (str != nullptr) {
		u32 += JunkHash(*str);
	}
	auto dat = root.data();
	if (dat != nullptr) {
		u32 += JunkHash(*dat);
	}
	auto strv = root.strv();
	if (strv != nullptr) {
		for (auto v : *strv) {
			if (v != nullptr) {
				u32 += JunkHash(*v);
			}
		}
	}
	auto datv = root.datav();
	if (datv != nullptr) {
		for (auto v : *datv) {
			if (v != nullptr) {
				dat = v->_();
				if (dat != nullptr) {
					u32 += JunkHash(*dat);
				}
			}
		}
	}

	u64 += root.i64() + root.u64();
	u64 += root.t_i64() + root.t_s64() + root.t_u64();
	auto u64v = root.u64v();
	if (u64v != nullptr) {
		for (auto v : *u64v) {
			u64 += v;
		}
	}

	f32 += root.f32();
	auto f32v = root.f32v();
	if (f32v != nullptr) {
		for (auto v : *f32v) {
			f32 += v;
		}
	}

	f64 += root.f64();
	auto f64v = root.f64v();
	if (f64v != nullptr) {
		for (auto v : *f64v) {
			f64 += v;
		}
	}

	auto obj = root.object();
	if (obj != nullptr) {
		Traverse(*obj);
	}
	auto objv = root.objectv();
	if (objv != nullptr) {
		for (auto v : *objv) {
			if (v != nullptr) {
				Traverse(*v);
			}
		}
	}

	auto index = root.index();
	if (index != nullptr) {
		for (auto p : *index) {
			u32 += JunkHash(*p->key()) + p->value();
		}
	}

	auto objs = root.objects();
	if (objs != nullptr) {
		for (auto p : *objs) {
			u32 += p->key();
			Traverse(*p->value());
		}
	}

	auto matrix = root.matrix();
	if (matrix != nullptr) {
		Traverse(*matrix);
	}

	auto vector = root.vector();
	if (vector != nullptr) {
		for (auto u : *vector) {
			Traverse(*u);
		}
	}

	auto arrays = root.arrays();
	if (arrays != nullptr) {
		Traverse(*arrays);
	}
}


template <typename T, typename V>
static inline void CollectScalarVector(const flatbuffers::Table& table, const reflection::Field& field, V& value) {
	auto vec = flatbuffers::GetFieldV<T>(table, field);
	if (vec == nullptr) {
		return;
	}
	for (auto v : *vec) {
		value += v;
	}
}

void Junk3::Traverse(const reflection::Schema& schema, const reflection::Object& descriptor, const flatbuffers::Table& table) {
	for (auto field : *descriptor.fields()) {
		switch (field->type()->base_type()) {
			case reflection::BaseType::Byte:
				u32 += flatbuffers::GetFieldI<int8_t>(table, *field);
				break;
			case reflection::BaseType::Bool:
				u32 += flatbuffers::GetFieldI<bool>(table, *field);
				break;
			case reflection::BaseType::Int:
				u32 += flatbuffers::GetFieldI<int32_t>(table, *field);
				break;
			case reflection::BaseType::UInt:
				u32 += flatbuffers::GetFieldI<uint32_t>(table, *field);
				break;
			case reflection::BaseType::Long:
				u64 += flatbuffers::GetFieldI<int64_t>(table, *field);
				break;
			case reflection::BaseType::ULong:
				u64 += flatbuffers::GetFieldI<uint64_t>(table, *field);
				break;
			case reflection::BaseType::Float:
				f32 += flatbuffers::GetFieldF<float>(table, *field);
				break;
			case reflection::BaseType::Double:
				f64 += flatbuffers::GetFieldF<double>(table, *field);
				break;
			case reflection::BaseType::String:
			{
				auto str = flatbuffers::GetFieldS(table, *field);
				if (str != nullptr) {
					u32 += JunkHash(*str);
				}
			}
				break;
			case reflection::BaseType::Obj:
			{
				auto obj = flatbuffers::GetFieldT(table, *field);
				if (obj != nullptr) {
					auto child = schema.objects()->Get(field->type()->index());
					Traverse(schema, *child, *obj);
				}
			}
				break;
			case reflection::BaseType::Vector:
				switch (field->type()->element()) {
					case reflection::BaseType::Byte:
					{
						auto vec = flatbuffers::GetFieldV<int8_t>(table, *field);
						if (vec != nullptr) {
							u32 += JunkHash(*vec);
						}
					}
						break;
					case reflection::BaseType::Bool:
						CollectScalarVector<bool>(table, *field, u32);
						break;
					case reflection::BaseType::Int:
						CollectScalarVector<int32_t>(table, *field, u32);
						break;
					case reflection::BaseType::UInt:
						CollectScalarVector<uint32_t>(table, *field, u32);
						break;
					case reflection::BaseType::Long:
						CollectScalarVector<int64_t>(table, *field, u64);
						break;
					case reflection::BaseType::ULong:
						CollectScalarVector<uint64_t>(table, *field, u64);
						break;
					case reflection::BaseType::Float:
						CollectScalarVector<float>(table, *field, f32);
						break;
					case reflection::BaseType::Double:
						CollectScalarVector<double>(table, *field, f64);
						break;
					case reflection::BaseType::String:
					{
						auto vec = flatbuffers::GetFieldV<flatbuffers::Offset<const flatbuffers::String>>(table, *field);
						if (vec != nullptr) {
							for (auto v : *vec) {
								u32 += JunkHash(*v);
							}
						}
					}
						break;
					case reflection::BaseType::Obj:
					{
						auto child = schema.objects()->Get(field->type()->index());
						auto vec = flatbuffers::GetFieldV<flatbuffers::Offset<const flatbuffers::Table>>(table, *field);
						if (vec != nullptr) {
							for (auto v : *vec) {
								Traverse(schema, *child, *v);
							}
						}
					}
						break;
					default:
						break;
				}
				break;
			default: break;
		}
	}
}


int BenchmarkFlatBuffers() {
	std::string raw;
	if (!protocache::LoadFile("test.fb", &raw)) {
		puts("fail to load test.fb");
		return -1;
	}
	Junk3 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		auto root = ::test::GetMain(raw.data());
		junk.Traverse(*root);
	}
	auto delta_ms = DeltaMs(start);

	printf("flatbuffers: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkFlatBuffersReflect() {
	std::string raw;
	if (!protocache::LoadFile("test.fbs", &raw)) {
		puts("fail to load test.fbs");
		return -1;
	}
	flatbuffers::Parser parser;
	if (!parser.Parse(raw.c_str())) {
		puts("fail to parse test.fbs");
		return -2;
	}
	parser.Serialize();
	auto schema = reflection::GetSchema(parser.builder_.GetBufferPointer());
	auto descriptor = schema->root_table();

	if (!protocache::LoadFile("test.fb", &raw)) {
		puts("fail to load test.fb");
		return -1;
	}
	Junk3 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		auto root = flatbuffers::GetAnyRoot(reinterpret_cast<const uint8_t*>(raw.data()));
		junk.Traverse(*schema, *descriptor, *root);
	}
	auto delta_ms = DeltaMs(start);

	printf("flatbuffers-reflect: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}



