#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <capnp/schema.h>
#include <capnp/message.h>
#include <capnp/dynamic.h>
#include <capnp/schema-parser.h>
#include <capnp/serialize.h>
#include <capnp/serialize-packed.h>
#include "protocache/extension/utils.h"
#include "test.capnp.h"
#include "common.h"

static inline uint32_t JunkHash(const capnp::Text::Reader& data) {
	return JunkHash(data.begin(), data.size());
}

static inline uint32_t JunkHash(const capnp::Data::Reader& data) {
	return JunkHash(data.begin(), data.size());
}

template <typename T>
::kj::ArrayPtr<const T> CastVector(const std::vector<T>& src) {
	return {src.data(), src.size()};
}

int CreateCapnFile() {
	// FIXME: how to convert from json?
	::capnp::MallocMessageBuilder message;

	auto root = message.initRoot<test::capn::Main>();
	root.setI32(-999);
	root.setU32(1234);
	root.setI64(-9876543210);
	root.setU64(98765432123456789);
	root.setFlag(true);
	root.setMode(test::capn::Mode::MODE_C);
	root.setStr("Hello World!");
	std::string tmp = "abc123!?$*&()'-=@~";
	root.setData(::capnp::Data::Reader(reinterpret_cast<const uint8_t*>(tmp.data()), tmp.size()));
	root.setF32(-2.1f);
	root.setF64(1.0);
	auto obj = root.initObject();
	obj.setI32(88);
	obj.setStr("tmp");
	root.setI32v(CastVector<int32_t>({1,2}));
	root.setU64v(CastVector<uint64_t>({12345678987654321}));
	auto strv = root.initStrv(10);
	strv.set(0, "abc");
	strv.set(1, "apple");
	strv.set(2, "banana");
	strv.set(3, "orange");
	strv.set(4, "pear");
	strv.set(5, "grape");
	strv.set(6, "strawberry");
	strv.set(7, "cherry");
	strv.set(8, "mango");
	strv.set(9, "watermelon");
	root.setF32v(CastVector<float>({1.1f, 2.2f}));
	root.setF64v(CastVector<double>({9.9,8.8,7.7,6.6,5.5}));
	auto flags = root.initFlags(7);
	flags.set(0, true);
	flags.set(1, true);
	flags.set(2, false);
	flags.set(3, true);
	flags.set(4, false);
	flags.set(5, false);
	flags.set(6, false);
	auto objv = root.initObjectv(3);
	objv[0].setI32(1);
	objv[1].setFlag(true);
	objv[2].setStr("good luck!");
	auto index = root.initIndex(6);
	index[0].setKey("abc-1");
	index[0].setValue(1);
	index[1].setKey("abc-2");
	index[1].setValue(2);
	index[2].setKey("x-1");
	index[2].setValue(1);
	index[3].setKey("x-2");
	index[3].setValue(2);
	index[4].setKey("x-3");
	index[4].setValue(3);
	index[5].setKey("x-4");
	index[5].setValue(4);
	auto objects = root.initObjects(4);
	objects[0].setKey(1);
	obj = objects[0].initValue();
	obj.setI32(1);
	obj.setStr("aaaaaaaaaaa");
	objects[1].setKey(2);
	obj = objects[1].initValue();
	obj.setI32(2);
	obj.setStr("b");
	objects[2].setKey(3);
	obj = objects[2].initValue();
	obj.setI32(3);
	obj.setStr("ccccccccccccccc");
	objects[3].setKey(4);
	obj = objects[3].initValue();
	obj.setI32(4);
	obj.setStr("ddddd");
	auto lines = root.initMatrix().initX(3);
	lines[0].setX(CastVector<float>({1,2,3}));
	lines[1].setX(CastVector<float>({4,5,6}));
	lines[2].setX(CastVector<float>({7,8,9}));
	auto vec = root.initVector(2);
	auto maps = vec[0].initX(2);
	maps[0].setKey("lv1");
	maps[0].initValue().setX(CastVector<float>({11,12}));
	maps[1].setKey("lv2");
	maps[1].initValue().setX(CastVector<float>({21,22}));
	maps = vec[1].initX(1);
	maps[0].setKey("lv3");
	maps[0].initValue().setX(CastVector<float>({31,32}));
	maps = root.initArrays().initX(2);
	maps[0].setKey("lv4");
	maps[0].initValue().setX(CastVector<float>({41,42}));
	maps[1].setKey("lv5");
	maps[1].initValue().setX(CastVector<float>({51,52}));

	::kj::VectorOutputStream out;
	writeMessage(out, message);
	auto view = out.getArray();
	std::ofstream ofs1("test.capn.bin");
	if (!ofs1.write(reinterpret_cast<const char*>(view.begin()), view.size())) {
		return -1;
	}

	out.clear();
	writePackedMessage(out, message);
	view = out.getArray();
	std::ofstream ofs2("test.capn-packed.bin");
	if (!ofs2.write(reinterpret_cast<const char*>(view.begin()), view.size())) {
		return -1;
	}
	return 0;
}

struct Junk4 : public Junk {
	void Traverse(const ::test::capn::Small::Reader& root);
	void Traverse(const ::test::capn::Vec2D::Reader& root);
	void Traverse(const ::test::capn::ArrMap::Reader& root);
	void Traverse(const ::test::capn::Main::Reader& root);

	void Traverse(const capnp::DynamicValue::Reader& root);
};

inline void Junk4::Traverse(const ::test::capn::Small::Reader& root) {
	u32 += root.getI32() + root.getFlag();
	u32 += JunkHash(root.getStr());
}

inline void Junk4::Traverse(const ::test::capn::Vec2D::Reader& root) {
	for (auto u : root.getX()) {
		for (auto v : u.getX()) {
			f32 += v;
		}
	}
}

