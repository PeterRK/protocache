// Copyright (c) 2026, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "proto-gen-utils.h"

namespace {

using EnumProto = ::google::protobuf::EnumDescriptorProto;
using FieldProto = ::google::protobuf::FieldDescriptorProto;
using FileProto = ::google::protobuf::FileDescriptorProto;
using MessageProto = ::google::protobuf::DescriptorProto;

struct AliasInfo final {
	FieldProto::Type key_type = TYPE_NONE;
	FieldProto::Type value_type = TYPE_NONE;
	std::string value_type_name;
};

struct MessageInfo final {
	std::string file;
	std::string fullname;
	std::string ts_name;
	bool alias = false;
};

struct EnumInfo final {
	std::string file;
	std::string fullname;
	std::string ts_name;
	std::string default_name;
	int default_number = 0;
	bool default_exported = false;
};

struct TsValue final {
	std::string type;
	std::string kind;
	std::string default_value;
	std::string resolver;
};

static std::unordered_map<std::string, AliasInfo> g_aliases;
static std::unordered_map<std::string, MessageInfo> g_messages;
static std::unordered_map<std::string, EnumInfo> g_enums;
static std::unordered_map<std::string, std::vector<std::string>> g_exports;
static std::unordered_map<std::string, std::string> g_export_owners;
static std::unordered_map<std::string, std::string> g_import_aliases;
static std::vector<std::string> g_import_files;
static std::string g_current_file;
static std::string g_runtime_import = "protocache";
static std::string g_error;

static void Fail(const std::string& message) {
	if (g_error.empty()) {
		g_error = message;
	}
}

static std::string Quote(const std::string& raw) {
	std::string out = "\"";
	for (unsigned char ch : raw) {
		switch (ch) {
			case '\\': out += "\\\\"; break;
			case '"': out += "\\\""; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (ch < 0x20) {
					static constexpr char kHex[] = "0123456789abcdef";
					out += "\\u00";
					out += kHex[ch >> 4U];
					out += kHex[ch & 0x0fU];
				} else {
					out += static_cast<char>(ch);
				}
		}
	}
	out += '"';
	return out;
}

static std::string TsIdent(const std::string& raw) {
	static const std::unordered_set<std::string> keywords = {
			"as", "async", "await", "break", "case", "catch", "class",
			"const", "constructor", "continue", "debugger", "default",
			"delete", "do", "else", "enum", "export", "extends", "false",
			"finally", "for", "from", "function", "get", "if", "implements",
			"import", "in", "instanceof", "interface", "let", "new", "null",
			"of", "package", "private", "protected", "public", "return", "set",
			"static", "super", "switch", "symbol", "this", "throw", "true",
			"try", "type", "typeof", "undefined", "var", "void", "while",
			"with", "yield",
	};
	std::string out;
	out.reserve(raw.size() + 1);
	for (unsigned char ch : raw) {
		out += std::isalnum(ch) || ch == '_' || ch == '$'
			? static_cast<char>(ch)
			: '_';
	}
	if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front()))) {
		out.insert(out.begin(), '_');
	}
	if (keywords.find(out) != keywords.end()) {
		out += '_';
	}
	return out;
}

static std::string FieldName(const std::string& raw) {
	static const std::unordered_set<std::string> reserved = {
			"hasField", "serialize",
	};
	auto out = TsIdent(raw);
	if (reserved.find(out) != reserved.end()) {
		out += '_';
	}
	return out;
}

static std::string SymbolName(const std::string& prefix, const std::string& raw) {
	auto name = TsIdent(raw);
	return prefix.empty() ? name : prefix + "_" + name;
}

static std::string StripExtension(const std::string& name) {
	auto slash = name.find_last_of("/\\");
	auto dot = name.find_last_of('.');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
		return name;
	}
	return name.substr(0, dot);
}

static std::string InternalFilename(const std::string& proto_file) {
	return StripExtension(proto_file) + ".pc.internal.ts";
}

static std::string PublicFilename(const std::string& proto_file) {
	return StripExtension(proto_file) + ".pc.ts";
}

static std::string InternalJsFilename(const std::string& proto_file) {
	return StripExtension(proto_file) + ".pc.internal.js";
}

