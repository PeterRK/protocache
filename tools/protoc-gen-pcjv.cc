// Copyright (c) 2023, Ruan Kunliang.
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

static std::unordered_map<std::string, std::string> g_java_names;

static void CollectAlias(const std::string& ns, const std::string& java_ns, const ::google::protobuf::EnumDescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	auto java_name = NaiveJoinName(java_ns, proto.name());
	g_java_names.emplace(fullname, java_name);
}

static bool CollectAlias(const std::string& ns, const std::string& java_ns, const ::google::protobuf::DescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	auto java_name = NaiveJoinName(java_ns, proto.name());
	g_java_names.emplace(fullname, java_name);
	for (auto& one : proto.enum_type()) {
		CollectAlias(fullname, java_name, one);
	}
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;
	for (auto& one : proto.nested_type()) {
		if (one.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, one.name()), &one);
			continue;
		}
		if (!CollectAlias(fullname, java_name, one)) {
			return false;
		}
	}
	if (proto.field_size() != 1 || proto.field(0).name() != "_") {
		return true;
	}
	// alias
	auto& field = proto.field(0);
	if (field.label() != ::google::protobuf::FieldDescriptorProto::LABEL_REPEATED) {
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

static std::string GenEnum(const ::google::protobuf::EnumDescriptorProto& proto, const std::string* package = nullptr) {
	std::ostringstream oss;
	if (package != nullptr) {
		oss << "package " << *package << ";\n\n"
			<< "public final class " << proto.name() << " {\n";
	} else {
		oss << "public final static class " << proto.name() << " {\n";
	}
	for (auto& value : proto.value()) {
		if (value.options().deprecated()) {
			continue;
		}
		oss << "\tpublic static int " << value.name() << " = " << value.number() << ";\n";
	}
	oss << "}\n";
	return oss.str();
}

static const char* BasicJavaType(::google::protobuf::FieldDescriptorProto::Type type) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
			return "com.github.peterrk.protocache.Bytes";
		case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
			return "com.github.peterrk.protocache.Str";
		case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
			return "com.github.peterrk.protocache.Float64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
			return "com.github.peterrk.protocache.Float32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
			return "com.github.peterrk.protocache.Int64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
			return "com.github.peterrk.protocache.Int32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
			return "com.github.peterrk.protocache.Bool";
		default:
			return nullptr;
	}
}

static std::string CalcAliasName(const AliasUnit& alias) {
	const char* value_type = nullptr;
	if (alias.value_type == ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE) {
		auto it = g_java_names.find(alias.value_class);
		if (it == g_java_names.end()) {
			std::cerr << "cannot find java name for: " << alias.value_class << std::endl;
			return {};
		}
		value_type = it->second.c_str();
	}
	std::string out;
	if (alias.key_type == TYPE_NONE) {
		if (value_type != nullptr) {
			out.reserve(128);
			out += "com.github.peterrk.protocache.Array<";
			out += value_type;
			out += '>';
		} else {
			value_type = BasicJavaType(alias.value_type);
			if (value_type == nullptr) {
				return {};
			}
			out.reserve(64);
			out += value_type;
			out += "Array";
		}
	} else {
		auto key_type =  BasicJavaType(alias.key_type);
		if (key_type == nullptr) {
			return {};
		}
		if (value_type == nullptr) {
			value_type = BasicJavaType(alias.value_type);
			if (value_type == nullptr) {
				return {};
			}
		}
		out.reserve(160);
		out += key_type;
		out += "Dict<";
		out += value_type;
		out += '>';
	}
	return out;
}

static std::string JavaFieldName(const std::string& name) {
	std::string out;
	out.reserve(name.size());
	bool word_end = true;
	for (auto ch : name) {
		if (ch == '_') {
			word_end = true;
			continue;
		}
		if (std::isalpha(ch)) {
			if (word_end) {
				out += std::toupper(ch);
			} else {
				out += ch;
			}
			word_end = false;
		} else {
			out += ch;
			word_end = true;
		}
	}
	return out;
}

