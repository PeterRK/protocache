// Copyright (c) 2026, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <unistd.h>

#include "proto-gen-utils.h"

namespace {

using FieldProto = ::google::protobuf::FieldDescriptorProto;
using MessageProto = ::google::protobuf::DescriptorProto;
using FileProto = ::google::protobuf::FileDescriptorProto;

struct MessageInfo {
	std::string file;
	std::string fullname;
	std::string py_name;
	std::string proto_name;
	std::string parent;
	bool alias = false;
};

struct PyValue {
	std::string kind;
	std::string cls = "None";
};

static std::unordered_map<std::string, AliasUnit> g_alias_book;
static std::unordered_map<std::string, MessageInfo> g_messages;
static std::unordered_map<std::string, std::string> g_file_modules;
static std::string g_current_file;
static std::unordered_map<std::string, std::string> g_import_aliases;
static bool g_failed = false;

static bool CanBeKey(FieldProto::Type type) {
	switch (type) {
		case FieldProto::TYPE_STRING:
		case FieldProto::TYPE_FIXED64:
		case FieldProto::TYPE_UINT64:
		case FieldProto::TYPE_FIXED32:
		case FieldProto::TYPE_UINT32:
		case FieldProto::TYPE_SFIXED64:
		case FieldProto::TYPE_SINT64:
		case FieldProto::TYPE_INT64:
		case FieldProto::TYPE_SFIXED32:
		case FieldProto::TYPE_SINT32:
		case FieldProto::TYPE_INT32:
			return true;
		default:
			return false;
	}
}

static bool CollectAlias(const std::string& ns, const MessageProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	std::unordered_map<std::string, const MessageProto*> map_entries;
	for (auto& one : proto.nested_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, one.name()), &one);
			continue;
		}
		if (!CollectAlias(fullname, one)) {
			return false;
		}
	}
	if (!IsAlias(proto)) {
		return true;
	}
	auto& field = proto.field(0);
	if (!IsRepeated(field)) {
		std::cerr << "illegal alias: " << fullname << std::endl;
		return false;
	}
	AliasUnit unit;
	unit.value_type = field.type();
	if (field.type() == FieldProto::TYPE_MESSAGE) {
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

static std::string ConvertPyFilename(const std::string& name) {
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name + "_pc.py";
	}
	return name.substr(0, pos) + "_pc.py";
}

static std::string ModuleName(const std::string& proto_file) {
	auto out = ConvertPyFilename(proto_file);
	if (out.size() >= 3 && out.substr(out.size() - 3) == ".py") {
		out.resize(out.size() - 3);
	}
	for (auto& ch : out) {
		if (ch == '/' || ch == '\\') {
			ch = '.';
		}
	}
	return out;
}

static std::string PyIdent(const std::string& raw) {
	static const std::unordered_set<std::string> keywords = {
			"False", "None", "True", "and", "as", "assert", "async", "await",
			"break", "class", "continue", "def", "del", "elif", "else", "except",
			"finally", "for", "from", "global", "if", "import", "in", "is",
			"lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
			"while", "with", "yield",
	};
	std::string out;
	out.reserve(raw.size() + 1);
	for (auto ch : raw) {
		if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
			out += ch;
		} else {
			out += '_';
		}
	}
	if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front()))) {
		out.insert(out.begin(), '_');
	}
	if (keywords.find(out) != keywords.end()) {
		out += '_';
	}
	return out;
}

static std::string PyFieldName(const std::string& raw) {
	static const std::unordered_set<std::string> reserved = {
			"Deserialize",
			"HasField",
			"Serialize",
	};
	auto out = PyIdent(raw);
	if (reserved.find(out) != reserved.end()) {
		out += "_";
	}
	return out;
}

static std::string PyClassName(const std::string& prefix, const std::string& name) {
	if (prefix.empty()) {
		return PyIdent(name);
	}
	return prefix + "_" + PyIdent(name);
}

static bool IsIgnoredDependency(const std::string& path) {
	static const std::string prefix = "google/protobuf/";
	return path.size() >= prefix.size() && path.compare(0, prefix.size(), prefix) == 0;
}

