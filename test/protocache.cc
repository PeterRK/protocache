// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <iostream>
#include <fstream>
#include <gtest/gtest.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/util/json_util.h>
#include "serialize.h"
#include "access.h"
#include "test.pb.h"

static bool ParseJsonToProto(const std::string& data, ::google::protobuf::Message* message) {
	::google::protobuf::util::JsonParseOptions option;
	option.ignore_unknown_fields = true;
	auto status = ::google::protobuf::util::JsonStringToMessage(data, message, option);
	if (!status.ok()) {
		std::cerr << "JsonStringToMessage: " << status << std::endl;
		return false;
	}
	return true;
}

static bool LoadFile(const std::string& path, std::string* out) {
	std::ifstream ifs(path, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	if (!ifs) {
		std::cerr << "fail to load file: " << path << std::endl;
		return false;
	}
	size_t size = ifs.tellg();
	ifs.seekg(0);
	out->resize(size);
	if (!ifs.read(const_cast<char*>(out->data()), size)) {
		return false;
	}
	return true;
}

static bool LoadJson(const std::string& path, ::google::protobuf::Message* message) {
	std::string raw;
	return LoadFile(path, &raw) && ParseJsonToProto(raw, message);
}

TEST(Proto, Basic) {
	google::protobuf::Arena arena;
	auto message = google::protobuf::Arena::CreateMessage<::test::Main>(&arena);
	ASSERT_TRUE(LoadJson("test.json", message));
	auto data = protocache::Serialize(*message);
	ASSERT_FALSE(data.empty());

	auto end = data.data() + data.size();
	protocache::Message root(data.data());
	ASSERT_FALSE(!root);
	ASSERT_EQ(root.GetMagic(), protocache::CalcMagic("test.Main"));

	ASSERT_EQ(-999, protocache::GetInt32(root.GetField(0, end), end));
	ASSERT_EQ(1234, protocache::GetUInt32(root.GetField(1, end), end));
	ASSERT_EQ(-9876543210LL, protocache::GetInt64(root.GetField(2, end), end));
	ASSERT_EQ(98765432123456789ULL, protocache::GetUInt64(root.GetField(3, end), end));
	ASSERT_TRUE(protocache::GetBool(root.GetField(4, end), end));
	ASSERT_EQ(2, protocache::GetEnumValue(root.GetField(5, end), end));
	std::string expected_str = "Hello World!";
	auto str = protocache::GetString(root.GetField(6, end), end);
	ASSERT_EQ(str, expected_str);
	expected_str = "abc123!?$*&()'-=@~";
	auto bytes = protocache::GetBytes(root.GetField(7, end), end);
	ASSERT_EQ(bytes.cast<char>(), expected_str);
	ASSERT_EQ(-2.1f, protocache::GetFloat(root.GetField(8, end), end));
	ASSERT_EQ(1.0, protocache::GetDouble(root.GetField(9, end), end));

	auto leaf = protocache::GetMessage(root.GetField(10, end), end);
	ASSERT_FALSE(!leaf);
	auto identity = protocache::CalcMagic("test.Small");
	ASSERT_EQ(leaf.GetMagic(), identity);
	ASSERT_EQ(88, protocache::GetInt32(leaf.GetField(0, end), end));
	ASSERT_FALSE(protocache::GetBool(leaf.GetField(1, end), end));
	expected_str = "tmp";
	str = protocache::GetString(leaf.GetField(2, end), end);
	ASSERT_EQ(str, expected_str);

	auto i32v = protocache::GetInt32Array(root.GetField(11, end), end);
	ASSERT_FALSE(!i32v);
	ASSERT_EQ(i32v.size(), 2);
	ASSERT_EQ(i32v[0], 1);
	ASSERT_EQ(i32v[1], 2);

	auto u64v = protocache::GetUInt64Array(root.GetField(12, end), end);
	ASSERT_FALSE(!u64v);
	ASSERT_EQ(u64v.size(), 1);
	ASSERT_EQ(u64v[0], 12345678987654321ULL);

	std::vector<std::string> expected_strv = {"abc","apple","banana","orange","pear","grape",
											  "strawberry","cherry","mango","watermelon"};
	auto strv = protocache::GetArray(root.GetField(13, end), end);
	ASSERT_FALSE(!strv);
	ASSERT_EQ(strv.Size(), expected_strv.size());
	auto sit = expected_strv.begin();
	for (const auto& field : strv) {
		auto str = protocache::GetString(field);
		ASSERT_FALSE(!str);
		ASSERT_EQ(str, *sit++);
	}

	auto f32v = protocache::GetFloatArray(root.GetField(15, end), end);
	ASSERT_FALSE(!f32v);
	ASSERT_EQ(f32v.size(), 2);
	ASSERT_EQ(f32v[0], 1.1f);
	ASSERT_EQ(f32v[1], 2.2f);

	std::vector<double> expected_f64v = {9.9,8.8,7.7,6.6,5.5};
	auto f64v = protocache::GetDoubleArray(root.GetField(16, end), end);
	ASSERT_FALSE(!f64v);
	ASSERT_EQ(f64v.size(), expected_f64v.size());
	for (unsigned i = 0; i < f64v.size(); i++) {
		ASSERT_EQ(f64v[i], expected_f64v[i]);
	}

	std::vector<double> expected_flags = {true,true,false,true,false,false,false};
	auto flags = protocache::GetBoolArray(root.GetField(17, end), end);
	ASSERT_FALSE(!flags);
	ASSERT_EQ(flags.size(), expected_flags.size());
	for (unsigned i = 0; i < flags.size(); i++) {
		ASSERT_EQ(flags[i], expected_flags[i]);
	}

	auto objects = protocache::GetArray(root.GetField(18, end), end);
	ASSERT_FALSE(!objects);
	ASSERT_EQ(objects.Size(), 3);
	ASSERT_EQ(protocache::GetInt32(protocache::GetMessage(objects[0], end).GetField(0), end), 1);
	ASSERT_TRUE(protocache::GetBool(protocache::GetMessage(objects[1], end).GetField(1), end));
	expected_str = "good luck!";
	str = protocache::GetString(protocache::GetMessage(objects[2], end).GetField(2), end);
	ASSERT_EQ(str, expected_str);

	ASSERT_EQ(0, protocache::GetUInt32(root.GetField(19)));
	ASSERT_EQ(0, protocache::GetInt32(root.GetField(20)));
	ASSERT_TRUE(!root.GetField(21));
	ASSERT_EQ(0, protocache::GetUInt64(root.GetField(22)));
	ASSERT_EQ(0, protocache::GetInt64(root.GetField(23)));
	ASSERT_TRUE(!root.GetField(24));

	auto map1 = protocache::GetMap(root.GetField(25, end), end);
	ASSERT_FALSE(!map1);
	ASSERT_EQ(map1.Size(), 6);
	auto mit = map1.Find("abc-1");
	ASSERT_NE(mit, map1.end());
	ASSERT_EQ(1, protocache::GetInt32((*mit).Value(), end));
	mit = map1.Find("abc-2");
	ASSERT_NE(mit, map1.end());
	ASSERT_EQ(2, protocache::GetInt32((*mit).Value(), end));
	mit = map1.Find("abc-3");
	ASSERT_EQ(mit, map1.end());
	mit = map1.Find("abc-4");
	ASSERT_EQ(mit, map1.end());

	auto map2 = protocache::GetMap(root.GetField(26, end), end);
	ASSERT_FALSE(!map2);
	ASSERT_EQ(map2.Size(), 4);
	for (const auto& pair : map2) {
		auto key = protocache::GetInt32(pair.Key(), end);
		ASSERT_NE(key, 0);
		auto val = protocache::GetMessage(pair.Value(), end);
		ASSERT_FALSE(!val);
		ASSERT_EQ(key, protocache::GetInt32(val.GetField(0, end)));
	}
}