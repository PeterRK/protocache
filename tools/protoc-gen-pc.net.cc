// Copyright (c) 2025, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <unistd.h>
#include "proto-gen-utils.h"

static std::unordered_map<std::string, AliasUnit> g_alias_book;
static std::unordered_map<std::string, std::string> g_cs_names;

static void CollectAlias(const std::string& ns, const std::string& cs_ns, const ::google::protobuf::EnumDescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	auto cs_name = NaiveJoinName(cs_ns, proto.name());
	g_cs_names.emplace(fullname, cs_name);
}

static bool CollectAlias(const std::string& ns, const std::string& cs_ns, const ::google::protobuf::DescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	auto cs_name = NaiveJoinName(cs_ns, proto.name());
	g_cs_names.emplace(fullname, cs_name);
	for (auto& one : proto.enum_type()) {
		CollectAlias(fullname, cs_name, one);
	}
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;
	for (auto& one : proto.nested_type()) {
		if (one.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, one.name()), &one);
			continue;
		}
		if (!CollectAlias(fullname, cs_name, one)) {
			return false;
		}
	}
	if (!IsAlias(proto, true)) {
		if (proto.field_size() == 1 && IsRepeated(proto.field(0))) {
			std::cerr << fullname << " may be alias?" << std::endl;
		}
		return true;
	}
	// alias
	auto& field = proto.field(0);
	if (!IsRepeated(field)) {
		std::cerr << "illegal alias: " << fullname << std::endl;
		return false;
	}
	AliasUnit unit;
	unit.value_type = field.type();
	if (field.type() == ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE) {
		auto it = map_entries.find(field.type_name());
		if (it != map_entries.end()) {
			unit.key_type = it->second->field(0).type();
			unit.value_type = it->second->field(1).type();
			unit.value_class = it->second->field(1).type_name();
		} else {
			unit.value_class = field.type_name();
		}
	}
	g_alias_book.emplace(std::move(fullname), std::move(unit));
	return true;
}

static const char* BasicCsType(::google::protobuf::FieldDescriptorProto::Type type) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
			return "ProtoCache.Bytes";
		case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
			return "ProtoCache.String";
		case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
			return "ProtoCache.Float64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
			return "ProtoCache.Float32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
			return "ProtoCache.UInt64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
			return "ProtoCache.Int64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
			return "ProtoCache.UInt32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
			return "ProtoCache.Int32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
			return "ProtoCache.Bool";
		default:
			return nullptr;
	}
}

static std::string CalcAliasName(const AliasUnit& alias) {
	const char* value_type = nullptr;
	if (alias.value_type == ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE) {
		auto it = g_cs_names.find(alias.value_class);
		if (it == g_cs_names.end()) {
			std::cerr << "cannot find java name for: " << alias.value_class << std::endl;
			return {};
		}
		value_type = it->second.c_str();
	}
	std::string out;
	if (alias.key_type == TYPE_NONE) {
		if (value_type != nullptr) {
			out.reserve(128);
			out += "ProtoCache.ObjectArray<global::";
			out += value_type;
			out += '>';
		} else {
			value_type = BasicCsType(alias.value_type);
			if (value_type == nullptr) {
				return {};
			}
			out.reserve(64);
			out += value_type;
			out += "Array";
		}
	} else {
		auto key_type =  BasicCsType(alias.key_type);
		if (key_type == nullptr) {
			return {};
		}
		bool basic = false;
		if (value_type == nullptr) {
			value_type = BasicCsType(alias.value_type);
			if (value_type == nullptr) {
				return {};
			}
			basic = true;
		}
		out.reserve(160);
		out += key_type;
		out += "Dict<global::";
		out += value_type;
		if (basic) {
			out += "Value";
		}
		out += '>';
	}
	return out;
}


static std::string GenEnum(const ::google::protobuf::EnumDescriptorProto& proto) {
	std::ostringstream oss;
	oss << "public class " << proto.name() << " {\n";
	for (auto& value : proto.value()) {
		if (!value.options().deprecated()) {
			oss << "\tpublic const int " << value.name() << " = " << value.number() << ";\n";
		}
	}
	oss << "}\n\n";
	return oss.str();
}