static std::string GenMessage(const std::string& ns, const ::google::protobuf::DescriptorProto& proto, const std::string* package = nullptr) {
	auto fullname = NaiveJoinName(ns, proto.name());

	std::ostringstream oss;
	if (package != nullptr) {
		oss << "package " << *package << ";\n\n"
			<< "public final class " << proto.name() << " extends ";
	} else {
		oss << "public final static class " << proto.name() << " extends ";
	}

	auto alias_mode = proto.field_size() == 1 && proto.field(0).name() == "_";

	if (alias_mode) {
		auto it = g_alias_book.find(fullname);
		if (it == g_alias_book.end()) {
			std::cerr << "alias lost: " << fullname << std::endl;
			return {};
		}
		auto java_type =  CalcAliasName(it->second);
		if (java_type.empty()) {
			return {};
		}
		oss << java_type;
	} else {
		oss << "com.github.peterrk.protocache.Message";
	}
	oss << " {\n";

	for (auto& one : proto.enum_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		oss << AddIndent(GenEnum(one));
	}
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;
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

	if (alias_mode) {
		oss << "}\n";
		return oss.str();
	}

	oss << '\n';
	for (auto& one : proto.field()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.name() == "_") {
			std::cerr << "found illegal field in message " << fullname << std::endl;
			return {};
		}
		oss << "\tpublic static final int FIELD_" << one.name() << " = " << (one.number()-1) << ";\n";
	}
	oss << "\n\tpublic " << proto.name() << "(){}\n"
		<< "\tpublic " << proto.name() << "(byte[] data){ this(data, 0); }\n"
		<< "\tpublic " << proto.name() << "(byte[] data, int offset){ super(data, offset); }\n\n";

	std::vector<std::string> to_clean;
	to_clean.reserve(proto.field_size());

	auto handle_simple_field = [&oss, &to_clean](bool repeated, const std::string& field_name,
								const char* raw_type, const char* boxed_type, bool primary=true) {
		if (repeated) {
			oss << "\tprivate com.github.peterrk.protocache." << boxed_type << "Array _" << field_name << " = null;\n"
				<< "\tpublic com.github.peterrk.protocache." << boxed_type << "Array"
				<< " get" << JavaFieldName(field_name) << "() {\n"
				<< "\t\tif (_" << field_name << " == null) {\n"
				<< "\t\t\t_" << field_name << " = get" << boxed_type << "Array(FIELD_" << field_name << ");\n"
				<< "\t\t}\n"
				<< "\t\treturn _" << field_name << ";\n"
				<< "\t}\n";
			to_clean.push_back(field_name);
		} else if (primary) {
			oss << "\tpublic " << raw_type << " get" << JavaFieldName(field_name)
				<< "() { return get" << boxed_type << "(FIELD_" << field_name << "); }\n";
		} else {
			oss << "\tprivate " << raw_type << " _" << field_name << " = null;\n"
				<< "\tpublic " << raw_type << " get" << JavaFieldName(field_name) << "() {\n"
				<< "\t\tif (_" << field_name << " == null) {\n"
				<< "\t\t\t_" << field_name << " = get" << boxed_type << "(FIELD_" << field_name << ");\n"
				<< "\t\t}\n"
				<< "\t\treturn _" << field_name << ";\n"
				<< "\t}\n";
			to_clean.push_back(field_name);
		}
	};

	for (auto& one : proto.field()) {
		if (one.options().deprecated()) {
			continue;
		}
		auto repeated = one.label() == ::google::protobuf::FieldDescriptorProto::LABEL_REPEATED;
		switch (one.type()) {
			case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
			{
				std::string java_type;
				if (repeated) {
					AliasUnit unit;
					unit.value_type = one.type();
					auto it = map_entries.find(one.type_name());
					if (it != map_entries.end()) {
						unit.key_type = it->second->field(0).type();
						unit.value_type = it->second->field(1).type();
						unit.value_class = it->second->field(1).type_name();
					} else {
						unit.value_class = one.type_name();
					}
					java_type = CalcAliasName(unit);
				} else {
					auto it = g_java_names.find(one.type_name());
					if (it != g_java_names.end()) {
						java_type = it->second;
					}
				}
				if (java_type.empty()) {
					return {};
				}
				oss << "\tprivate " << java_type << " _" << one.name() << " = null;\n"
					<< "\tpublic " << java_type << " get" << JavaFieldName(one.name()) << "() {\n"
					<< "\t\tif (_" << one.name() << " == null) {\n"
					<< "\t\t\t_" << one.name() << " = getField(FIELD_" << one.name() << ", " << java_type << "::new);\n"
					<< "\t\t}\n"
					<< "\t\treturn _" << one.name() << ";\n"
					<< "\t}\n";
				to_clean.push_back(one.name());
			}
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
				handle_simple_field(repeated, one.name(), "byte[]", "Bytes", false);
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
				handle_simple_field(repeated, one.name(), "String", "Str", false);
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
				handle_simple_field(repeated, one.name(), "double", "Float64");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
				handle_simple_field(repeated, one.name(), "float", "Float32");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
				handle_simple_field(repeated, one.name(), "long", "Int64");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
				handle_simple_field(repeated, one.name(), "int", "Int32");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
				handle_simple_field(repeated, one.name(), "boolean", "Bool");
				break;
			default:
				std::cerr << "unsupported field " << one.name() << " in message " << proto.name() << std::endl;
				return {};
		}
	}

	if (!to_clean.empty()) {
		oss << "\n\t@Override\n"
			<< "\tpublic void init(byte[] data, int offset) {\n";
		for (auto& field_name : to_clean) {
			oss << "\t\t_" << field_name << " = null;\n";
		}
		oss << "\t\tsuper.init(data, offset);\n\t}";
	}
	oss << "}\n";
	return oss.str();
}

