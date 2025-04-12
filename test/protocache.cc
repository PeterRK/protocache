// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <fstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <google/protobuf/message.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/descriptor.pb.h>
#include "protocache/extension/reflection.h"
#include "protocache/extension/utils.h"
#include "test.pc.h"
#include "test.pc-ex.h"

TEST(PtotoCache, Empty) {
	// no crash
	protocache::String str;
	ASSERT_TRUE(!str);
	auto t0 = str.Get();
	ASSERT_EQ(t0.size(), 0);
	protocache::Message obj;
	ASSERT_TRUE(!obj);
	ASSERT_TRUE(!obj.GetField(0));
	protocache::Array arr;
	ASSERT_TRUE(!arr);
	ASSERT_EQ(arr.Size(), 0);
	ASSERT_EQ(arr.begin(), arr.end());
	protocache::Map map;
	ASSERT_TRUE(!map);
	ASSERT_EQ(map.Size(), 0);
	ASSERT_EQ(map.begin(), map.end());
	ASSERT_EQ(map.Find(0), map.end());
	protocache::Field field;
	ASSERT_TRUE(!field);
	ASSERT_EQ(protocache::FieldT<int32_t>(field).Get(), 0);
	ASSERT_EQ(protocache::FieldT<bool>(field).Get(), false);
	ASSERT_EQ(protocache::FieldT<protocache::Slice<bool>>(field).Get().size(), 0);
	ASSERT_EQ(protocache::FieldT<protocache::Slice<char>>(field).Get().size(), 0);
	ASSERT_TRUE(!protocache::FieldT<protocache::Array>(field).Get());
	ASSERT_TRUE(!protocache::FieldT<protocache::Message>(field).Get());
}

static bool SerializeByProtobuf(const std::string& json, protocache::Buffer& buf) {
	std::string err;
	google::protobuf::FileDescriptorProto file;
	if (!protocache::ParseProtoFile("test.proto", &file, &err)) {
		std::cerr << "fail to parse proto: " << err << std::endl;
		return false;
	}

	google::protobuf::DescriptorPool pool(google::protobuf::DescriptorPool::generated_pool());
	if (pool.BuildFile(file) == nullptr) {
		std::cerr << "fail to prepare protobuf pool" << std::endl;
		return false;
	}

	auto descriptor = pool.FindMessageTypeByName("test.Main");
	if (descriptor == nullptr) {
		std::cerr << "fail to get root" << std::endl;
		return false;
	}

	google::protobuf::DynamicMessageFactory factory(&pool);
	auto prototype = factory.GetPrototype(descriptor);
	if (prototype == nullptr) {
		std::cerr << "fail to create protobuf" << std::endl;
		return false;
	}
	std::unique_ptr<google::protobuf::Message> message(prototype->New());

	if (!protocache::LoadJson(json, message.get())) {
		std::cerr << "fail to load json: " << json << std::endl;
		return false;
	}
	return protocache::Serialize(*message, &buf);
}


