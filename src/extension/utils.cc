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
#include "protocache/extension/utils.h"

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

static bool ParseProto(google::protobuf::io::ZeroCopyInputStream& input,
					   google::protobuf::FileDescriptorProto* result, std::string* err) {
	ErrorCollector collector;
	google::protobuf::io::Tokenizer tokenizer(&input, &collector);
	google::protobuf::compiler::Parser parser;
	auto done = parser.Parse(&tokenizer, result);
	if (!done && err != nullptr) {
		*err = collector.String();
	}
	return done;
}

bool ParseProto(const std::string& data, google::protobuf::FileDescriptorProto* result, std::string* err) {
	google::protobuf::io::ArrayInputStream stream(data.data(), data.size());
	return ParseProto(stream, result, err);
}

bool ParseProtoFile(const std::string& filename, google::protobuf::FileDescriptorProto* result, std::string* err) {
	std::ifstream input(filename);
	if (!input) {
		if (err != nullptr) {
			*err = "fail to open: " + filename;
		}
		return false;
	}
	result->set_name(filename);
	google::protobuf::io::IstreamInputStream stream(&input);
	return ParseProto(stream, result, err);
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

bool DumpJson(const google::protobuf::Message& message, const std::string& path) {
	std::string data;
	google::protobuf::util::JsonPrintOptions option;
	option.add_whitespace = true;
	option.preserve_proto_field_names = true;
	auto status = google::protobuf::util::MessageToJsonString(message, &data, option);
	if (!status.ok()) {
		return false;
	}
	std::ofstream ofs(path);
	if (!ofs) {
		return false;
	}
	return !!ofs.write(data.data(), data.size());
}

} // protocache