static std::string CalcFilename(const std::string& prefix, const std::string& name) {
	std::string filename;
	filename.reserve(prefix.size()+name.size()+5);
	filename += prefix;
	filename += name;
	filename += ".java";
	return filename;
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
		if (proto.options().java_package().empty()) {
			std::cerr << "java package is unspecified in file: " << proto.name() << std::endl;
			return -1;
		}
		auto java_ns = proto.options().java_package();
		for (auto& one : proto.enum_type()) {
			CollectAlias(ns, java_ns, one);
		}
		for (auto& one : proto.message_type()) {
			if (!CollectAlias(ns, java_ns, one)) {
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
		auto package = proto.options().java_package();
		std::string path;
		path.reserve(package.size() + 1);
		path = package;
#ifdef _WIN32
		constexpr char kDelim = '\\';
#else
		constexpr char kDelim = '/';
#endif
		for (auto& ch : path) {
			if (ch == '.') {
				ch = kDelim;
			}
		}
		path.push_back(kDelim);
		for (auto& one : proto.enum_type()) {
			if (one.options().deprecated()) {
				continue;
			}
			auto file = response.add_file();
			file->set_name(CalcFilename(path, one.name()));
			file->set_content(GenEnum(one, &package));
			if (file->content().empty()) {
				std::cerr << "fail to generate code for enum " << one.name() << " in file: " << proto.name() << std::endl;
				return -2;
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
			auto file = response.add_file();
			file->set_name(CalcFilename(path, one.name()));
			file->set_content(GenMessage(ns, one, &package));
			if (file->content().empty()) {
				std::cerr << "fail to generate code for message " << one.name() << " in file: " << proto.name() << std::endl;
				return -2;
			}
		}
	}

	if (!response.SerializeToFileDescriptor(STDOUT_FILENO)) {
		std::cerr << "fail to response" << std::endl;
		return 2;
	}
	return 0;
}