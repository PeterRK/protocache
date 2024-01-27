#pragma once

#include <string>
#include <vector>
#include <google/protobuf/compiler/plugin.pb.h>

static inline std::string NaiveJoinName(const std::string& ns, const std::string& name) {
	std::string fullname;
	fullname.reserve(ns.size() + 1 + name.size());
	fullname += ns;
	fullname += '.';
	fullname += name;
	return fullname;
}

static void Split(const std::string& raw, char delim, std::vector<std::string>* out) {
	unsigned begin = 0;
	for (unsigned i = 0; i < raw.size(); i++) {
		if (raw[i] == delim) {
			out->push_back(raw.substr(begin, i-begin));
			begin = i+1;
		}
	}
	out->push_back(raw.substr(begin));
}

static std::string AddIndent(const std::string& raw) {
	std::string out;
	out.reserve(raw.size()+raw.size()/8);
	if (!raw.empty() && raw.front() != '\n') {
		out.push_back('\t');
	}
	for (auto str = raw.c_str(); *str; str++) {
		out += *str;
		if (*str == '\n' && (str[1] != '\n' && str[1] != '\0')) {
			out += '\t';
		}
	}
	return out;
}

static constexpr ::google::protobuf::FieldDescriptorProto::Type TYPE_NONE = static_cast<::google::protobuf::FieldDescriptorProto::Type>(0);

struct AliasUnit {
	::google::protobuf::FieldDescriptorProto::Type key_type = TYPE_NONE;
	::google::protobuf::FieldDescriptorProto::Type value_type = TYPE_NONE;
	std::string value_class;
};