static std::vector<std::string> PathParts(const std::string& path) {
	std::vector<std::string> parts;
	Split(path, '/', &parts);
	return parts;
}

static std::string RelativeInternalImport(
		const std::string& from_proto,
		const std::string& to_proto) {
	auto from = PathParts(from_proto);
	auto to = PathParts(InternalJsFilename(to_proto));
	if (!from.empty()) from.pop_back();
	size_t common = 0;
	while (common < from.size() && common < to.size() &&
		   from[common] == to[common]) {
		++common;
	}
	std::ostringstream oss;
	for (size_t i = common; i < from.size(); ++i) {
		oss << "../";
	}
	for (size_t i = common; i < to.size(); ++i) {
		if (i != common) oss << '/';
		oss << to[i];
	}
	auto out = oss.str();
	if (out.empty() || (out[0] != '.')) out = "./" + out;
	return out;
}

static std::string Basename(const std::string& path) {
	auto slash = path.find_last_of("/\\");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string LoaderName(const std::string& proto_file) {
	return "load" + ToPascal(TsIdent(Basename(StripExtension(proto_file))));
}

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

static std::string KindName(FieldProto::Type type) {
	switch (type) {
		case FieldProto::TYPE_BYTES: return "$pc.Kind.Bytes";
		case FieldProto::TYPE_STRING: return "$pc.Kind.String";
		case FieldProto::TYPE_DOUBLE: return "$pc.Kind.F64";
		case FieldProto::TYPE_FLOAT: return "$pc.Kind.F32";
		case FieldProto::TYPE_FIXED64:
		case FieldProto::TYPE_UINT64: return "$pc.Kind.U64";
		case FieldProto::TYPE_FIXED32:
		case FieldProto::TYPE_UINT32: return "$pc.Kind.U32";
		case FieldProto::TYPE_SFIXED64:
		case FieldProto::TYPE_SINT64:
		case FieldProto::TYPE_INT64: return "$pc.Kind.I64";
		case FieldProto::TYPE_SFIXED32:
		case FieldProto::TYPE_SINT32:
		case FieldProto::TYPE_INT32: return "$pc.Kind.I32";
		case FieldProto::TYPE_BOOL: return "$pc.Kind.Bool";
		case FieldProto::TYPE_ENUM: return "$pc.Kind.Enum";
		default: return {};
	}
}

static std::string ScalarType(FieldProto::Type type) {
	switch (type) {
		case FieldProto::TYPE_DOUBLE:
		case FieldProto::TYPE_FLOAT:
		case FieldProto::TYPE_FIXED32:
		case FieldProto::TYPE_UINT32:
		case FieldProto::TYPE_SFIXED32:
		case FieldProto::TYPE_SINT32:
		case FieldProto::TYPE_INT32: return "number";
		case FieldProto::TYPE_FIXED64:
		case FieldProto::TYPE_UINT64:
		case FieldProto::TYPE_SFIXED64:
		case FieldProto::TYPE_SINT64:
		case FieldProto::TYPE_INT64: return "bigint";
		case FieldProto::TYPE_BOOL: return "boolean";
		case FieldProto::TYPE_STRING: return "string";
		case FieldProto::TYPE_BYTES: return "Uint8Array";
		default: return {};
	}
}

static std::string ScalarDefault(FieldProto::Type type) {
	switch (type) {
		case FieldProto::TYPE_FIXED64:
		case FieldProto::TYPE_UINT64:
		case FieldProto::TYPE_SFIXED64:
		case FieldProto::TYPE_SINT64:
		case FieldProto::TYPE_INT64: return "0n";
		case FieldProto::TYPE_BOOL: return "false";
		case FieldProto::TYPE_STRING: return "\"\"";
		case FieldProto::TYPE_BYTES: return "new Uint8Array()";
		default: return "0";
	}
}

static bool ReserveSymbol(
		const std::string& file,
		const std::string& ts_name,
		const std::string& fullname) {
	auto key = file + '\n' + ts_name;
	auto [it, inserted] = g_export_owners.emplace(key, fullname);
	if (!inserted && it->second != fullname) {
		Fail("TypeScript symbol collision in " + file + ": " + ts_name);
		return false;
	}
	return true;
}

static bool RegisterExport(
		const std::string& file,
		const std::string& ts_name,
		const std::string& fullname) {
	if (!ReserveSymbol(file, ts_name, fullname)) return false;
	g_exports[file].push_back(ts_name);
	return true;
}

static bool CollectAlias(const std::string& ns, const MessageProto& proto) {
	if (proto.options().deprecated() || proto.options().map_entry()) return true;
	auto fullname = NaiveJoinName(ns, proto.name());
	std::unordered_map<std::string, const MessageProto*> maps;
	for (const auto& nested : proto.nested_type()) {
		if (nested.options().deprecated()) continue;
		if (nested.options().map_entry()) {
			maps.emplace(NaiveJoinName(fullname, nested.name()), &nested);
		} else if (!CollectAlias(fullname, nested)) {
			return false;
		}
	}
	if (!IsAlias(proto)) return true;
	const auto& field = proto.field(0);
	if (!IsRepeated(field)) {
		Fail("ProtoCache alias must contain a repeated '_' field: " + fullname);
		return false;
	}
	AliasInfo alias;
	alias.value_type = field.type();
	alias.value_type_name = field.type_name();
	if (field.type() == FieldProto::TYPE_MESSAGE) {
		auto map = maps.find(field.type_name());
		if (map != maps.end()) {
			const auto& key = map->second->field(0);
			const auto& value = map->second->field(1);
			if (!CanBeKey(key.type())) {
				Fail("unsupported ProtoCache map key type in alias: " + fullname);
				return false;
			}
			alias.key_type = key.type();
			alias.value_type = value.type();
			alias.value_type_name = value.type_name();
		}
	}
	g_aliases.emplace(fullname, std::move(alias));
	return true;
}

static void CollectEnum(
		const std::string& ns,
		const EnumProto& proto,
		const std::string& file,
		const std::string& prefix) {
	if (proto.options().deprecated()) return;
	if (proto.value_size() == 0) {
		Fail("enum has no values: " + NaiveJoinName(ns, proto.name()));
		return;
	}
	EnumInfo info;
	info.file = file;
	info.fullname = NaiveJoinName(ns, proto.name());
	info.ts_name = SymbolName(prefix, proto.name());
	info.default_name = TsIdent(proto.value(0).name());
	info.default_number = proto.value(0).number();
	info.default_exported = !proto.value(0).options().deprecated();
	RegisterExport(file, info.ts_name, info.fullname);
	g_enums.emplace(info.fullname, std::move(info));
}

static void CollectMessage(
		const std::string& ns,
		const MessageProto& proto,
		const std::string& file,
		const std::string& prefix) {
	if (proto.options().deprecated() || proto.options().map_entry()) return;
	auto fullname = NaiveJoinName(ns, proto.name());
	MessageInfo info;
	info.file = file;
	info.fullname = fullname;
	info.ts_name = SymbolName(prefix, proto.name());
	info.alias = g_aliases.find(fullname) != g_aliases.end();
	RegisterExport(file, info.ts_name, info.fullname);
	if (info.alias) {
		ReserveSymbol(file, info.ts_name + "Schema", info.fullname + "#schema");
	}
	g_messages.emplace(fullname, info);
	for (const auto& one : proto.enum_type()) {
		CollectEnum(fullname, one, file, info.ts_name);
	}
	for (const auto& one : proto.nested_type()) {
		CollectMessage(fullname, one, file, info.ts_name);
	}
}

static std::string ImportAlias(const std::string& file) {
	auto found = g_import_aliases.find(file);
	if (found != g_import_aliases.end()) return found->second;
	auto alias = "$pcdep" + std::to_string(g_import_files.size());
	g_import_files.push_back(file);
	g_import_aliases.emplace(file, alias);
	return alias;
}

static std::string MessageRef(const std::string& fullname, bool schema) {
	auto found = g_messages.find(fullname);
	if (found == g_messages.end()) {
		Fail("unknown message type: " + fullname);
		return "never";
	}
	auto name = found->second.ts_name + (schema ? "Schema" : "");
	return found->second.file == g_current_file
		? name
		: ImportAlias(found->second.file) + "." + name;
}

static std::string EnumRef(const std::string& fullname) {
	auto found = g_enums.find(fullname);
	if (found == g_enums.end()) {
		Fail("unknown enum type: " + fullname);
		return "never";
	}
	return found->second.file == g_current_file
		? found->second.ts_name
		: ImportAlias(found->second.file) + "." + found->second.ts_name;
}

static TsValue ValueOf(FieldProto::Type type, const std::string& type_name) {
	TsValue out;
	if (type == FieldProto::TYPE_MESSAGE) {
		auto message = g_messages.find(type_name);
		if (message == g_messages.end()) {
			Fail("unknown message type: " + type_name);
			return {"never", "$pc.Kind.Message", "undefined", {}};
		}
		out.type = MessageRef(type_name, false);
		auto alias = g_aliases.find(type_name);
		if (alias == g_aliases.end()) {
			out.kind = "$pc.Kind.Message";
			out.default_value = "undefined";
			out.resolver = MessageRef(type_name, false);
		} else if (alias->second.key_type == TYPE_NONE) {
			out.kind = "$pc.Kind.Array";
			out.default_value = "[]";
			out.resolver = MessageRef(type_name, true);
		} else {
			out.kind = "$pc.Kind.Map";
			out.default_value = "new Map()";
			out.resolver = MessageRef(type_name, true);
		}
		return out;
	}
	if (type == FieldProto::TYPE_ENUM) {
		auto found = g_enums.find(type_name);
		if (found == g_enums.end()) {
			Fail("unknown enum type: " + type_name);
			return {"never", "$pc.Kind.Enum", "0", {}};
		}
		out.type = EnumRef(type_name);
		out.kind = "$pc.Kind.Enum";
		out.default_value = found->second.default_exported
			? EnumRef(type_name) + "." + found->second.default_name
			: "$pc.unknownEnum(" + std::to_string(found->second.default_number) + ")";
		return out;
	}
	out.type = ScalarType(type);
	out.kind = KindName(type);
	out.default_value = ScalarDefault(type);
	if (out.type.empty() || out.kind.empty()) {
		Fail("unsupported protobuf field type: " + std::to_string(type));
	}
	return out;
}

static std::unordered_map<std::string, const MessageProto*> MapEntries(
		const std::string& fullname,
		const MessageProto& proto) {
	std::unordered_map<std::string, const MessageProto*> out;
	for (const auto& one : proto.nested_type()) {
		if (!one.options().deprecated() && one.options().map_entry()) {
			out.emplace(NaiveJoinName(fullname, one.name()), &one);
		}
	}
	return out;
}

static bool CheckField(const std::string& owner, const FieldProto& field) {
	if (field.name() == "_") {
		Fail("non-alias message contains reserved '_' field: " + owner);
		return false;
	}
	if (field.number() <= 0 || field.number() > 6387) {
		Fail("field number is outside the ProtoCache range: " + owner + "." + field.name());
		return false;
	}
	if (field.has_oneof_index() && !field.proto3_optional()) {
		Fail("oneof is not supported: " + owner + "." + field.name());
		return false;
	}
	return true;
}

static void GenEnumsInMessage(
		const std::string& ns,
		const MessageProto& proto,
		std::ostringstream& out);

static void GenEnum(
		const std::string& fullname,
		const EnumProto& proto,
		std::ostringstream& out) {
	auto found = g_enums.find(fullname);
	if (found == g_enums.end()) return;
	out << "export const " << found->second.ts_name << " = {\n";
	for (const auto& value : proto.value()) {
		if (!value.options().deprecated()) {
			out << "  " << TsIdent(value.name()) << ": " << value.number() << ",\n";
		}
	}
	out << "} as const;\n\n"
		<< "export type " << found->second.ts_name
		<< " = $pc.EnumValue<(typeof " << found->second.ts_name
		<< ")[keyof typeof " << found->second.ts_name << "]>;\n\n";
}

static void GenEnumsInMessage(
		const std::string& ns,
		const MessageProto& proto,
		std::ostringstream& out) {
	if (proto.options().deprecated() || proto.options().map_entry()) return;
	auto fullname = NaiveJoinName(ns, proto.name());
	for (const auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			GenEnum(NaiveJoinName(fullname, one.name()), one, out);
		}
	}
	for (const auto& one : proto.nested_type()) {
		GenEnumsInMessage(fullname, one, out);
	}
}

