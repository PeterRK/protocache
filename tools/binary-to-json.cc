// Copyright (c) 2025, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <iostream>
#include <fstream>
#include <memory>
#include <gflags/gflags.h>
#include <google/protobuf/dynamic_message.h>
#include "protocache/extension/utils.h"

DEFINE_string(input, "data.bin", "input file");
DEFINE_string(output, "data.json", "output file");
DEFINE_string(schema, "schema.proto", "schema file");
DEFINE_string(root, "", "root message name");
DEFINE_bool(flat, true, "input protocache binary instead of protobuf binary");

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

	std::string raw;
	if (!protocache::LoadFile(FLAGS_input, &raw)) {
		std::cerr << "fail to load binary: " << FLAGS_input << std::endl;
		return -3;
	}

	if (FLAGS_flat) {
		protocache::Slice<uint32_t> view(reinterpret_cast<const uint32_t *>(raw.data()), raw.size() / sizeof(uint32_t));
		if (!protocache::Deserialize(view, message.get())) {
			std::cerr << "fail to deserialize:" << std::endl;
			return -4;
		}
	} else {
		if (!message->ParseFromString(raw)) {
			std::cerr << "fail to deserialize:" << std::endl;
			return -4;
		}
	}

	if (!protocache::DumpJson(*message, FLAGS_output)) {
		std::cerr << "fail to dump: " << FLAGS_output << std::endl;
		return 2;
	}
	return 0;
}