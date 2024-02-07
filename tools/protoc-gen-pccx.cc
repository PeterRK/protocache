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

static bool CollectAlias(const std::string& ns, const ::google::protobuf::DescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;
	for (auto& one : proto.nested_type()) {
		if (one.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, one.name()), &one);
			continue;
		}
		if (!CollectAlias(fullname, one)) {
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

static std::string ConvertFilename(const std::string& name) {
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name + ".pc.h";
	}
	return name.substr(0, pos+1) + "pc.h";
}

static std::string TypeName(::google::protobuf::FieldDescriptorProto::Type type, const std::string& clazz, bool extra=false) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
		{
			std::string name;
			if (extra) {
				name += "::ex";
			}
			for (auto ch : clazz) {
				if (ch == '.') {
					name += "::";
				} else {
					name += ch;
				}
			}
			if (g_alias_book.find(clazz) != g_alias_book.end()) {
				name += "::ALIAS";
			}
			return name;
		}
		case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
			return "protocache::Slice<uint8_t>";
		case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
			return "protocache::Slice<char>";
		case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
			return "double";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
			return "float";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
			return "uint64_t";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
			return "uint32_t";
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
			return "int64_t";
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
			return "int32_t";
		case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
			return "bool";
		case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
			return "protocache::EnumValue";
		default:
			return {};
	}
}

static const char* TypeMark(::google::protobuf::FieldDescriptorProto::Type type) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
			return "bytes";
		case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
			return "str";
		case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
			return "f64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
			return "f32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
			return "u64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
			return "u32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
			return "i64";
		case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
			return "i32";
		case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
			return "bool";
		case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
			return "enum";
		default:
			return nullptr;
	}
}