static void GenAliases(
		const std::string& ns,
		const MessageProto& proto,
		std::ostringstream& out) {
	if (proto.options().deprecated() || proto.options().map_entry()) return;
	auto fullname = NaiveJoinName(ns, proto.name());
	for (const auto& one : proto.nested_type()) {
		GenAliases(fullname, one, out);
	}
	auto message = g_messages.find(fullname);
	auto alias = g_aliases.find(fullname);
	if (message == g_messages.end() || alias == g_aliases.end()) return;
	auto value = ValueOf(alias->second.value_type, alias->second.value_type_name);
	if (alias->second.key_type == TYPE_NONE) {
		out << "export type " << message->second.ts_name << " = " << value.type << "[];\n"
			<< "export const " << message->second.ts_name << "Schema = $pc.arraySchemaV1<"
			<< value.type << ">(\n  " << value.kind;
		if (!value.resolver.empty()) out << ",\n  () => " << value.resolver;
		out << ",\n);\n\n";
	} else {
		auto key = ValueOf(alias->second.key_type, {});
		out << "export type " << message->second.ts_name << " = Map<"
			<< key.type << ", " << value.type << ">;\n"
			<< "export const " << message->second.ts_name << "Schema = $pc.mapSchemaV1<"
			<< key.type << ", " << value.type << ">(\n  " << key.kind << ",\n  "
			<< value.kind;
		if (!value.resolver.empty()) out << ",\n  () => " << value.resolver;
		out << ",\n);\n\n";
	}
}