static void CollectMessages(const std::string& ns, const MessageProto& proto,
							const std::string& file, const std::string& prefix,
							const std::string& parent) {
	if (proto.options().map_entry() || proto.options().deprecated()) {
		return;
	}
	auto fullname = NaiveJoinName(ns, proto.name());
	auto py_name = PyClassName(prefix, proto.name());
	MessageInfo info;
	info.file = file;
	info.fullname = fullname;
	info.py_name = py_name;
	info.proto_name = proto.name();
	info.parent = parent;
	info.alias = g_alias_book.find(fullname) != g_alias_book.end();
	g_messages.emplace(fullname, info);
	for (auto& one : proto.nested_type()) {
		CollectMessages(fullname, one, file, py_name, fullname);
	}
}

static std::string KindName(FieldProto::Type type) {
	switch (type) {
		case FieldProto::TYPE_BYTES:
			return "_pc.BYTES";
		case FieldProto::TYPE_STRING:
			return "_pc.STRING";
		case FieldProto::TYPE_DOUBLE:
			return "_pc.F64";
		case FieldProto::TYPE_FLOAT:
			return "_pc.F32";
		case FieldProto::TYPE_FIXED64:
		case FieldProto::TYPE_UINT64:
			return "_pc.U64";
		case FieldProto::TYPE_FIXED32:
		case FieldProto::TYPE_UINT32:
			return "_pc.U32";
		case FieldProto::TYPE_SFIXED64:
		case FieldProto::TYPE_SINT64:
		case FieldProto::TYPE_INT64:
			return "_pc.I64";
		case FieldProto::TYPE_SFIXED32:
		case FieldProto::TYPE_SINT32:
		case FieldProto::TYPE_INT32:
			return "_pc.I32";
		case FieldProto::TYPE_BOOL:
			return "_pc.BOOL";
		case FieldProto::TYPE_ENUM:
			return "_pc.ENUM";
		default:
			return {};
	}
}

static std::string ClassRef(const std::string& fullname) {
	auto it = g_messages.find(fullname);
	if (it == g_messages.end()) {
		std::cerr << "unknown message type: " << fullname << std::endl;
		g_failed = true;
		return "None";
	}
	if (it->second.file == g_current_file) {
		return it->second.py_name;
	}
	auto alias = g_import_aliases.find(it->second.file);
	if (alias == g_import_aliases.end()) {
		std::cerr << "missing import for message type: " << fullname << std::endl;
		g_failed = true;
		return "None";
	}
	return alias->second + "." + it->second.py_name;
}

static PyValue ValueOf(FieldProto::Type type, const std::string& type_name) {
	PyValue out;
	if (type == FieldProto::TYPE_MESSAGE) {
		auto alias = g_alias_book.find(type_name);
		out.cls = ClassRef(type_name);
		if (alias != g_alias_book.end()) {
			out.kind = alias->second.key_type == TYPE_NONE ? "_pc.ARRAY" : "_pc.MAP";
		} else {
			out.kind = "_pc.MESSAGE";
		}
		return out;
	}
	out.kind = KindName(type);
	if (out.kind.empty()) {
		std::cerr << "unsupported field type: " << type << std::endl;
		g_failed = true;
		out.kind = "_pc.MESSAGE";
	}
	return out;
}

static std::string GenEnum(const ::google::protobuf::EnumDescriptorProto& proto) {
	std::ostringstream oss;
	oss << "class " << PyIdent(proto.name()) << ":\n";
	bool any = false;
	for (auto& value : proto.value()) {
		if (value.options().deprecated()) {
			continue;
		}
		oss << "    " << PyIdent(value.name()) << " = " << value.number() << "\n";
		any = true;
	}
	if (!any) {
		oss << "    pass\n";
	}
	oss << "\n\n";
	return oss.str();
}

static std::unordered_map<std::string, const MessageProto*> MapEntries(const MessageProto& proto,
																	   const std::string& fullname) {
	std::unordered_map<std::string, const MessageProto*> out;
	for (auto& one : proto.nested_type()) {
		if (!one.options().deprecated() && one.options().map_entry()) {
			out.emplace(NaiveJoinName(fullname, one.name()), &one);
		}
	}
	return out;
}