TEST(PtotoCache, Basic) {
	protocache::Buffer buffer;
	ASSERT_TRUE(SerializeByProtobuf("test.json", buffer));
	auto data = buffer.View();
	ASSERT_EQ(195, data.size());
	auto end = data.data() + data.size();
	auto& root = *protocache::Message(data).Cast<test::Main>();
	ASSERT_FALSE(!root);

	ASSERT_EQ(-999, root.i32(end));
	ASSERT_EQ(1234, root.u32(end));
	ASSERT_EQ(-9876543210LL, root.i64(end));
	ASSERT_EQ(98765432123456789ULL, root.u64(end));
	ASSERT_TRUE(root.flag(end));
	ASSERT_EQ(test::Mode::MODE_C, root.mode(end));
	std::string expected_str = "Hello World!";
	ASSERT_EQ(root.str(end), expected_str);
	expected_str = "abc123!?$*&()'-=@~";
	auto bytes = root.data(end);
	ASSERT_EQ(protocache::SliceCast<char>(bytes), expected_str);
	ASSERT_EQ(-2.1f, root.f32(end));
	ASSERT_EQ(1.0, root.f64(end));

	auto& leaf = *root.object(end);
	ASSERT_FALSE(!leaf);
	ASSERT_EQ(88, leaf.i32(end));
	ASSERT_FALSE(leaf.flag(end));
	expected_str = "tmp";
	ASSERT_EQ(leaf.str(end), expected_str);

	auto i32v = root.i32v(end);
	ASSERT_FALSE(!i32v);
	ASSERT_EQ(i32v.Size(), 2);
	ASSERT_EQ(i32v[0], 1);
	ASSERT_EQ(i32v[1], 2);

	auto u64v = root.u64v(end);
	ASSERT_FALSE(!u64v);
	ASSERT_EQ(u64v.Size(), 1);
	ASSERT_EQ(u64v[0], 12345678987654321ULL);

	std::vector<std::string> expected_strv = {"abc","apple","banana","orange","pear","grape",
											  "strawberry","cherry","mango","watermelon"};
	auto strv = root.strv(end);
	ASSERT_FALSE(!strv);
	ASSERT_EQ(strv.Size(), expected_strv.size());
	auto sit = expected_strv.begin();
	for (const auto& one : strv) {
		ASSERT_EQ(one, *sit++);
	}

	auto f32v = root.f32v(end);
	ASSERT_FALSE(!f32v);
	ASSERT_EQ(f32v.Size(), 2);
	ASSERT_EQ(f32v[0], 1.1f);
	ASSERT_EQ(f32v[1], 2.2f);

	std::vector<double> expected_f64v = {9.9,8.8,7.7,6.6,5.5};
	auto f64v = root.f64v(end);
	ASSERT_FALSE(!f64v);
	ASSERT_EQ(f64v.Size(), expected_f64v.size());
	for (unsigned i = 0; i < f64v.Size(); i++) {
		ASSERT_EQ(f64v[i], expected_f64v[i]);
	}

	std::vector<double> expected_flags = {true,true,false,true,false,false,false};
	auto flags = root.flags(end);
	ASSERT_FALSE(!flags);
	ASSERT_EQ(flags.Size(), expected_flags.size());
	for (unsigned i = 0; i < flags.Size(); i++) {
		ASSERT_EQ(flags[i], expected_flags[i]);
	}

	auto objects = root.objectv(end);
	ASSERT_FALSE(!objects);
	ASSERT_EQ(objects.Size(), 3);
	ASSERT_EQ(objects[0]->i32(end), 1);
	ASSERT_TRUE(objects[1]->flag(end));
	expected_str = "good luck!";
	ASSERT_EQ(objects[2]->str(end), expected_str);

	ASSERT_TRUE(root.HasField(::test::Main::_::objectv));
	ASSERT_FALSE(root.HasField(::test::Main::_::t_i32));

	auto map1 = root.index(end);
	ASSERT_FALSE(!map1);
	ASSERT_EQ(map1.Size(), 6);
	auto mit = map1.Find(protocache::Slice<char>("abc-1"));
	ASSERT_NE(mit, map1.end());
	ASSERT_EQ(1, (*mit).Value(end));
	mit = map1.Find(protocache::Slice<char>("abc-2"));
	ASSERT_NE(mit, map1.end());
	ASSERT_EQ(2, (*mit).Value(end));
	mit = map1.Find(protocache::Slice<char>("abc-3"));
	ASSERT_EQ(mit, map1.end());
	mit = map1.Find(protocache::Slice<char>("abc-4"));
	ASSERT_EQ(mit, map1.end());

	auto map2 = root.objects(end);
	ASSERT_FALSE(!map2);
	ASSERT_EQ(map2.Size(), 4);
	for (const auto& pair : map2) {
		auto key = pair.Key(end);
		ASSERT_NE(key, 0);
		auto& val = *pair.Value(end);
		ASSERT_FALSE(!val);
		ASSERT_EQ(key, val.i32(end));
	}

	auto matrix = root.matrix(end);
	ASSERT_FALSE(!matrix);
	ASSERT_EQ(matrix.Size(), 3);
	ASSERT_EQ(matrix[2].Size(), 3);
	ASSERT_EQ(matrix[2][2], 9);

	auto vector = root.vector(end);
	ASSERT_FALSE(!vector);
	ASSERT_EQ(vector.Size(), 2);
	auto map3 = vector[0];
	ASSERT_EQ(map3.Size(), 2);
	auto mit3 = map3.Find(protocache::Slice<char>("lv2"));
	ASSERT_NE(mit3, map3.end());
	auto vec = (*mit3).Value();
	ASSERT_EQ(vec.Size(), 2);
	ASSERT_EQ(vec[0], 21);
	ASSERT_EQ(vec[1], 22);

	auto map4 = root.arrays(end);
	ASSERT_FALSE(!map4);
	auto mit4 = map4.Find(protocache::Slice<char>("lv5"));
	ASSERT_NE(mit4, map4.end());
	vec = (*mit4).Value();
	ASSERT_EQ(vec.Size(), 2);
	ASSERT_EQ(vec[0], 51);
	ASSERT_EQ(vec[1], 52);
}

