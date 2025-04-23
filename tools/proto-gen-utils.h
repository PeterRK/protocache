#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <google/protobuf/descriptor.pb.h>
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

static std::vector<const ::google::protobuf::FieldDescriptorProto*> FieldsInOrder(const ::google::protobuf::DescriptorProto& proto) {
	std::vector<const ::google::protobuf::FieldDescriptorProto*> out;
	out.reserve(proto.field_size());
	for (auto& one : proto.field()) {
		if (!one.options().deprecated()) {
			out.push_back(&one);
		}
	}
	std::sort(out.begin(), out.end(),
			  [](const ::google::protobuf::FieldDescriptorProto* a, const ::google::protobuf::FieldDescriptorProto* b)->bool{
				  return a->number() < b->number();
			  });
	return out;
}

static inline bool IsRepeated(const ::google::protobuf::FieldDescriptorProto& proto) {
	return proto.label() == ::google::protobuf::FieldDescriptorProto::LABEL_REPEATED;
}

static inline bool IsAlias(const ::google::protobuf::DescriptorProto& proto, bool ext=false) {
	if (ext) {
		return proto.field_size() == 1 && (proto.field(0).name() == "_" || proto.field(0).name() == "_x_");
	}
	return proto.field_size() == 1 && proto.field(0).name() == "_";
}