static std::string FieldAlias(const FieldProto& field) {
	return "_field_" + PyFieldName(field.name());
}

static std::string FieldIdRef(const std::string& owner, const FieldProto& field) {
	return owner + "." + FieldAlias(field);
}

static std::string ValueTypeSpec(const PyValue& value) {
	return value.cls;
}

static std::string SchemaEntry(const std::string& name, const std::string& id,
							   bool repeated, const std::string& key_kind, const PyValue& value) {
	std::ostringstream oss;
	oss << "    (\"" << name << "\", " << id << ", "
		<< (repeated ? "True" : "False") << ", " << key_kind << ", "
		<< value.kind << ", " << ValueTypeSpec(value) << "),\n";
	return oss.str();
}

static std::string GenSchemaEntry(const std::string& owner, const FieldProto& field,
								  const std::unordered_map<std::string, const MessageProto*>& map_entries) {
	std::ostringstream oss;
	auto name = PyFieldName(field.name());
	auto id = FieldIdRef(owner, field);
	if (IsRepeated(field)) {
		if (field.type() == FieldProto::TYPE_MESSAGE) {
			auto map = map_entries.find(field.type_name());
			if (map != map_entries.end()) {
				auto& key_field = map->second->field(0);
				auto& value_field = map->second->field(1);
				if (!CanBeKey(key_field.type())) {
					std::cerr << "illegal map key type: " << key_field.type() << std::endl;
					g_failed = true;
				}
				auto value = ValueOf(value_field.type(), value_field.type_name());
				return SchemaEntry(name, id, true, KindName(key_field.type()), value);
			}
		}
		auto value = ValueOf(field.type(), field.type_name());
		return SchemaEntry(name, id, true, "_pc.NONE", value);
	}

	auto value = ValueOf(field.type(), field.type_name());
	return SchemaEntry(name, id, false, "_pc.NONE", value);
}

static std::string GenAliasClass(const MessageInfo& info) {
	std::ostringstream oss;
	auto it = g_alias_book.find(info.fullname);
	if (it == g_alias_book.end()) {
		g_failed = true;
		return {};
	}
	auto& alias = it->second;
	if (alias.key_type == TYPE_NONE) {
		auto value = ValueOf(alias.value_type, alias.value_class);
		oss << "class " << info.py_name << "(_pc.Array):\n"
			<< "    schema = (_pc.NONE, " << value.kind << ", " << ValueTypeSpec(value) << ")\n\n\n";
	} else {
		if (!CanBeKey(alias.key_type)) {
			std::cerr << "illegal alias map key type: " << alias.key_type << std::endl;
			g_failed = true;
		}
		auto value = ValueOf(alias.value_type, alias.value_class);
		oss << "class " << info.py_name << "(_pc.Map):\n"
			<< "    schema = (" << KindName(alias.key_type) << ", "
			<< value.kind << ", " << ValueTypeSpec(value) << ")\n\n\n";
	}
	return oss.str();
}

static std::string GenMessage(const std::string& ns, const MessageProto& proto) {
	if (proto.options().map_entry() || proto.options().deprecated()) {
		return {};
	}
	auto fullname = NaiveJoinName(ns, proto.name());
	std::ostringstream oss;
	for (auto& one : proto.nested_type()) {
		oss << GenMessage(fullname, one);
	}

	auto info = g_messages.find(fullname);
	if (info == g_messages.end()) {
		g_failed = true;
		return {};
	}
	for (auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			oss << GenEnum(one);
		}
	}
	if (info->second.alias) {
		oss << GenAliasClass(info->second);
		return oss.str();
	}

	auto fields = FieldsInOrder(proto);
	oss << "class " << info->second.py_name << "(_pc.Message):\n";
	for (auto field : fields) {
		if (field->name() == "_") {
			std::cerr << "found illegal field in message " << fullname << std::endl;
			g_failed = true;
			continue;
		}
		oss << "    " << FieldAlias(*field) << " = " << (field->number() - 1) << "\n";
	}
	if (!fields.empty()) {
		oss << "\n";
	}
	if (fields.empty()) {
		oss << "    pass\n\n";
	} else {
		oss << "\n";
	}
	return oss.str();
}