TEST(PtotoCache, Alias) {
	protocache::Buffer buffer;
	ASSERT_TRUE(SerializeByProtobuf("test-alias.json", buffer));
	auto data = buffer.View();
	ASSERT_EQ(data.size(), 12);
	ASSERT_EQ(data[4], 0xd);
	ASSERT_EQ(data[5], 1);
	ASSERT_EQ(data[6], 1);
}

TEST(PtotoCache, Reflection) {
	std::string err;
	google::protobuf::FileDescriptorProto file;
	ASSERT_TRUE(protocache::ParseProtoFile("reflect-test.proto", &file, &err));

	protocache::reflection::DescriptorPool pool;
	ASSERT_TRUE(pool.Register(file));

	auto root = pool.Find("test.Main");
	ASSERT_NE(root, nullptr);
	ASSERT_EQ(root->tags.size(), 1);
	ASSERT_NE(root->tags.find("test_b"), root->tags.end());

	file = google::protobuf::FileDescriptorProto(); // fail without rese
	ASSERT_TRUE(protocache::ParseProtoFile("test.proto", &file, &err));

	auto it = root->fields.find("f64");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_DOUBLE);
	ASSERT_EQ(it->second.tags.size(), 1);
	ASSERT_NE(it->second.tags.find("mark"), it->second.tags.end());

	it = root->fields.find("strv");
	ASSERT_NE(it, root->fields.end());
	ASSERT_TRUE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_STRING);

	it = root->fields.find("mode");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_ENUM);

	it = root->fields.find("object");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_MESSAGE);
	auto object = pool.Find(it->second.value_type);
	ASSERT_NE(object, nullptr);
	ASSERT_FALSE(object->IsAlias());
	it = object->fields.find("flag");
	ASSERT_NE(it, object->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_BOOL);

	it = root->fields.find("index");
	ASSERT_NE(it, root->fields.end());
	ASSERT_TRUE(it->second.repeated);
	ASSERT_TRUE(it->second.IsMap());

	it = root->fields.find("matrix");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_MESSAGE);
	object = pool.Find(it->second.value_type);
	ASSERT_NE(object, nullptr);
	ASSERT_TRUE(object->IsAlias());
	ASSERT_TRUE(object->alias.repeated);
	ASSERT_FALSE(object->alias.IsMap());
	ASSERT_EQ(object->alias.value, protocache::reflection::Field::TYPE_MESSAGE);
	object = pool.Find(object->alias.value_type);
	ASSERT_NE(object, nullptr);
	ASSERT_TRUE(object->IsAlias());
	ASSERT_TRUE(object->alias.repeated);
	ASSERT_FALSE(object->alias.IsMap());
	ASSERT_EQ(object->alias.value, protocache::reflection::Field::TYPE_FLOAT);

	it = root->fields.find("arrays");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_MESSAGE);
	object = pool.Find(it->second.value_type);
	ASSERT_NE(object, nullptr);
	ASSERT_TRUE(object->IsAlias());
	ASSERT_TRUE(object->alias.repeated);
	ASSERT_TRUE(object->alias.IsMap());
	ASSERT_EQ(object->alias.key, protocache::reflection::Field::TYPE_STRING);
	ASSERT_EQ(object->alias.value, protocache::reflection::Field::TYPE_MESSAGE);
	object = pool.Find(object->alias.value_type);
	ASSERT_NE(object, nullptr);
	ASSERT_TRUE(object->IsAlias());
	ASSERT_TRUE(object->alias.repeated);
	ASSERT_FALSE(object->alias.IsMap());
	ASSERT_EQ(object->alias.value, protocache::reflection::Field::TYPE_FLOAT);

	google::protobuf::DescriptorPool pb(google::protobuf::DescriptorPool::generated_pool());
	ASSERT_NE(pb.BuildFile(file), nullptr);

	auto descriptor = pb.FindMessageTypeByName("test.Main");
	ASSERT_NE(descriptor, nullptr);

	google::protobuf::DynamicMessageFactory factory(&pb);
	auto prototype = factory.GetPrototype(descriptor);
	ASSERT_NE(prototype, nullptr);
	std::unique_ptr<google::protobuf::Message> message(prototype->New());

	ASSERT_TRUE(protocache::LoadJson("test.json", message.get()));
	protocache::Buffer buffer;
	ASSERT_TRUE(protocache::Serialize(*message, &buffer));
	auto data = buffer.View();
	auto end = data.data() + data.size();
	protocache::Message unit(data);

	it = root->fields.find("f32");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_FLOAT);
	ASSERT_EQ(-2.1f, protocache::GetField<float>(unit, it->second.id, end));
}

