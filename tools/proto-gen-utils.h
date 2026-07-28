#pragma once

#include <cctype>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/compiler/plugin.pb.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#else
#include <unistd.h>
#endif

static inline bool PrepareProtocPluginIO() noexcept {
#if defined(_WIN32)
	return _setmode(_fileno(stdin), _O_BINARY) != -1 &&
		   _setmode(_fileno(stdout), _O_BINARY) != -1;
#else
	return true;
#endif
}

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

static inline bool CanBeKey(::google::protobuf::FieldDescriptorProto::Type type) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
			return true;
		default:
			return false;
	}
}

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

static inline bool IsAlias(
		const ::google::protobuf::DescriptorProto& proto,
		bool extended = false) {
	if (proto.field_size() != 1) return false;
	const auto& name = proto.field(0).name();
	return name == "_" || (extended && name == "_x_");
}

static inline bool IsSupportedProtoCacheFieldType(
		::google::protobuf::FieldDescriptorProto::Type type) noexcept {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
		case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
		case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
		case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
		case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
			return true;
		default:
			return false;
	}
}

static inline bool ProtoCacheSchemaError(
		std::string* error, const std::string& message) {
	if (error != nullptr) *error = message;
	return false;
}

// Generation feasibility is intentionally weaker than portable schema conformance.
// Reject only structures that these generators cannot represent reliably.
static bool ValidateGeneratorMessage(
		const std::string& ns,
		const ::google::protobuf::DescriptorProto& proto,
		std::string* error,
		bool extended_alias) {
	using FieldProto = ::google::protobuf::FieldDescriptorProto;
	if (proto.options().map_entry() || proto.options().deprecated()) return true;
	const auto fullname = NaiveJoinName(ns, proto.name());
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;
	for (const auto& nested : proto.nested_type()) {
		if (nested.options().deprecated()) continue;
		if (nested.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, nested.name()), &nested);
		} else if (!ValidateGeneratorMessage(fullname, nested, error, extended_alias)) {
			return false;
		}
	}

	auto fields = FieldsInOrder(proto);
	const auto alias = IsAlias(proto, extended_alias);
	if (!alias) {
		std::unordered_set<int> numbers;
		std::unordered_set<std::string> names;
		for (const auto* field : fields) {
			if (field->number() <= 0 || field->number() > 6387) {
				return ProtoCacheSchemaError(
					error, "field number is outside the ProtoCache range: " +
						fullname + "." + field->name());
			}
			if (!numbers.insert(field->number()).second ||
					!names.insert(field->name()).second) {
				return ProtoCacheSchemaError(
					error, "duplicate field name or number: " +
						fullname + "." + field->name());
			}
		}
	}

	auto validate_type = [&](const FieldProto& field)->bool {
		if (!field.has_type() || !IsSupportedProtoCacheFieldType(field.type())) {
			return ProtoCacheSchemaError(
				error, "unsupported field type: " + fullname + "." + field.name());
		}
		if (field.type() != FieldProto::TYPE_MESSAGE) return true;
		auto found = map_entries.find(field.type_name());
		if (found == map_entries.end()) return true;
		const auto& entry = *found->second;
		if (entry.field_size() != 2 || !CanBeKey(entry.field(0).type())) {
			return ProtoCacheSchemaError(
				error, "unsupported map key type: " + fullname + "." + field.name());
		}
		if (!entry.field(1).has_type() ||
				!IsSupportedProtoCacheFieldType(entry.field(1).type())) {
			return ProtoCacheSchemaError(
				error, "unsupported map value type: " + fullname + "." + field.name());
		}
		return true;
	};

	if (alias) {
		const auto& field = proto.field(0);
		if (!IsRepeated(field)) {
			return ProtoCacheSchemaError(
				error, "alias field cannot be represented as a container: " + fullname);
		}
		return validate_type(field);
	}

	for (const auto* field : fields) {
		if (field->name() == "_") {
			return ProtoCacheSchemaError(
				error, "non-alias message contains reserved alias field: " + fullname + "." +
					field->name());
		}
		if (!validate_type(*field)) return false;
	}
	return true;
}

static bool ValidateGeneratorInput(
		const ::google::protobuf::compiler::CodeGeneratorRequest& request,
		std::string* error,
		bool extended_alias = false) {
	std::unordered_set<std::string> files;
	files.reserve(request.file_to_generate_size());
	for (const auto& file : request.file_to_generate()) files.insert(file);
	for (const auto& file : request.proto_file()) {
		if (files.find(file.name()) == files.end()) continue;
		const std::string ns = file.package().empty() ? "" : "." + file.package();
		for (const auto& message : file.message_type()) {
			if (!ValidateGeneratorMessage(ns, message, error, extended_alias)) return false;
		}
	}
	return true;
}

static std::string ToPascal(const std::string& name) {
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
