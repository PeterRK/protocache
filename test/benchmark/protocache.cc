// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include "protocache/extension/utils.h"
#include "protocache/extension/reflection.h"
#include "test.pc.h"
#include "test.pc-ex.h"
#include "common.h"

static inline uint32_t JunkHash(const protocache::Slice<char>& data) {
	return JunkHash(data.data(), data.size());
}

static inline uint32_t JunkHash(const protocache::Slice<uint8_t>& data) {
	return JunkHash(data.data(), data.size());
}

struct Junk2 : public Junk {
	void Traverse(const ::test::Small& root);
	void Traverse(const ::test::Main& root);

	void Access(const protocache::reflection::Field& descriptor, protocache::Field field);
	void Traverse(const protocache::reflection::Field& descriptor, protocache::Field field);
	void Traverse(const protocache::reflection::Descriptor& descriptor, protocache::Message root);

	void Traverse(const ::ex::test::Small& root);
	void Traverse(const ::ex::test::Main& root);
};

inline void Junk2::Traverse(const ::test::Small& root) {
	u32 += root.i32() + root.flag();
	u32 += JunkHash(root.str());
}

void Junk2::Traverse(const ::test::Main& root) {
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
	for (auto v : root.strv()) {
		u32 += JunkHash(v);
	}
	for (auto v : root.datav()) {
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
	for (auto v : root.objectv()) {
		Traverse(v);
	}

	for (auto p : root.index()) {
		u32 += JunkHash(p.Key()) + p.Value();
	}

	for (auto p : root.objects()) {
		u32 += p.Key();
		Traverse(p.Value());
	}

	for (auto u : root.matrix()) {
		for (auto v : u) {
			f32 += v;
		}
	}
	for (auto u : root.vector()) {
		for (auto p : u) {
			u32 += JunkHash(p.Key());
			for (auto v : p.Value()) {
				f32 += v;
			}
		}
	}
	for (auto p : root.arrays()) {
		u32 += JunkHash(p.Key());
		for (auto v : p.Value()) {
			f32 += v;
		}
	}
}

void Junk2::Access(const protocache::reflection::Field& descriptor, protocache::Field field) {
	switch (descriptor.value) {
		case protocache::reflection::Field::TYPE_MESSAGE:
		{
			assert(descriptor.value_descriptor != nullptr);
			if (descriptor.value_descriptor->IsAlias()) {
				Traverse(descriptor.value_descriptor->alias, field);
			} else {
				Traverse(*descriptor.value_descriptor, protocache::Message(field.GetObject()));
			}
		}
			break;
		case protocache::reflection::Field::TYPE_BYTES:
			u32 += JunkHash(protocache::FieldT<protocache::Slice<uint8_t>>(field).Get());
			break;
		case protocache::reflection::Field::TYPE_STRING:
			u32 += JunkHash(protocache::FieldT<protocache::Slice<char>>(field).Get());
			break;
		case protocache::reflection::Field::TYPE_DOUBLE:
			f64 += protocache::FieldT<double>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_FLOAT:
			f32 += protocache::FieldT<float>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_UINT64:
			u64 += protocache::FieldT<uint64_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_UINT32:
			u32 += protocache::FieldT<uint32_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_INT64:
			u64 += protocache::FieldT<int64_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_INT32:
			u32 += protocache::FieldT<int32_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_BOOL:
			u32 += protocache::FieldT<bool>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_ENUM:
			u32 += protocache::FieldT<protocache::EnumValue>(field).Get();
			break;
		default:
			break;
	}
}

void Junk2::Traverse(const protocache::reflection::Field& descriptor, protocache::Field field) {
	if (descriptor.IsMap()) {
		for (auto p : protocache::Map(field.GetObject())) {
			switch (descriptor.key) {
				case protocache::reflection::Field::TYPE_STRING:
					u32 += JunkHash(protocache::FieldT<protocache::Slice<char>>(p.Key()).Get());
					break;
				case protocache::reflection::Field::TYPE_UINT64:
					u32 += protocache::FieldT<uint64_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_UINT32:
					u32 += protocache::FieldT<uint32_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_INT64:
					u32 += protocache::FieldT<int64_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_INT32:
					u32 += protocache::FieldT<int32_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_BOOL:
					u32 += protocache::FieldT<bool>(p.Key()).Get();
					break;
				default:
					break;
			}
			Access(descriptor, p.Value());
		}
		return;
	}
	if (descriptor.repeated) {
		switch (descriptor.value) {
			case protocache::reflection::Field::TYPE_MESSAGE:
			{
				assert(descriptor.value_descriptor != nullptr);
				if (descriptor.value_descriptor->IsAlias()) {
					for (auto v : protocache::Array(field.GetObject())) {
						Traverse(descriptor.value_descriptor->alias, v);
					}
				} else {
					for (auto v : protocache::ArrayT<protocache::Message>(field.GetObject())) {
						Traverse(*descriptor.value_descriptor, v);
					}
				}
			}
				break;
			case protocache::reflection::Field::TYPE_BYTES:
				for (auto u : protocache::Array(field.GetObject())) {
					u32 += JunkHash(protocache::FieldT<protocache::Slice<uint8_t>>(u).Get());
				}
				break;
			case protocache::reflection::Field::TYPE_STRING:
				for (auto u : protocache::Array(field.GetObject())) {
					u32 += JunkHash(protocache::FieldT<protocache::Slice<char>>(u).Get());
				}
				break;
			case protocache::reflection::Field::TYPE_DOUBLE:
				for (auto v : protocache::ArrayT<double>(field.GetObject())) {
					f64 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_FLOAT:
				for (auto v : protocache::ArrayT<float>(field.GetObject())) {
					f32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_UINT64:
				for (auto v : protocache::ArrayT<uint64_t>(field.GetObject())) {
					u64 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_UINT32:
				for (auto v : protocache::ArrayT<uint32_t>(field.GetObject())) {
					u32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_INT64:
				for (auto v : protocache::ArrayT<int64_t>(field.GetObject())) {
					u64 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_INT32:
				for (auto v : protocache::ArrayT<int32_t>(field.GetObject())) {
					u32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_BOOL:
				for (auto v : protocache::ArrayT<bool>(field.GetObject())) {
					u32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_ENUM:
				for (auto v : protocache::ArrayT<protocache::EnumValue>(field.GetObject())) {
					u32 += v;
				}
				break;
			default:
				break;
		}

	} else {
		Access(descriptor, field);
	}
}

void Junk2::Traverse(const protocache::reflection::Descriptor& descriptor, protocache::Message root) {
	for (auto& p : descriptor.fields) {
		auto field = root.GetField(p.second.id);
		if (!field) {
			continue;
		}
		Traverse(p.second, field);
	}
}

void Junk2::Traverse(const ::ex::test::Small& root) {
	u32 += root.i32 + root.flag;
	u32 += JunkHash(root.str);
}

void Junk2::Traverse(const ::ex::test::Main& root) {
	u32 += root.i32 + root.u32 + root.flag + root.mode;
	u32 += root.t_i32 + root.t_s32 + root.t_u32;
	for (auto v : root.i32v) {
		u32 += v;
	}
	for (auto v : root.flags) {
		u32 += v;
	}
	u32 += JunkHash(root.str);
	u32 += JunkHash(root.data);
	for (auto& v : root.strv) {
		u32 += JunkHash(v);
	}
	for (auto& v : root.datav) {
		u32 += JunkHash(v);
	}

	u64 += root.i64 + root.u64;
	u64 += root.t_i64 + root.t_s64 + root.t_u64;
	for (auto v : root.u64v) {
		u64 += v;
	}

	f32 += root.f32;
	for (auto v : root.f32v) {
		f32 += v;
	}

	f64 += root.f64;
	for (auto v : root.f64v) {
		f64 += v;
	}

	Traverse(root.object);
	for (auto& v : root.objectv) {
		Traverse(v);
	}

	for (auto& p : root.index) {
		u32 += JunkHash(p.first) + p.second;
	}

	for (auto p : root.objects) {
		u32 += p.first;
		Traverse(p.second);
	}

	for (auto& u : root.matrix) {
		for (auto v : u) {
			f32 += v;
		}
	}
	for (auto& u : root.vector) {
		for (auto& p : u) {
			u32 += JunkHash(p.first);
			for (auto v : p.second) {
				f32 += v;
			}
		}
	}
	for (auto& p : root.arrays) {
		u32 += JunkHash(p.first);
		for (auto v : p.second) {
			f32 += v;
		}
	}
}

int BenchmarkProtoCache() {
	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}
	Junk2 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		::test::Main root(reinterpret_cast<const uint32_t*>(raw.data()));
		junk.Traverse(root);
	}
	auto delta_ms = DeltaMs(start);

	printf("protocache: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkProtoCacheReflect() {
	std::string err;
	google::protobuf::FileDescriptorProto file;
	if(!protocache::ParseProtoFile("test.proto", &file, &err)) {
		puts("fail to load test.proto");
		return -1;
	}
	protocache::reflection::DescriptorPool pool;
	if (!pool.Register(file)) {
		puts("fail to prepare descriptor pool");
		return -2;
	}
	auto descriptor = pool.Find("test.Main");
	if (descriptor == nullptr) {
		puts("fail to get entry descriptor");
		return -2;
	}

	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}

	Junk2 junk;
	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		protocache::Message root(reinterpret_cast<const uint32_t*>(raw.data()));
		junk.Traverse(*descriptor, root);
	}
	auto delta_ms = DeltaMs(start);

	printf("protocache-reflect: %ldms %016lx\n", delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkProtoCacheEX() {
	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}
	Junk2 junk;

	protocache::Slice<uint32_t> view(reinterpret_cast<const uint32_t*>(raw.data()), raw.size()/4);

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		::ex::test::Main root(view);
		junk.Traverse(root);
	}
	auto delta_ms = DeltaMs(start);

	printf("protocache-ex: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkProtoCacheSerialize() {
	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}

	protocache::Slice<uint32_t> view(reinterpret_cast<const uint32_t*>(raw.data()), raw.size()/4);
	::ex::test::Main root(view);

	unsigned cnt = 0;
	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		cnt += root.Serialize().size();
	}
	auto delta_ms = DeltaMs(start);

	printf("protocache: %ldms %x\n", delta_ms, cnt);
	return 0;
}