TEST(PtotoCacheEX, Basic) {
	protocache::Buffer buffer;
	ASSERT_TRUE(SerializeByProtobuf("test.json", buffer));
	auto data = buffer.View();
	ASSERT_FALSE(data.empty());
	::ex::test::Main root(data);

	ASSERT_EQ(-999, root.i32());
	ASSERT_EQ(1234, root.u32());
	ASSERT_EQ(-9876543210LL, root.i64());
	ASSERT_EQ(98765432123456789ULL, root.u64());
	ASSERT_TRUE(root.flag());
	ASSERT_EQ(test::Mode::MODE_C, root.mode());
	ASSERT_EQ(root.str(), "Hello World!");
	ASSERT_EQ(root.data(), "abc123!?$*&()'-=@~");
	ASSERT_EQ(-2.1f, root.f32());
	ASSERT_EQ(1.0, root.f64());

	ASSERT_EQ(88, root.object()->i32());
	ASSERT_FALSE(root.object()->flag());
	ASSERT_EQ(root.object()->str(), "tmp");

	ASSERT_EQ(root.i32v().size(), 2);
	ASSERT_EQ(root.i32v()[0], 1);
	ASSERT_EQ(root.i32v()[1], 2);

	ASSERT_EQ(root.u64v().size(), 1);
	ASSERT_EQ(root.u64v()[0], 12345678987654321ULL);

	std::vector<std::string> expected_strv = {"abc","apple","banana","orange","pear","grape",
											  "strawberry","cherry","mango","watermelon"};

	ASSERT_EQ(root.strv().size(), expected_strv.size());
	auto sit = expected_strv.begin();
	for (const auto& one : root.strv()) {
		ASSERT_EQ(one, *sit++);
	}

	ASSERT_EQ(root.f32v().size(), 2);
	ASSERT_EQ(root.f32v()[0], 1.1f);
	ASSERT_EQ(root.f32v()[1], 2.2f);

	std::vector<double> expected_f64v = {9.9,8.8,7.7,6.6,5.5};
	ASSERT_EQ(root.f64v().size(), expected_f64v.size());
	for (unsigned i = 0; i < root.f64v().size(); i++) {
		ASSERT_EQ(root.f64v()[i], expected_f64v[i]);
	}

	std::vector<double> expected_flags = {true,true,false,true,false,false,false};
	ASSERT_EQ(root.flags().size(), expected_flags.size());
	for (unsigned i = 0; i < root.flags().size(); i++) {
		ASSERT_EQ(root.flags()[i], expected_flags[i]);
	}

	ASSERT_EQ(root.objectv().size(), 3);
	ASSERT_EQ(root.objectv()[0]->i32(), 1);
	ASSERT_TRUE(root.objectv()[1]->flag());
	ASSERT_EQ(root.objectv()[2]->str(), "good luck!");

	ASSERT_EQ(root.index().size(), 6);
	auto mit = root.index().find("abc-1");
	ASSERT_NE(mit, root.index().end());
	ASSERT_EQ(1, mit->second);
	mit  = root.index().find("abc-2");
	ASSERT_NE(mit, root.index().end());
	ASSERT_EQ(2,  mit->second);
	mit = root.index().find("abc-3");
	ASSERT_EQ(mit, root.index().end());
	mit  = root.index().find("abc-4");
	ASSERT_EQ(mit, root.index().end());

	ASSERT_EQ(root.objects().size(), 4);
	for (auto& pair : root.objects()) {
		ASSERT_NE(pair.first, 0);
		ASSERT_EQ(pair.first, pair.second->i32());
	}

	ASSERT_EQ(root.matrix().size(), 3);
	ASSERT_EQ(root.matrix()[2].size(), 3);
	ASSERT_EQ(root.matrix()[2][2], 9);

	ASSERT_EQ(root.vector().size(), 2);
	auto& map3 = root.vector()[0];
	ASSERT_EQ(map3.size(), 2);
	auto mit3 = map3.find("lv2");
	ASSERT_NE(mit3, map3.end());
	auto& vec3 = mit3->second;
	ASSERT_EQ(vec3.size(), 2);
	ASSERT_EQ(vec3[0], 21);
	ASSERT_EQ(vec3[1], 22);

	auto mit4 = root.arrays().find("lv5");
	ASSERT_NE(mit4, root.arrays().end());
	auto& vec4 = mit4->second;
	ASSERT_EQ(vec4.size(), 2);
	ASSERT_EQ(vec4[0], 51);
	ASSERT_EQ(vec4[1], 52);

	// check compile error
	::ex::test::CyclicA cyclic;
}