static void GenMessageClasses(
		const std::string& ns,
		const MessageProto& proto,
		std::ostringstream& out) {
	if (proto.options().deprecated() || proto.options().map_entry()) return;
	auto fullname = NaiveJoinName(ns, proto.name());
	for (const auto& one : proto.nested_type()) {
		GenMessageClasses(fullname, one, out);
	}
	auto info = g_messages.find(fullname);
	if (info == g_messages.end() || info->second.alias) return;
	auto maps = MapEntries(fullname, proto);
	auto fields = FieldsInOrder(proto);
	out << "export class " << info->second.ts_name << " extends $pc.Message {\n";
	for (const auto* field : fields) {
		if (!CheckField(fullname, *field)) continue;
		auto name = FieldName(field->name());
		if (IsRepeated(*field) && field->type() == FieldProto::TYPE_MESSAGE) {
			auto map = maps.find(field->type_name());
			if (map != maps.end()) {
				const auto& key_field = map->second->field(0);
				const auto& value_field = map->second->field(1);
				if (!CanBeKey(key_field.type())) {
					Fail("unsupported map key type: " + fullname + "." + field->name());
					continue;
				}
				auto key = ValueOf(key_field.type(), key_field.type_name());
				auto value = ValueOf(value_field.type(), value_field.type_name());
				out << "  " << name << ": Map<" << key.type << ", " << value.type
					<< "> = new Map();\n";
				continue;
			}
		}
		auto value = ValueOf(field->type(), field->type_name());
		if (IsRepeated(*field)) {
			out << "  " << name << ": " << value.type << "[] = [];\n";
		} else if (field->type() == FieldProto::TYPE_MESSAGE &&
				   g_aliases.find(field->type_name()) == g_aliases.end()) {
			out << "  " << name << "?: " << value.type << ";\n";
		} else if (field->type() == FieldProto::TYPE_ENUM) {
			out << "  " << name << ": " << value.type << " = "
				<< value.default_value << ";\n";
		} else if (field->type() == FieldProto::TYPE_MESSAGE) {
			out << "  " << name << ": " << value.type << " = "
				<< value.default_value << ";\n";
		} else {
			out << "  " << name << " = " << value.default_value << ";\n";
		}
	}
	out << "\n  static schema: $pc.MessageSchema<" << info->second.ts_name << ">;\n\n"
		<< "  constructor(init?: $pc.MessageInit<" << info->second.ts_name << ">) {\n"
		<< "    super();\n"
		<< "    if (init !== undefined) $pc.assignFields(this, init, "
		<< info->second.ts_name << ".schema);\n"
		<< "  }\n"
		<< "}\n\n";
}

