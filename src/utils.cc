// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <string>
#include <fstream>
#include <sstream>
#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "protocache/utils.h"

namespace protocache {

class ErrorCollector : public google::protobuf::io::ErrorCollector {
public:
	void AddError(int line, int column, const std::string& message) override {
		core_ << "[error] line " << line << ", column " << column << ": " << message << std::endl;
	}

	void AddWarning(int line, int column, const std::string& message) override {
		core_ << "[warning] line " << line << ", column " << column << ": " << message << std::endl;
	}

	std::string String() noexcept {
		return core_.str();
	}

private:
	std::ostringstream core_;
};

bool ParseProtoFile(std::string filename, google::protobuf::FileDescriptorProto* result, std::string* err) {
	std::ifstream input(filename);
	if (!input) {
		if (err != nullptr) {
			*err = "fail to open: " + filename;
		}
		return false;
	}
	google::protobuf::io::IstreamInputStream stream(&input);

	ErrorCollector collector;
	google::protobuf::io::Tokenizer tokenizer(&stream, &collector);
	google::protobuf::compiler::Parser parser;
	result->set_name(filename);
	auto done = parser.Parse(&tokenizer, result);
	if (!done && err != nullptr) {
		*err = collector.String();
	}
	return done;
}

bool LoadFile(const std::string& path, std::string* out) {
	std::ifstream ifs(path, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	if (!ifs) {
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

bool LoadJson(const std::string& path, google::protobuf::Message* message) {
	std::string data;
	if (!protocache::LoadFile(path, &data)) {
		return false;
	}
	google::protobuf::util::JsonParseOptions option;
	option.ignore_unknown_fields = true;
	auto status = google::protobuf::util::JsonStringToMessage(data, message, option);
	return status.ok();
}

} // protocache