TEST(PtotoCacheEX, Serialize) {
	protocache::Buffer buffer;
	ASSERT_TRUE(SerializeByProtobuf("test.json", buffer));
	auto data = buffer.View();
	auto end = data.data() + data.size();
	::ex::test::Main ex(data);

	ex.objectv();
	ex.strv(end)[0] = "xyz";
	ex.matrix(end)[1][1] = 999;

	for (auto& pair : ex.objects(end)) {
		pair.second->i32(end) = pair.first + 1;
	}

	protocache::Buffer buf;
	ASSERT_TRUE(ex.Serialize(&buf));
	auto modified = buf.View();
	ASSERT_EQ(data.size(), modified.size());
	end = modified.data() + modified.size();
	::ex::test::Main root(modified);

	ASSERT_EQ(root.str(end), "Hello World!");
	ASSERT_EQ(root.data(end), "abc123!?$*&()'-=@~");
	ASSERT_EQ(-2.1f, root.f32(end));
	ASSERT_EQ(1.0, root.f64(end));

	std::vector<std::string> expected_strv = {"xyz","apple","banana","orange","pear","grape",
											  "strawberry","cherry","mango","watermelon"};

	ASSERT_EQ(root.strv(end).size(), expected_strv.size());
	auto sit = expected_strv.begin();
	for (const auto& one : root.strv(end)) {
		ASSERT_EQ(one, *sit++);
	}

	ASSERT_EQ(root.matrix(end).size(), 3);
	ASSERT_EQ(root.matrix(end)[1][1], 999);
	ASSERT_EQ(root.matrix(end)[2][2], 9);

	ASSERT_EQ(root.objects(end).size(), 4);
	for (auto& pair : root.objects(end)) {
		ASSERT_EQ(pair.first+1, pair.second->i32(end));
	}

	auto mit4 = root.arrays().find("lv5");
	ASSERT_NE(mit4, root.arrays().end());
	auto& vec4 = mit4->second;
	ASSERT_EQ(vec4.size(), 2);
	ASSERT_EQ(vec4[0], 51);
	ASSERT_EQ(vec4[1], 52);
}

TEST(PtotoCacheEX, Alias) {
	::ex::test::Main root;
	root.object()->i32() = 0;
	auto& matrix = root.matrix();
	matrix.resize(3);
	matrix[2].resize(3);
	protocache::Buffer buf;
	ASSERT_TRUE(root.Serialize(&buf));
	auto view = buf.View();
	ASSERT_EQ(view.size(), 12);
	ASSERT_EQ(view[4], 0xd);
	ASSERT_EQ(view[5], 1);
	ASSERT_EQ(view[6], 1);
}

TEST(Compress, All) {
	protocache::Buffer buffer;
	ASSERT_TRUE(SerializeByProtobuf("test.json", buffer));
	auto data = buffer.View();

	protocache::Slice<uint8_t> view(reinterpret_cast<const uint8_t*>(data.data()), data.size()*4);
	std::string cooked;
	protocache::Compress(view.data(), view.size(), &cooked);
	ASSERT_NE(0, cooked.size());
	ASSERT_LT(cooked.size(), view.size());

	std::string raw;
	ASSERT_TRUE(protocache::Decompress(cooked, &raw));
	ASSERT_EQ(protocache::SliceCast<char>(view), protocache::Slice<char>(raw));
}
