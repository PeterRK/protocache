// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>
#include "fory/serialization/fory.h"
#include "protocache/extension/utils.h"
#include "common.h"
#include "test_fory.h"

namespace test_fory {

using Fory = ::fory::serialization::Fory;

static bool RegisterForyTypes(Fory& fory) {
	register_types(fory);
	return true;
}

static Fory NewFory() {
	return Fory::builder()
		.xlang(true)
		.track_ref(true)
		.compatible(true)
		.build();
}

static Small MakeSmall(int32_t i32, bool flag, const std::string& str) {
	Small small;
	small.set_i32(i32);
	small.set_flag(flag);
	small.set_str(str);
	return small;
}

static Vec2D::Vec1D MakeVec1D(std::initializer_list<float> values) {
	Vec2D::Vec1D vec;
	vec.mutable_x()->assign(values);
	return vec;
}

static ArrMap::Array MakeArray(std::initializer_list<float> values) {
	ArrMap::Array array;
	array.mutable_x()->assign(values);
	return array;
}

static ArrMap MakeArrMap(std::initializer_list<std::pair<const char*, ArrMap::Array>> entries) {
	ArrMap arr;
	for (const auto& entry : entries) {
		arr.mutable_x()->emplace(entry.first, entry.second);
	}
	return arr;
}

static Main CreateForyObject() {
	Main root;
	root.set_i32(-999);
	root.set_u32(1234);
	root.set_i64(-9876543210);
	root.set_u64(98765432123456789ULL);
	root.set_flag(true);
	root.set_mode(Mode::C);
	root.set_str("Hello World!");
	const std::string tmp = "abc123!?$*&()'-=@~";
	root.mutable_data()->assign(tmp.begin(), tmp.end());
	root.set_f32(-2.1f);
	root.set_f64(1.0);
	*root.mutable_object() = MakeSmall(88, false, "tmp");
	*root.mutable_i32v() = {1, 2};
	*root.mutable_u64v() = {12345678987654321ULL};
	*root.mutable_strv() = {
		"abc",
		"apple",
		"banana",
		"orange",
		"pear",
		"grape",
		"strawberry",
		"cherry",
		"mango",
		"watermelon",
	};
	*root.mutable_f32v() = {1.1f, 2.2f};
	*root.mutable_f64v() = {9.9, 8.8, 7.7, 6.6, 5.5};
	*root.mutable_flags() = {true, true, false, true, false, false, false};
	*root.mutable_objectv() = {
		MakeSmall(1, false, ""),
		MakeSmall(0, true, ""),
		MakeSmall(0, false, "good luck!"),
	};
	root.set_t_u32(0);
	root.set_t_i32(0);
	root.set_t_s32(0);
	root.set_t_u64(0);
	root.set_t_i64(0);
	root.set_t_s64(0);
	*root.mutable_index() = {
		{"abc-1", 1},
		{"abc-2", 2},
		{"x-1", 1},
		{"x-2", 2},
		{"x-3", 3},
		{"x-4", 4},
	};
	*root.mutable_objects() = {
		{1, MakeSmall(1, false, "aaaaaaaaaaa")},
		{2, MakeSmall(2, false, "b")},
		{3, MakeSmall(3, false, "ccccccccccccccc")},
		{4, MakeSmall(4, false, "ddddd")},
	};
	*root.mutable_matrix()->mutable_x() = {
		MakeVec1D({1, 2, 3}),
		MakeVec1D({4, 5, 6}),
		MakeVec1D({7, 8, 9}),
	};
	*root.mutable_vector() = {
		MakeArrMap({{"lv1", MakeArray({11, 12})}, {"lv2", MakeArray({21, 22})}}),
		MakeArrMap({{"lv3", MakeArray({31, 32})}}),
	};
	*root.mutable_arrays() = MakeArrMap({
		{"lv4", MakeArray({41, 42})},
		{"lv5", MakeArray({51, 52})},
	});
	return root;
}

static int CreateForyFile() {
	auto fory = NewFory();
	if (!RegisterForyTypes(fory)) {
		return -1;
	}
	auto result = fory.serialize(CreateForyObject());
	if (!result.ok()) {
		std::fprintf(stderr, "fory serialize failed: %s\n", result.error().to_string().c_str());
		return -2;
	}
	std::ofstream ofs("test.fr", std::ios::binary);
	const auto& data = result.value();
	if (!ofs.write(reinterpret_cast<const char*>(data.data()), data.size())) {
		return -3;
	}
	return 0;
}

static bool LoadForyFile(std::string* raw) {
	if (protocache::LoadFile("test.fr", raw)) {
		return true;
	}
	puts("fail to load test.fr");
	return false;
}

struct Junk5 : public Junk {
	void Traverse(const Small& root);
	void Traverse(const Vec2D& root);
	void Traverse(const ArrMap& root);
	void Traverse(const Main& root);
};

inline void Junk5::Traverse(const Small& root) {
	u32 += root.i32() + root.flag();
	u32 += JunkHash(root.str());
}

inline void Junk5::Traverse(const Vec2D& root) {
	for (auto& u : root.x()) {
		for (auto v : u.x()) {
			f32 += v;
		}
	}
}

inline void Junk5::Traverse(const ArrMap& root) {
	for (auto& p : root.x()) {
		u32 += JunkHash(p.first);
		for (auto v : p.second.x()) {
			f32 += v;
		}
	}
}

void Junk5::Traverse(const Main& root) {
	u32 += root.i32() + root.u32() + root.flag() + static_cast<uint32_t>(root.mode());
	u32 += root.t_i32() + root.t_s32() + root.t_u32();
	for (auto v : root.i32v()) {
		u32 += v;
	}
	for (auto v : root.flags()) {
		u32 += v;
	}
	u32 += JunkHash(root.str());
	u32 += JunkHash(root.data().data(), root.data().size());
	for (auto& v : root.strv()) {
		u32 += JunkHash(v);
	}
	for (auto& v : root.datav()) {
		u32 += JunkHash(v.data(), v.size());
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

} // namespace test_fory

int BenchmarkFory() {
	std::string raw;
	if (!test_fory::LoadForyFile(&raw)) {
		return -1;
	}

	auto fory = test_fory::NewFory();
	if (!test_fory::RegisterForyTypes(fory)) {
		return -2;
	}
	test_fory::Junk5 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		auto root = fory.deserialize<test_fory::Main>(
			reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
		if (!root.ok()) {
			std::fprintf(stderr, "fory deserialize failed: %s\n", root.error().to_string().c_str());
			return -3;
		}
		junk.Traverse(root.value());
	}
	auto delta_ms = DeltaMs(start);

	printf("fory: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkForySerialize() {
	auto fory = test_fory::NewFory();
	if (!test_fory::RegisterForyTypes(fory)) {
		return -1;
	}
	auto root = test_fory::CreateForyObject();

	unsigned cnt = 0;
	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		auto data = fory.serialize(root);
		if (!data.ok()) {
			std::fprintf(stderr, "fory serialize failed: %s\n", data.error().to_string().c_str());
			return -2;
		}
		cnt += data.value().size();
	}
	auto delta_ms = DeltaMs(start);
	printf("fory-serialize: %ldms %x\n", delta_ms, cnt);
	return 0;
}