static std::string SchemaEntry(
		const std::string& name,
		const FieldProto& field,
		bool repeated,
		const std::string& key_kind,
		const TsValue& value) {
	std::ostringstream out;
	out << "  [" << Quote(name) << ", " << (field.number() - 1) << ", "
		<< (repeated ? "true" : "false") << ", " << key_kind << ", "
		<< value.kind;
	if (!value.resolver.empty()) out << ", () => " << value.resolver;
	out << "],\n";
	return out.str();
}

static void GenMessageSchemas(
		const std::string& ns,
		const MessageProto& proto,
		std::ostringstream& out) {
	if (proto.options().deprecated() || proto.options().map_entry()) return;
	auto fullname = NaiveJoinName(ns, proto.name());
	for (const auto& one : proto.nested_type()) {
		GenMessageSchemas(fullname, one, out);
	}
	auto info = g_messages.find(fullname);
	if (info == g_messages.end() || info->second.alias) return;
	auto maps = MapEntries(fullname, proto);
	auto fields = FieldsInOrder(proto);
	out << info->second.ts_name << ".schema = $pc.messageSchemaV1<"
		<< info->second.ts_name << ">([\n";
	for (const auto* field : fields) {
		if (!CheckField(fullname, *field)) continue;
		auto name = FieldName(field->name());
		if (IsRepeated(*field) && field->type() == FieldProto::TYPE_MESSAGE) {
			auto map = maps.find(field->type_name());
			if (map != maps.end()) {
				const auto& key_field = map->second->field(0);
				const auto& value_field = map->second->field(1);
				auto value = ValueOf(value_field.type(), value_field.type_name());
				out << SchemaEntry(
					name, *field, true, KindName(key_field.type()), value);
				continue;
			}
		}
		auto value = ValueOf(field->type(), field->type_name());
		out << SchemaEntry(
			name, *field, IsRepeated(*field), "$pc.Kind.None", value);
	}
	out << "] satisfies $pc.MessageFieldsV1<" << info->second.ts_name << ">);\n\n";
}

