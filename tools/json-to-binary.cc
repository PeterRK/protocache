// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <iostream>
#include <fstream>
#include <memory>
#include <gflags/gflags.h>
#include <google/protobuf/dynamic_message.h>
#include "protocache/extension/utils.h"

DEFINE_string(input, "data.json", "input file");
DEFINE_string(output, "data.bin", "output file");
DEFINE_string(schema, "schema.proto", "schema file");
DEFINE_string(root, "", "root message name");
DEFINE_bool(flat, true, "output protocache binary instead of protobuf binary");
DEFINE_bool(compress, false, "compress flat binary");

int main(int argc, char* argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);

	if (FLAGS_root.empty()) {
		std::cerr << "need root message name" << std::endl;
		return 1;
	}

	std::string err;
	google::protobuf::FileDescriptorProto file;
	if (!protocache::ParseProtoFile(FLAGS_schema, &file, &err)) {
		std::cerr << "fail to load schema:\n" << err << std::endl;
		return -1;
	}
	google::protobuf::DescriptorPool pool(google::protobuf::DescriptorPool::generated_pool());
	if (pool.BuildFile(file) == nullptr) {
		std::cerr << "fail to prepare descriptor pool" << std::endl;
		return -1;
	}
	auto descriptor = pool.FindMessageTypeByName(FLAGS_root);
	if (descriptor == nullptr) {
		std::cerr << "fail to find root message: " << FLAGS_root << std::endl;
		return -2;
	}
	google::protobuf::DynamicMessageFactory factory(&pool);
	auto prototype = factory.GetPrototype(descriptor);
	if (prototype == nullptr) {
		std::cerr << "fail to create root message: " << FLAGS_root << std::endl;
		return -2;
	}
	std::unique_ptr<google::protobuf::Message> message(prototype->New());

	if (!protocache::LoadJson(FLAGS_input, message.get())) {
		std::cerr << "fail to load json: " << FLAGS_input << std::endl;
		return -3;
	}
	std::ofstream output(FLAGS_output);
	if (!output) {
		std::cerr << "fail to open file for output: " << FLAGS_output << std::endl;
		return 2;
	}
	if (FLAGS_flat) {
		protocache::Data data;
		if (!protocache::Serialize(*message, &data)) {
			std::cerr << "fail to serialize" << std::endl;
			return -4;
		}
		if (FLAGS_compress) {
			std::string cooked;
			protocache::Compress(reinterpret_cast<const uint8_t*>(data.data()), data.size()*4U, &cooked);
			output.write(cooked.data(), cooked.size());
		} else {
			output.write(reinterpret_cast<const char*>(data.data()), data.size()*4U);
		}
	} else {
		auto data = message->SerializePartialAsString();
		output.write(data.data(), data.size());
	}
	return 0;
}