inline void Junk4::Traverse(const ::test::capn::ArrMap::Reader& root) {
	for (auto p : root.getX()) {
		u32 += JunkHash(p.getKey());
		for (auto v : p.getValue().getX()) {
			f32 += v;
		}
	}
}

void Junk4::Traverse(const ::test::capn::Main::Reader& root) {
	u32 += root.getI32() + root.getU32() + root.getFlag() + (uint32_t)root.getMode();
	u32 += root.getTI32() + root.getTS32() + root.getTU32();
	for (auto v : root.getI32v()) {
		u32 += v;
	}
	for (auto v : root.getFlags()) {
		u32 += v;
	}
	u32 += JunkHash(root.getStr());
	u32 += JunkHash(root.getData());
	for (auto v : root.getStrv()) {
		if (v != nullptr) {
			u32 += JunkHash(v);
		}
	}
	for (auto v : root.getDatav()) {
		u32 += JunkHash(v);
	}

	u64 += root.getI64() + root.getU64();
	u64 += root.getTI64() + root.getTS64() + root.getTU64();
	for (auto v : root.getU64v()) {
		u64 += v;
	}

	f32 += root.getF32();
	for (auto v : root.getF32v()) {
		f32 += v;
	}

	f64 += root.getF64();
	for (auto v : root.getF64v()) {
		f64 += v;
	}

	Traverse(root.getObject());
	for (auto v : root.getObjectv()) {
		Traverse(v);
	}

	for (auto p : root.getIndex()) {
		u32 += JunkHash(p.getKey()) + p.getValue();
	}

	for (auto p : root.getObjects()) {
		u32 += p.getKey();
		Traverse(p.getValue());
	}

	Traverse(root.getMatrix());

	for (auto u : root.getVector()) {
		Traverse(u);
	}
	Traverse(root.getArrays());
}

void Junk4::Traverse(const capnp::DynamicValue::Reader& value) {
	switch (value.getType()) {
		case capnp::DynamicValue::BOOL:
			u32 += value.as<bool>();
			break;
		// FIXME: no int32/uint32/float ?
		case capnp::DynamicValue::INT:
			u64 += value.as<int64_t>();
			break;
		case capnp::DynamicValue::UINT:
			u64 += value.as<uint64_t>();
			break;
		case capnp::DynamicValue::FLOAT:
			f64 += value.as<double>();
			break;
		case capnp::DynamicValue::TEXT:
			u32 += JunkHash(value.as<capnp::Text>());
			break;
		case capnp::DynamicValue::LIST: {
			for (auto v: value.as<capnp::DynamicList>()) {
				Traverse(v);
			}
			break;
		}
		case capnp::DynamicValue::ENUM:
			u32 += value.as<capnp::DynamicEnum>().getRaw();
			break;
		case capnp::DynamicValue::STRUCT: {
			auto v = value.as<capnp::DynamicStruct>();
			for (auto field: v.getSchema().getFields()) {
				if (!v.has(field)) continue;
				Traverse(v.get(field));
			}
		}
			break;
		default:
			break;
	}
}

int BenchmarkCapnProto(bool packed) {
	std::string raw;
	if (!protocache::LoadFile(packed? "test.capn-packed.bin" : "test.capn.bin", &raw)) {
		puts("fail to load test.capn.bin");
		return -1;
	}
	Junk4 junk;

	auto start = std::chrono::steady_clock::now();
	if (packed) {
		for (size_t i = 0; i < kLoop; i++) {
			::kj::ArrayInputStream input(::kj::ArrayPtr<const uint8_t>(reinterpret_cast<const uint8_t*>(raw.data()), raw.size()));
			::capnp::PackedMessageReader message(input);
			auto root = message.getRoot<test::capn::Main>();
			junk.Traverse(root);
		}
	} else {
		for (size_t i = 0; i < kLoop; i++) {
			::kj::ArrayPtr<const capnp::word> view(reinterpret_cast<const capnp::word*>(raw.data()), raw.size()/sizeof(capnp::word));
			::capnp::FlatArrayMessageReader message(view);
			auto root = message.getRoot<test::capn::Main>();
			junk.Traverse(root);
		}
	}
	auto delta_ms = DeltaMs(start);

	printf("capnproto%s: %luB %ldms %016lx\n", packed? "-packed" : "",
		   raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkCapnProtoReflect(bool packed) {
	auto schema = capnp::Schema::from<test::capn::Main>().asStruct();

	std::string raw;
	if (!protocache::LoadFile(packed? "test.capn-packed.bin" : "test.capn.bin", &raw)) {
		puts("fail to load test.capn.bin");
		return -1;
	}
	Junk4 junk;

	auto start = std::chrono::steady_clock::now();
	if (packed) {
		for (size_t i = 0; i < kLoop; i++) {
			::kj::ArrayInputStream input(::kj::ArrayPtr<const uint8_t>(reinterpret_cast<const uint8_t*>(raw.data()), raw.size()));
			::capnp::PackedMessageReader message(input);
			auto root = message.getRoot<capnp::DynamicStruct>(schema);
			junk.Traverse(root);
		}
	} else {
		for (size_t i = 0; i < kLoop; i++) {
			::kj::ArrayPtr<const capnp::word> view(reinterpret_cast<const capnp::word*>(raw.data()), raw.size()/sizeof(capnp::word));
			::capnp::FlatArrayMessageReader message(view);
			auto root = message.getRoot<capnp::DynamicStruct>(schema);
			junk.Traverse(root);
		}
	}
	auto delta_ms = DeltaMs(start);

	printf("capnproto%s-reflect: %luB %ldms %016lx\n", packed? "-packed" : "",
		   raw.size(), delta_ms, junk.Fuse());
	return 0;
}