static std::string GenMessage(const std::string& ns, const ::google::protobuf::DescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	std::ostringstream oss;
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;

	oss << "public class " << proto.name() << " : global::";

	if (IsAlias(proto, true)) {
		auto it = g_alias_book.find(fullname);
		if (it == g_alias_book.end()) {
			std::cerr << "alias lost: " << fullname << std::endl;
			return {};
		}
		auto cs_type = CalcAliasName(it->second);
		if (cs_type.empty()) {
			return {};
		}
		oss << cs_type;
	} else {
		oss << "ProtoCache.IUnit.Object";
	}
	oss << " {\n";

	for (auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			oss << AddIndent(GenEnum(one));
		}
	}
	for (auto& one : proto.nested_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, one.name()), &one);
			continue;
		}
		auto piece = GenMessage(fullname, one);
		if (piece.empty()) {
			std::cerr << "fail to gen code for message: " << one.name() << std::endl;
			return {};
		}
		oss << AddIndent(piece);
	}

	auto fields = FieldsInOrder(proto);
	if (IsAlias(proto, true) || fields.empty()) {
		oss << "}\n\n";
		return oss.str();
	}

	for (auto one : fields) {
		if (one->name() == "_") {
			std::cerr << "found illegal field in message " << fullname << std::endl;
			return {};
		}
		oss << "\tpublic const int _" << one->name() << " = " << (one->number()-1) << ";\n";
	}
	oss << "\n\tprivate global::ProtoCache.Message _core_;\n"
		<< "\tpublic " << proto.name() << "() {}\n"
		<< "\tpublic " << proto.name() << "(ReadOnlyMemory<byte> data) { Init(data); }\n"
		<< "\tpublic bool HasField(int id) { return _core_.HasField(id); }\n"
		<< "\tpublic override void Init(ReadOnlyMemory<byte> data) {\n"
		<< "\t\t_core_.Init(data);\n";
	for (auto one : fields) {
		if (IsRepeated(*one)
			|| one->type() == ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE
			|| one->type() == ::google::protobuf::FieldDescriptorProto::TYPE_STRING
			|| one->type() == ::google::protobuf::FieldDescriptorProto::TYPE_BYTES) {
			oss << "\t\t" << one->name() << "_ = null;\n";
		}
	}
	oss << "\t}\n\n";

	auto handle_simple_field = [&oss](
			const ::google::protobuf::FieldDescriptorProto& field,
			const char* raw_type, const char* boxed_type, bool primary=true) {
		if (IsRepeated(field)) {
			oss << "\tprivate global::ProtoCache." << boxed_type << "Array? " << field.name() << "_ = null;\n"
				<< "\tpublic global::ProtoCache." << boxed_type << "Array " << ToPascal(field.name()) << " { get {\n"
				<< "\t\t" << field.name() << "_ \?\?= _core_.GetObject<global::ProtoCache." << boxed_type << "Array>(_" << field.name() << ");\n"
				<< "\t\treturn " << field.name() << "_;\n"
				<< "\t}}\n";
		} else if (primary) {
			oss << "\tpublic " << raw_type << ' ' << ToPascal(field.name())
				<< " { get { return _core_.Get" << boxed_type << "(_" << field.name() << "); } }\n";
		} else {
			oss << "\tprivate " << raw_type << "? " << field.name() << "_ = null;\n"
				<< "\tpublic " << raw_type << ' ' << ToPascal(field.name()) << " { get {\n"
				<< "\t\t" << field.name() << "_ \?\?= _core_.Get" << boxed_type << "(_" << field.name() << ");\n"
				<< "\t\treturn " << field.name() << "_;\n"
				<< "\t}}\n";
		}
	};

	for (auto one : fields) {
		switch (one->type()) {
			case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
			{
				std::string cs_type;
				if (IsRepeated(*one)) {
					AliasUnit unit;
					unit.value_type = one->type();
					auto it = map_entries.find(one->type_name());
					if (it != map_entries.end()) {
						unit.key_type = it->second->field(0).type();
						unit.value_type = it->second->field(1).type();
						unit.value_class = it->second->field(1).type_name();
					} else {
						unit.value_class = one->type_name();
					}
					cs_type = CalcAliasName(unit);
				} else {
					auto it = g_cs_names.find(one->type_name());
					if (it != g_cs_names.end()) {
						cs_type = it->second;
					}
				}
				if (cs_type.empty()) {
					return {};
				}
				oss << "\tprivate global::" << cs_type << "? " << one->name() << "_ = null;\n"
					<< "\tpublic global::" << cs_type << ' ' << ToPascal(one->name()) << " { get {\n"
					<< "\t\t" << one->name() << "_ \?\?= _core_.GetObject<global::" << cs_type << ">(_" << one->name() << ");\n"
					<< "\t\treturn " << one->name() << "_;\n"
					<< "\t}}\n";
			}
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
				handle_simple_field(*one, "byte[]", "Bytes", false);
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
				handle_simple_field(*one, "string", "String", false);
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
				handle_simple_field(*one, "double", "Float64");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
				handle_simple_field(*one, "float", "Float32");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
				handle_simple_field(*one, "ulong", "UInt64");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
				handle_simple_field(*one, "long", "Int64");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
				handle_simple_field(*one, "uint", "UInt32");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
				handle_simple_field(*one, "int", "Int32");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
				handle_simple_field(*one, "bool", "Bool");
				break;
			default:
				std::cerr << "unsupported field " << one->name() << " in message " << proto.name() << std::endl;
				return {};
		}
	}

	oss << "}\n\n";
	return oss.str();
}