static bool CanBeKey(::google::protobuf::FieldDescriptorProto::Type type) {
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

static std::string GenEnum(const ::google::protobuf::EnumDescriptorProto& proto) {
	std::ostringstream oss;
	oss << "enum " << proto.name() << " : int32_t {\n";
	auto n = proto.value_size();
	for (int i = 0; i < n; i++) {
		auto& value = proto.value(i);
		if (value.options().deprecated()) {
			continue;
		}
		oss << '\t' << value.name() << " = " << value.number();
		if (i != n-1) {
			oss << ',';
		}
		oss << '\n';
	}
	oss << "};\n\n";
	return oss.str();
}

static std::string GenMessage(const std::string& ns, const ::google::protobuf::DescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	std::ostringstream oss;
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;

	auto alias_mode = proto.field_size() == 1 && proto.field(0).name() == "_";

	if (alias_mode) {
		oss << "struct " << proto.name() << " final {\n";
	} else {
		oss << "class " << proto.name() << " final {\n"
			<< "private:\n"
			<< "\tprotocache::Message core_;\n"
			<< "public:\n"
			<< "\tstruct _ {\n";
		for (auto& one : proto.field()) {
			if (one.options().deprecated()) {
				continue;
			}
			if (one.name() == "_") {
				std::cerr << "found illegal field in message " << fullname << std::endl;
				return {};
			}
			oss << "\t\tstatic constexpr unsigned " << one.name() << " = " << (one.number()-1) << ";\n";
		}
		oss << "\t};\n\n";
	}

	for (auto& one : proto.enum_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		oss << AddIndent(GenEnum(one));
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

	if (alias_mode) {
		auto it = g_alias_book.find(fullname);
		if (it == g_alias_book.end()) {
			std::cerr << "alias lost: " << fullname << std::endl;
			return {};
		}
		auto value = TypeName(it->second.value_type, it->second.value_class);
		if (value.empty()) {
			std::cerr << "illegal value type: " << it->second.value_type << std::endl;
			return {};
		}
		if (it->second.key_type == TYPE_NONE) {
			oss << "\tusing ALIAS = protocache::ArrayT<" << value << ">;\n";
		} else {
			if (!CanBeKey(it->second.key_type)) {
				std::cerr << "illegal key type: " << it->second.key_type << std::endl;
				return {};
			}
			auto key = TypeName(it->second.key_type, {});
			oss << "\tusing ALIAS = protocache::MapT<" << key << ',' << value << ">;\n";
		}
		oss << "};\n\n";
		return oss.str();
	}

	oss << "\texplicit " << proto.name() << "(const protocache::Message& message) : core_(message) {}\n"
		<< "\texplicit " << proto.name() << "(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}\n"
		<< "\tbool operator!() const noexcept { return !core_; }\n"
		<< "\tbool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return core_.HasField(id,end); }\n\n";

	auto handle_simple_field = [&oss](bool repeated,
									  const std::string& field_name, const char* out_type, const char* get_type) {
		if (repeated) {
			oss << "\tprotocache::ArrayT<" << out_type << "> " << field_name << "(const uint32_t* end=nullptr) const noexcept {\n"
				<< "\t\treturn protocache::GetArray<" << out_type << ">(core_, _::" << field_name << ", end);\n"
				<< "\t}\n";
		} else {
			oss << '\t' << out_type << ' ' << field_name << "(const uint32_t* end=nullptr) const noexcept {\n"
				<< "\t\treturn protocache::Get" << get_type << "(core_, _::" << field_name << ", end);\n"
				<< "\t}\n";
		}
	};

	for (auto& one : proto.field()) {
		if (one.options().deprecated()) {
			continue;
		}
		auto repeated = one.label() == ::google::protobuf::FieldDescriptorProto::LABEL_REPEATED;
		switch (one.type()) {
			case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
				if (repeated) { // array or map
					auto it = map_entries.find(one.type_name());
					if (it != map_entries.end()) {
						auto& key_field = it->second->field(0);
						auto& value_field = it->second->field(1);
						if (!CanBeKey(key_field.type())) {
							std::cerr << "illegal key type: " << key_field.type() << std::endl;
							return {};
						}
						auto key = TypeName(key_field.type(), {});
						auto value = TypeName(value_field.type(), value_field.type_name());
						if (value.empty()) {
							std::cerr << "illegal value type: " << value_field.type() << std::endl;
							return {};
						}
						oss << "\tprotocache::MapT<" << key << ',' << value << "> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
							<< "\t\treturn protocache::GetMap<" << key << ',' << value << ">(core_, _::" << one.name() << ", end);\n"
							<< "\t}\n";
					} else {
						auto value = TypeName(one.type(), one.type_name());
						oss << "\tprotocache::ArrayT<" << value << "> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
							<< "\t\treturn protocache::GetArray<" << value << ">(core_, _::" << one.name() << ", end);\n"
							<< "\t}\n";
					}
				} else {
					auto name = TypeName(one.type(), one.type_name());
					oss << '\t' << name << ' ' << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn " << name << "(core_.GetField(_::" << one.name() << ", end).GetObject(end), end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
				handle_simple_field(repeated, one.name(), "protocache::Slice<uint8_t>", "Bytes");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
				handle_simple_field(repeated, one.name(), "protocache::Slice<char>", "String");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
				handle_simple_field(repeated, one.name(), "double", "Double");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
				handle_simple_field(repeated, one.name(), "float", "Float");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
				handle_simple_field(repeated, one.name(), "uint64_t", "UInt64");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
				handle_simple_field(repeated, one.name(), "uint32_t", "UInt32");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
				handle_simple_field(repeated, one.name(), "int64_t", "Int64");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
				handle_simple_field(repeated, one.name(), "int32_t", "Int32");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
				handle_simple_field(repeated, one.name(), "bool", "Bool");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
				handle_simple_field(repeated, one.name(), "protocache::EnumValue", "EnumValue");
				break;
			default:
				std::cerr << "unsupported field " << one.name() << " in message " << proto.name() << std::endl;
				return {};
		}
	}

	oss << "};\n\n";
	return oss.str();
}

static std::string HeaderName(const std::string& proto_name) {
	auto out = proto_name;
	for (auto& ch : out) {
		if (!std::isalnum(ch)) {
			ch = '_';
		}
	}
	return out;
}

static std::string GenFile(const ::google::protobuf::FileDescriptorProto& proto) {
	std::ostringstream oss;
	auto header_name = HeaderName(proto.name());
	oss << "#pragma once\n"
		<< "#ifndef PROTOCACHE_INCLUDED_" << header_name << '\n'
		<< "#define PROTOCACHE_INCLUDED_" << header_name << '\n';

	oss << "\n#include <protocache/access.h>\n";
	for (auto& one : proto.dependency()) {
		oss << "#include \"" << ConvertFilename(one) << "\"\n";
	}
	oss << '\n';

	std::string ns;
	std::vector<std::string> ns_parts;
	if (!proto.package().empty()) {
		ns = '.' + proto.package();
		Split(proto.package(), '.', &ns_parts);
	}
	for (auto& part : ns_parts) {
		oss << "namespace " << part << " {\n";
	}
	oss << '\n';

	for (auto& one : proto.enum_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		oss << GenEnum(one);
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
	for (auto it = ns_parts.rbegin(); it != ns_parts.rend(); ++it) {
		oss << "} // " << *it << '\n';
	}

	oss << "#endif // PROTOCACHE_INCLUDED_" << header_name << '\n';
	return oss.str();
}

struct CodePair {
	std::string header;
	std::string source;
};

static CodePair GenMessageEX(const std::string& ns, const ::google::protobuf::DescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	std::ostringstream oss_h, oss_s;
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;

	auto alias_mode = proto.field_size() == 1 && proto.field(0).name() == "_";

	oss_h << "struct " << proto.name() << " final {\n";

	for (auto& one : proto.nested_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, one.name()), &one);
			continue;
		}
		auto piece = GenMessageEX(fullname, one);
		oss_h << AddIndent(piece.header);
		oss_s << piece.source;
	}

	if (alias_mode) {
		auto it = g_alias_book.find(fullname);
		if (it == g_alias_book.end()) {
			std::cerr << "alias lost: " << fullname << std::endl;
			return {};
		}
		auto value = TypeName(it->second.value_type, it->second.value_class, true);
		if (value.empty()) {
			std::cerr << "illegal value type: " << it->second.value_type << std::endl;
			return {};
		}
		if (it->second.key_type == TYPE_NONE) {
			oss_h << "\tusing ALIAS = protocache::ArrayEX<" << value << ">;\n";
		} else {
			if (!CanBeKey(it->second.key_type)) {
				std::cerr << "illegal key type: " << it->second.key_type << std::endl;
				return {};
			}
			auto key = TypeName(it->second.key_type, {}, true);
			oss_h << "\tusing ALIAS = protocache::MapEX<" << key << ',' << value << ">;\n";
		}
		oss_h << "};\n\n";
		return {oss_h.str(), oss_s.str()};
	}

	oss_h << '\t' << proto.name() << "() = default;\n"
		  << '\t' << proto.name() << "(const uint32_t* data, const uint32_t* end);\n"
		  << "\texplicit " << proto.name() << "(const protocache::Slice<uint32_t>& data) : "
		  << proto.name() << "(data.begin(), data.end()) {}\n"
		  << "\tprotocache::Data Serialize() const;\n\n";

	auto define_simple_field = [&oss_h](bool repeated, const std::string& field_name,
			const char* out_type, const char* get_type=nullptr) {
		if (repeated) {
			oss_h << "\tprotocache::ArrayEX<" << (get_type != nullptr? get_type : out_type) << "> " << field_name << ";\n";
		} else {
			oss_h << '\t' << out_type << ' ' << field_name << ";\n";
		}
	};

	for (auto& one : proto.field()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.name() == "_") {
			std::cerr << "found illegal field in message " << fullname << std::endl;
			return {};
		}
		auto repeated = one.label() == ::google::protobuf::FieldDescriptorProto::LABEL_REPEATED;
		switch (one.type()) {
			case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
				if (repeated) { // array or map
					auto it = map_entries.find(one.type_name());
					if (it != map_entries.end()) {
						auto& key_field = it->second->field(0);
						auto& value_field = it->second->field(1);
						if (!CanBeKey(key_field.type())) {
							std::cerr << "illegal key type: " << key_field.type() << std::endl;
							return {};
						}
						auto key = TypeName(key_field.type(), {});
						auto value = TypeName(value_field.type(), value_field.type_name(), true);
						if (value.empty()) {
							std::cerr << "illegal value type: " << value_field.type() << std::endl;
							return {};
						}
						oss_h << "\tprotocache::MapEX<" << key << ',' << value << "> " << one.name() << ";\n";
					} else {
						auto value = TypeName(one.type(), one.type_name(), true);
						oss_h << "\tprotocache::ArrayEX<" << value << "> " << one.name() << ";\n";
					}
				} else {
					auto name = TypeName(one.type(), one.type_name(), true);
					oss_h << '\t' << name << ' ' << one.name() << ";\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
				define_simple_field(repeated, one.name(), "std::string", "protocache::Slice<uint8_t>");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
				define_simple_field(repeated, one.name(), "std::string", "protocache::Slice<char>");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
				define_simple_field(repeated, one.name(), "double");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
				define_simple_field(repeated, one.name(), "float");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
				define_simple_field(repeated, one.name(), "uint64_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
				define_simple_field(repeated, one.name(), "uint32_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
				define_simple_field(repeated, one.name(), "int64_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
				define_simple_field(repeated, one.name(), "int32_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
				define_simple_field(repeated, one.name(), "bool");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
				define_simple_field(repeated, one.name(), "protocache::EnumValue");
				break;
			default:
				std::cerr << "unsupported field " << one.name() << " in message " << proto.name() << std::endl;
				return {};
		}
	}
	oss_h << "};\n\n";

	std::string cxx_ns;
	for (auto ch : ns) {
		if (ch == '.') {
			cxx_ns += "::";
		} else {
			cxx_ns += ch;
		}
	}
	cxx_ns += "::";
	cxx_ns += proto.name();
	cxx_ns += "::";

	oss_s << "ex" << cxx_ns << proto.name() << "(const uint32_t* _data, const uint32_t* _end) {\n"
		  << "\tusing _ = " << cxx_ns << "_;\n"
		  << "\tprotocache::Message _view(_data, _end);\n";
	for (auto& one : proto.field()) {
		if (one.options().deprecated()) {
			continue;
		}
		oss_s << "\tprotocache::ExtractField(_view, _::" << one.name() << ", _end, &" << one.name() << ");\n";
	}
	oss_s << "};\n\n"
		  << "protocache::Data ex" << cxx_ns << "Serialize() const {\n"
		  << "\tusing _ = " << cxx_ns << "_;\n"
		  << "\tstd::vector<protocache::Data> parts(" << proto.field_size() << ");\n";
	for (auto& one : proto.field()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.label() == ::google::protobuf::FieldDescriptorProto::LABEL_REPEATED) {
			oss_s << "\tif (!" << one.name() << ".empty()) {\n"
				<< "\t\tparts[_::" << one.name() << "] = protocache::Serialize("  << one.name() <<  ");\n"
				<< "\t\tif (parts[_::" << one.name() << "].empty()) return {};\n"
				<< "\t}\n";
			continue;
		}
		switch (one.type()) {
			case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
				oss_s << "\tparts[_::" << one.name() << "] = protocache::Serialize(" << one.name() << ");\n"
					<< "\tif (parts[_::" << one.name() << "].empty()) return {};\n"
					<< "\tif (parts[_::" << one.name() << "].size() == 1) { parts[_::" << one.name() << "].clear(); }\n";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
			case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
				oss_s << "\tif (!" << one.name() << ".empty()) {\n"
					  << "\t\tparts[_::" << one.name() << "] = protocache::Serialize("  << one.name() <<  ");\n"
					  << "\t\tif (parts[_::" << one.name() << "].empty()) return {};\n"
					  << "\t}\n";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
			case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
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
			case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
				oss_s << "\tif (" << one.name() << " != 0) { parts[_::" << one.name() << "] = protocache::Serialize(" << one.name() <<  "); }\n";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
				oss_s << "\tif (" << one.name() << ") { parts[_::" << one.name() << "] = protocache::Serialize(" << one.name() <<  "); }\n";
				break;
			default:
				return {};
		}
	}
	oss_s << "\treturn protocache::SerializeMessage(parts);\n}\n\n";

	return {oss_h.str(), oss_s.str()};
}

static std::string ConvertExHeaderFilename(const std::string& name) {
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name + ".pc-ex.h";
	}
	return name.substr(0, pos+1) + "pc-ex.h";
}

static std::string ConvertExSourceFilename(const std::string& name) {
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name + ".pc-ex.cc";
	}
	return name.substr(0, pos+1) + "pc-ex.cc";
}

static CodePair GenFileEX(const ::google::protobuf::FileDescriptorProto& proto) {
	std::ostringstream oss_h, oss_s;
	auto header_name = HeaderName(proto.name());
	oss_h << "#pragma once\n"
		<< "#ifndef PROTOCACHE_INCLUDED_EX_" << header_name << '\n'
		<< "#define PROTOCACHE_INCLUDED_EX_" << header_name << '\n';

	oss_h << "\n#include <protocache/access-ex.h>\n";
	for (auto& one : proto.dependency()) {
		oss_h << "#include \"" << ConvertExHeaderFilename(one) << "\"\n";
	}
	oss_h << '\n';

	oss_s << "\n#include \"" << ConvertFilename(proto.name())  << "\"\n"
		 << "#include \"" << ConvertExHeaderFilename(proto.name())  << "\"\n\n";

	std::string ns;
	std::vector<std::string> ns_parts;
	if (!proto.package().empty()) {
		ns = '.' + proto.package();
		Split(proto.package(), '.', &ns_parts);
	}
	oss_h << "namespace ex {\n";
	for (auto& part : ns_parts) {
		oss_h << "namespace " << part << " {\n";
	}
	oss_h << '\n';

	for (auto& one : proto.message_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		auto piece = GenMessageEX(ns, one);
		oss_h << piece.header;
		oss_s << piece.source;
	}

	for (auto it = ns_parts.rbegin(); it != ns_parts.rend(); ++it) {
		oss_h << "} // " << *it << '\n';
	}
	oss_h << "} //ex\n";
	oss_h << "#endif // PROTOCACHE_INCLUDED_" << header_name << '\n';
	return {oss_h.str(), oss_s.str()};
}

int main() {
	::google::protobuf::compiler::CodeGeneratorRequest request;
	::google::protobuf::compiler::CodeGeneratorResponse response;

	if (!request.ParseFromFileDescriptor(STDIN_FILENO)) {
		std::cerr << "fail to get request" << std::endl;
		return 1;
	}

	bool extra = request.parameter() == "extra";

	for (auto& proto : request.proto_file()) {
		std::string ns;
		if (!proto.package().empty()) {
			ns = '.' + proto.package();
		}
		for (auto& one : proto.message_type()) {
			if (!CollectAlias(ns, one)) {
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
		if (extra) {
			auto extra_code = GenFileEX(proto);
			auto header = response.add_file();
			header->set_name(ConvertExHeaderFilename(proto.name()));
			header->set_content(extra_code.header);
			auto source = response.add_file();
			source->set_name(ConvertExSourceFilename(proto.name()));
			source->set_content(extra_code.source);
		}
	}

	if (!response.SerializeToFileDescriptor(STDOUT_FILENO)) {
		std::cerr << "fail to response" << std::endl;
		return 2;
	}
	return 0;
}