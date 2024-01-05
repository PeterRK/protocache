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
#include <google/protobuf/util/json_util.h>
#include <protocache/serialize.h>
#include <protocache/reflection.h>
#include <protocache/utils.h>
#include "test.pc.h"


TEST(Proto, Basic) {
	std::string err;
	google::protobuf::FileDescriptorProto file;
	ASSERT_TRUE(protocache::ParseProtoFile("test.proto", &file, &err));

	google::protobuf::DescriptorPool pool;
	ASSERT_NE(pool.BuildFile(file), nullptr);

	auto descriptor = pool.FindMessageTypeByName("test.Main");
	ASSERT_NE(descriptor, nullptr);

	google::protobuf::DynamicMessageFactory factory(&pool);
	auto prototype = factory.GetPrototype(descriptor);
	ASSERT_NE(prototype, nullptr);
	std::unique_ptr<google::protobuf::Message> message(prototype->New());

	ASSERT_TRUE(protocache::LoadJson("test.json", message.get()));
	auto data = protocache::Serialize(*message);
	ASSERT_FALSE(data.empty());

	auto end = data.data() + data.size();
	test::Main root(data.data());
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
	ASSERT_EQ(bytes.cast<char>(), expected_str);
	ASSERT_EQ(-2.1f, root.f32(end));
	ASSERT_EQ(1.0, root.f64(end));

	auto leaf = root.object(end);
	ASSERT_FALSE(!leaf);
	ASSERT_EQ(88, leaf.i32(end));
	ASSERT_FALSE(leaf.flag(end));
	expected_str = "tmp";
	ASSERT_EQ(leaf.str(end), expected_str);

	auto i32v = root.i32v(end);
	ASSERT_FALSE(!i32v);
	ASSERT_EQ(i32v.size(), 2);
	ASSERT_EQ(i32v[0], 1);
	ASSERT_EQ(i32v[1], 2);

	auto u64v = root.u64v(end);
	ASSERT_FALSE(!u64v);
	ASSERT_EQ(u64v.size(), 1);
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
	ASSERT_EQ(f32v.size(), 2);
	ASSERT_EQ(f32v[0], 1.1f);
	ASSERT_EQ(f32v[1], 2.2f);

	std::vector<double> expected_f64v = {9.9,8.8,7.7,6.6,5.5};
	auto f64v = root.f64v(end);
	ASSERT_FALSE(!f64v);
	ASSERT_EQ(f64v.size(), expected_f64v.size());
	for (unsigned i = 0; i < f64v.size(); i++) {
		ASSERT_EQ(f64v[i], expected_f64v[i]);
	}

	std::vector<double> expected_flags = {true,true,false,true,false,false,false};
	auto flags = root.flags(end);
	ASSERT_FALSE(!flags);
	ASSERT_EQ(flags.size(), expected_flags.size());
	for (unsigned i = 0; i < flags.size(); i++) {
		ASSERT_EQ(flags[i], expected_flags[i]);
	}

	auto objects = root.objectv(end);
	ASSERT_FALSE(!objects);
	ASSERT_EQ(objects.Size(), 3);
	ASSERT_EQ(objects[0].i32(end), 1);
	ASSERT_TRUE(objects[1].flag(end));
	expected_str = "good luck!";
	ASSERT_EQ(objects[2].str(end), expected_str);

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
		auto val = pair.Value(end);
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
	auto mit3 = vector[0].Find(protocache::Slice<char>("lv2"));
	ASSERT_NE(mit3, map3.end());
	auto vec = (*mit3).Value();
	ASSERT_EQ(vec.Size(), 2);
	ASSERT_EQ(vec[0], 21);
	ASSERT_EQ(vec[1], 22);

	auto map4 = root.arrays(end);
	ASSERT_FALSE(!map4);
	auto mit4 = map4.Find(protocache::Slice<char>("lv5"));
	ASSERT_NE(mit4, map3.end());
	vec = (*mit4).Value();
	ASSERT_EQ(vec.Size(), 2);
	ASSERT_EQ(vec[0], 51);
	ASSERT_EQ(vec[1], 52);
}

TEST(Proto, Reflection) {
	std::string err;
	google::protobuf::FileDescriptorProto file;
	ASSERT_TRUE(protocache::ParseProtoFile("test.proto", &file, &err));

	protocache::reflection::DescriptorPool pool;
	ASSERT_TRUE(pool.Register(file));

	auto root = pool.Find("test.Main");
	ASSERT_NE(root, nullptr);

	auto it = root->fields.find("f64");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_DOUBLE);

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

	google::protobuf::DescriptorPool pb;
	ASSERT_NE(pb.BuildFile(file), nullptr);

	auto descriptor = pb.FindMessageTypeByName("test.Main");
	ASSERT_NE(descriptor, nullptr);

	google::protobuf::DynamicMessageFactory factory(&pb);
	auto prototype = factory.GetPrototype(descriptor);
	ASSERT_NE(prototype, nullptr);
	std::unique_ptr<google::protobuf::Message> message(prototype->New());

	ASSERT_TRUE(protocache::LoadJson("test.json", message.get()));
	auto data = protocache::Serialize(*message);
	ASSERT_FALSE(data.empty());
	auto end = data.data() + data.size();
	protocache::Message unit(data.data());

	it = root->fields.find("f32");
	ASSERT_NE(it, root->fields.end());
	ASSERT_FALSE(it->second.repeated);
	ASSERT_EQ(it->second.value, protocache::reflection::Field::TYPE_FLOAT);
	ASSERT_EQ(-2.1f, protocache::GetFloat(unit, it->second.id, end));
}