static std::string GenSchema(const std::string& ns, const MessageProto& proto) {
	if (proto.options().map_entry() || proto.options().deprecated()) {
		return {};
	}
	auto fullname = NaiveJoinName(ns, proto.name());
	std::ostringstream oss;
	for (auto& one : proto.nested_type()) {
		oss << GenSchema(fullname, one);
	}
	auto info = g_messages.find(fullname);
	if (info == g_messages.end()) {
		g_failed = true;
		return {};
	}
	if (info->second.alias) {
		return oss.str();
	}
	auto fields = FieldsInOrder(proto);
	auto maps = MapEntries(proto, fullname);
	if (fields.empty()) {
		oss << info->second.py_name << "._schema = ()\n\n";
		return oss.str();
	}
	oss << info->second.py_name << "._schema = (\n";
	for (auto field : fields) {
		if (field->name() == "_") {
			continue;
		}
		oss << GenSchemaEntry(info->second.py_name, *field, maps);
	}
	oss << ")\n\n";
	return oss.str();
}

static void AddDependencyImports(const FileProto& proto, std::ostringstream& oss) {
	unsigned idx = 0;
	for (auto& dep : proto.dependency()) {
		if (IsIgnoredDependency(dep)) {
			continue;
		}
		bool has_messages = false;
		for (auto& it : g_messages) {
			if (it.second.file == dep) {
				has_messages = true;
				break;
			}
		}
		if (!has_messages) {
			continue;
		}
		auto alias = "_pcdep_" + std::to_string(idx++);
		g_import_aliases.emplace(dep, alias);
		oss << "import " << g_file_modules[dep] << " as " << alias << "\n";
	}
}

static std::string GenFile(const FileProto& proto) {
	g_current_file = proto.name();
	g_import_aliases.clear();
	g_failed = false;

	std::ostringstream oss;
	oss << "# Generated by protoc-gen-pcpy. DO NOT EDIT.\n"
		<< "import protocache as _pc\n";
	AddDependencyImports(proto, oss);
	oss << "\n\n";

	for (auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			oss << GenEnum(one);
		}
	}

	std::string ns;
	if (!proto.package().empty()) {
		ns = "." + proto.package();
	}
	for (auto& one : proto.message_type()) {
		oss << GenMessage(ns, one);
	}
	for (auto& one : proto.message_type()) {
		oss << GenSchema(ns, one);
	}
	if (g_failed) {
		return {};
	}
	auto out = oss.str();
	while (out.size() > 1 && out[out.size() - 1] == '\n' && out[out.size() - 2] == '\n') {
		out.pop_back();
	}
	return out;
}

} // namespace

int main() {
	::google::protobuf::compiler::CodeGeneratorRequest request;
	::google::protobuf::compiler::CodeGeneratorResponse response;

	if (!request.ParseFromFileDescriptor(STDIN_FILENO)) {
		std::cerr << "fail to get request" << std::endl;
		return 1;
	}

	for (auto& proto : request.proto_file()) {
		g_file_modules.emplace(proto.name(), ModuleName(proto.name()));
		std::string ns;
		if (!proto.package().empty()) {
			ns = "." + proto.package();
		}
		for (auto& one : proto.message_type()) {
			if (!CollectAlias(ns, one)) {
				std::cerr << "fail to collect alias from file: " << proto.name() << std::endl;
				return -1;
			}
		}
	}

	for (auto& proto : request.proto_file()) {
		std::string ns;
		if (!proto.package().empty()) {
			ns = "." + proto.package();
		}
		for (auto& one : proto.message_type()) {
			CollectMessages(ns, one, proto.name(), {}, {});
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
		auto content = GenFile(proto);
		if (content.empty()) {
			std::cerr << "fail to generate code for file: " << proto.name() << std::endl;
			return -2;
		}
		auto one = response.add_file();
		one->set_name(ConvertPyFilename(proto.name()));
		one->set_content(std::move(content));
	}

	if (!response.SerializeToFileDescriptor(STDOUT_FILENO)) {
		std::cerr << "fail to response" << std::endl;
		return 2;
	}
	return 0;
}