static std::string GenInternalFile(const FileProto& proto) {
	g_current_file = proto.name();
	g_import_aliases.clear();
	g_import_files.clear();
	std::ostringstream body;
	std::string ns = proto.package().empty() ? "" : "." + proto.package();
	for (const auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			GenEnum(NaiveJoinName(ns, one.name()), one, body);
		}
	}
	for (const auto& one : proto.message_type()) GenEnumsInMessage(ns, one, body);
	for (const auto& one : proto.message_type()) GenAliases(ns, one, body);
	for (const auto& one : proto.message_type()) GenMessageClasses(ns, one, body);
	for (const auto& one : proto.message_type()) GenMessageSchemas(ns, one, body);
	if (!g_error.empty()) return {};

	std::ostringstream out;
	out << "// Generated by protoc-gen-pcts. DO NOT EDIT.\n";
	if (!body.str().empty()) {
		out << "import * as $pc from " << Quote(g_runtime_import) << ";\n";
		for (const auto& file : g_import_files) {
			out << "import * as " << g_import_aliases[file] << " from "
				<< Quote(RelativeInternalImport(proto.name(), file)) << ";\n";
		}
		out << "\n" << body.str();
	} else {
		out << "export {};\n";
	}
	auto content = out.str();
	while (content.size() > 1 && content.back() == '\n' &&
		   content[content.size() - 2] == '\n') {
		content.pop_back();
	}
	return content;
}