static std::string GenFile(const ::google::protobuf::FileDescriptorProto& proto) {
	std::ostringstream oss;
	oss << "namespace " << proto.options().csharp_namespace() << " {\n\n";

	for (auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			oss << GenEnum(one);
		}
	}

	std::string ns;
	if (!proto.package().empty()) {
		ns = '.' + proto.package();
	}
	for (auto& one : proto.message_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		auto piece = GenMessage(ns, one);
		if (piece.empty()) {
			std::cerr << "fail to gen code for message: " << one.name() << std::endl;
			return {};
		}
		oss << piece;
	}

	oss << "}\n";
	return oss.str();
}

static std::string ConvertFilename(const std::string& name) {
	std::string out;
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		out = name + ".pc.cs";
	} else {
		out = name.substr(0, pos+1) + "pc.cs";
	}
	out.front() = (char)std::toupper(out.front());
	return out;
}

int main() {
	::google::protobuf::compiler::CodeGeneratorRequest request;
	::google::protobuf::compiler::CodeGeneratorResponse response;

	if (!request.ParseFromFileDescriptor(STDIN_FILENO)) {
		std::cerr << "fail to get request" << std::endl;
		return 1;
	}

	for (auto& proto : request.proto_file()) {
		std::string ns;
		if (!proto.package().empty()) {
			ns = '.' + proto.package();
		}
		if (proto.options().csharp_namespace().empty()) {
			std::cerr << "cs namespace is unspecified in file: " << proto.name() << std::endl;
			return -1;
		}
		auto cs_ns = proto.options().csharp_namespace();
		for (auto& one : proto.enum_type()) {
			CollectAlias(ns, cs_ns, one);
		}
		for (auto& one : proto.message_type()) {
			if (!CollectAlias(ns, cs_ns, one)) {
				std::cerr << "fail to collect alias from file: " << proto.name() << std::endl;
				return -1;
			}
		}
	}

	std::unordered_set<std::string> files;
	files.reserve(request.file_to_generate_size());
	for (auto& one : request.file_to_generate()) {
		files.insert(one);
	}

	for (auto& proto : request.proto_file()) {
		if (files.find(proto.name()) == files.end()) {
			continue;
		}
		auto one = response.add_file();
		one->set_name(ConvertFilename(proto.name()));
		one->set_content(GenFile(proto));
		if (one->content().empty()) {
			std::cerr << "fail to generate code for file: " << proto.name() << std::endl;
			return -2;
		}
	}

	if (!response.SerializeToFileDescriptor(STDOUT_FILENO)) {
		std::cerr << "fail to response" << std::endl;
		return 2;
	}
	return 0;
}