static std::string GenPublicFile(const FileProto& proto) {
	auto stem = Basename(StripExtension(proto.name()));
	auto internal = "./" + stem + ".pc.internal.js";
	auto loader = LoaderName(proto.name());
	auto api = "$" + ToPascal(TsIdent(stem)) + "Api";
	std::ostringstream out;
	out << "// Generated by protoc-gen-pcts. DO NOT EDIT.\n"
		<< "import {\n  ensureInitialized as $ensureInitialized,\n"
		<< "  type InitOptions as $InitOptions,\n} from "
		<< Quote(g_runtime_import) << ";\n\n"
		<< "type " << api << " = typeof import(" << Quote(internal) << ");\n\n"
		<< "let $loaded: Promise<" << api << "> | undefined;\n\n"
		<< "export function " << loader << "($options: $InitOptions): Promise<"
		<< api << "> {\n"
		<< "  if ($loaded !== undefined) return $loaded;\n"
		<< "  const $pending = $ensureInitialized($options).then(() => import("
		<< Quote(internal) << "));\n"
		<< "  const $guarded = $pending.catch(($error: unknown) => {\n"
		<< "    if ($loaded === $guarded) $loaded = undefined;\n"
		<< "    throw $error;\n"
		<< "  });\n"
		<< "  $loaded = $guarded;\n"
		<< "  return $guarded;\n"
		<< "}\n";
	auto exports = g_exports.find(proto.name());
	if (exports != g_exports.end() && !exports->second.empty()) {
		out << "\nexport type {\n";
		for (const auto& symbol : exports->second) {
			out << "  " << symbol << ",\n";
		}
		out << "} from " << Quote(internal) << ";\n";
	}
	return out.str();
}

static bool ParseOptions(const std::string& raw) {
	if (raw.empty()) return true;
	std::vector<std::string> options;
	Split(raw, ',', &options);
	for (const auto& option : options) {
		auto equal = option.find('=');
		auto name = equal == std::string::npos ? option : option.substr(0, equal);
		auto value = equal == std::string::npos ? "" : option.substr(equal + 1);
		if (name == "runtime_import" && !value.empty()) {
			g_runtime_import = value;
		} else {
			Fail("unknown or invalid protoc-gen-pcts option: " + option);
			return false;
		}
	}
	return true;
}

}  // namespace

int main() {
	if (!PrepareProtocPluginIO()) {
		std::cerr << "failed to configure protoc plugin IO" << std::endl;
		return 1;
	}
	::google::protobuf::compiler::CodeGeneratorRequest request;
	::google::protobuf::compiler::CodeGeneratorResponse response;
	if (!request.ParseFromFileDescriptor(STDIN_FILENO)) {
		std::cerr << "failed to parse CodeGeneratorRequest" << std::endl;
		return 1;
	}
	if (!ParseOptions(request.parameter())) {
		response.set_error(g_error);
		response.SerializeToFileDescriptor(STDOUT_FILENO);
		return 0;
	}

	for (const auto& proto : request.proto_file()) {
		if (proto.syntax() != "proto3") {
			Fail("protoc-gen-pcts supports proto3 only: " + proto.name());
			break;
		}
		std::string ns = proto.package().empty() ? "" : "." + proto.package();
		for (const auto& one : proto.message_type()) {
			if (!CollectAlias(ns, one)) break;
		}
	}
	for (const auto& proto : request.proto_file()) {
		std::string ns = proto.package().empty() ? "" : "." + proto.package();
		for (const auto& one : proto.enum_type()) {
			CollectEnum(ns, one, proto.name(), {});
		}
		for (const auto& one : proto.message_type()) {
			CollectMessage(ns, one, proto.name(), {});
		}
	}

	std::unordered_set<std::string> requested;
	for (const auto& file : request.file_to_generate()) requested.insert(file);
	if (g_error.empty()) {
		for (const auto& proto : request.proto_file()) {
			if (requested.find(proto.name()) == requested.end()) continue;
			auto internal = GenInternalFile(proto);
			auto public_api = GenPublicFile(proto);
			if (!g_error.empty()) break;
			auto* internal_file = response.add_file();
			internal_file->set_name(InternalFilename(proto.name()));
			internal_file->set_content(std::move(internal));
			auto* public_file = response.add_file();
			public_file->set_name(PublicFilename(proto.name()));
			public_file->set_content(std::move(public_api));
		}
	}
	if (!g_error.empty()) response.set_error(g_error);
	response.set_supported_features(
		::google::protobuf::compiler::CodeGeneratorResponse::FEATURE_PROTO3_OPTIONAL);
	if (!response.SerializeToFileDescriptor(STDOUT_FILENO)) {
		std::cerr << "failed to serialize CodeGeneratorResponse" << std::endl;
		return 2;
	}
	return 0;
}
