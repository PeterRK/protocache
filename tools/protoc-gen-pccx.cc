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

static std::string ConvertFilename(const std::string& name, bool trim=false) {
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name + ".pc.h";
	}
	auto sep = std::string::npos;
	if (trim) {
#ifdef _WIN32
		sep = name.rfind('\\', pos);
#else
		sep= name.rfind('/', pos);
#endif
	}
	if (sep == std::string::npos) {
		return name.substr(0, pos+1) + "pc.h";
	}
	return name.substr(sep+1, pos-sep) + "pc.h";
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

	oss << "\texplicit " << proto.name() << "(const uint32_t* ptr, const uint32_t* end=nullptr) : core_(ptr, end) {}\n"
		<< "\texplicit " << proto.name() << "(const protocache::Slice<uint32_t>& data) : "
		<< proto.name() << "(data.begin(), data.end()) {}\n"
		<< "\tbool operator!() const noexcept { return !core_; }\n"
		<< "\tbool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return core_.HasField(id,end); }\n\n"
		<< "\tstatic protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {\n"
		<< "\t\tauto view = protocache::Message::Detect(ptr, end);\n"
		<< "\t\tif (!view) return {};\n"
		<< "\t\tprotocache::Message core(ptr);\n"
		<< "\t\tprotocache::Slice<uint32_t> t;\n";

	auto get_map_type = [](const ::google::protobuf::DescriptorProto* unit)->std::string{
		auto& key_field = unit->field(0);
		auto& value_field = unit->field(1);
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
		std::string out = "protocache::MapT<";
		out += key;
		out += ',';
		out += value;
		out += '>';
		return out;
	};

	auto handle_detect = [&oss](const std::string& field_name, const char* out_type) {
		oss << "\t\tt = protocache::DetectField<" << out_type << ">(core, _::" << field_name << ", end);\n"
			<< "\t\tif (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};\n";
	};

	for (int i = proto.field_size()-1; i >= 0; i--) {
		auto& one = proto.field(i);
		if (one.options().deprecated()) {
			continue;
		}
		if (one.label() == ::google::protobuf::FieldDescriptorProto::LABEL_REPEATED) {
			switch (one.type()) {
				case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
				{
					auto it = map_entries.find(one.type_name());
					if (it != map_entries.end()) {
						auto out_type = get_map_type(it->second);
						if (out_type.empty()) {
							return {};
						}
						handle_detect(one.name(), out_type.c_str());
					} else {
						std::string out_type = "protocache::ArrayT<";
						out_type += TypeName(one.type(), one.type_name());
						out_type += '>';
						handle_detect(one.name(), out_type.c_str());
					}
				}
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
					handle_detect( one.name(), "protocache::ArrayT<protocache::Slice<uint8_t>>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
					handle_detect(one.name(), "protocache::ArrayT<protocache::Slice<char>>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
					handle_detect(one.name(), "protocache::ArrayT<double>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
					handle_detect( one.name(), "protocache::ArrayT<float>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
				case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
					handle_detect(one.name(), "protocache::ArrayT<uint64_t>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
				case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
					handle_detect(one.name(), "protocache::ArrayT<uint32_t>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
				case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
				case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
					handle_detect(one.name(), "protocache::ArrayT<int64_t>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
				case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
				case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
					handle_detect(one.name(), "protocache::ArrayT<int32_t>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
					handle_detect(one.name(), "protocache::ArrayT<bool>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
					handle_detect(one.name(), "protocache::ArrayT<protocache::EnumValue>");
					break;
				default:
					break;
			}
		} else {
			switch (one.type()) {
				case ::google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
					handle_detect(one.name(), TypeName(one.type(), one.type_name()).c_str());
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
					handle_detect(one.name(), "protocache::Slice<uint8_t>");
					break;
				case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
					handle_detect(one.name(), "protocache::Slice<char>");
					break;
				default:
					break;
			}
		}
	}
	oss << "\t\treturn view;\n"
		<< "\t}\n\n";

	auto handle_get = [&oss](bool repeated, const std::string& field_name, const char* out_type) {
		std::string tmp;
		if (repeated) {
			tmp = "protocache::ArrayT<";
			tmp += out_type;
			tmp += '>';
			out_type = tmp.c_str();
		}
		oss << '\t' << out_type << ' ' << field_name << "(const uint32_t* end=nullptr) const noexcept {\n"
			<< "\t\treturn protocache::GetField<" << out_type << ">(core_, _::" << field_name << ", end);\n"
			<< "\t}\n";
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
						auto out_type = get_map_type(it->second);
						if (out_type.empty()) {
							return {};
						}
						handle_get(false, one.name(), out_type.c_str());
						break;
					}
				}
				handle_get(repeated, one.name(), TypeName(one.type(), one.type_name()).c_str());
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
				handle_get(repeated, one.name(), "protocache::Slice<uint8_t>");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
				handle_get(repeated, one.name(), "protocache::Slice<char>");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
				handle_get(repeated, one.name(), "double");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
				handle_get(repeated, one.name(), "float");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
				handle_get(repeated, one.name(), "uint64_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
				handle_get(repeated, one.name(), "uint32_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
				handle_get(repeated, one.name(), "int64_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
				handle_get(repeated, one.name(), "int32_t");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
				handle_get(repeated, one.name(), "bool");
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
				handle_get(repeated, one.name(), "protocache::EnumValue");
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

static const std::unordered_map<std::string, std::string> g_predefined_proto = {
		{"google/protobuf/any.proto", "protocache/protobuf/any.proto"},
		{"google/protobuf/timestamp.proto", "protocache/protobuf/timestamp.proto"},
		{"google/protobuf/duration.proto", "protocache/protobuf/duration.proto"},
		{"google/protobuf/wrappers.proto", "protocache/protobuf/wrappers.proto"},
};

static bool IsIgnoredDependency(const std::string& path) {
	static const std::string prefix = "google/protobuf/";
	if (path.size() >= prefix.size()
		&& memcmp(path.data(), prefix.data(), prefix.size()) == 0) {
		return true;
	}
	return false;
}

static std::string GenFile(const ::google::protobuf::FileDescriptorProto& proto) {
	std::ostringstream oss;
	auto header_name = HeaderName(proto.name());
	oss << "#pragma once\n"
		<< "#ifndef PROTOCACHE_INCLUDED_" << header_name << '\n'
		<< "#define PROTOCACHE_INCLUDED_" << header_name << '\n';

	oss << "\n#include <protocache/access.h>\n";
	for (auto& one : proto.dependency()) {
		auto it = g_predefined_proto.find(one);
		if (it != g_predefined_proto.end()) {
			oss << "#include <" << ConvertFilename(it->second) << ">\n";
		} else if (!IsIgnoredDependency(one)) {
			oss << "#include \"" << ConvertFilename(one) << "\"\n";
		}
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

static std::string GenMessageEX(const std::string& ns, const ::google::protobuf::DescriptorProto& proto) {
	auto fullname = NaiveJoinName(ns, proto.name());
	std::ostringstream oss;
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;

	auto alias_mode = proto.field_size() == 1 && proto.field(0).name() == "_";

	oss << "struct " << proto.name() << " final {\n";

	for (auto& one : proto.nested_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.options().map_entry()) {
			map_entries.emplace(NaiveJoinName(fullname, one.name()), &one);
			continue;
		}
		auto piece = GenMessageEX(fullname, one);
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
		auto value = TypeName(it->second.value_type, it->second.value_class, true);
		if (value.empty()) {
			std::cerr << "illegal value type: " << it->second.value_type << std::endl;
			return {};
		}
		if (it->second.key_type == TYPE_NONE) {
			oss << "\tusing ALIAS = protocache::ArrayEX<" << value << ">;\n";
		} else {
			if (!CanBeKey(it->second.key_type)) {
				std::cerr << "illegal key type: " << it->second.key_type << std::endl;
				return {};
			}
			auto key = TypeName(it->second.key_type, {}, true);
			oss << "\tusing ALIAS = protocache::MapEX<" << key << ',' << value << ">;\n";
		}
		oss << "};\n\n";
		return oss.str();
	}

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

	int max_id = 1;
	for (auto& field : proto.field()) {
		max_id = std::max(max_id, field.number());
	}

	oss << '\t' << proto.name() << "() = default;\n"
		<< "\texplicit " << proto.name() << "(const uint32_t* data, const uint32_t* end=nullptr) : __view__(data, end) {}\n"
		<< "\texplicit " << proto.name() << "(const protocache::Slice<uint32_t>& data) : "
		<< proto.name() << "(data.begin(), data.end()) {}\n"
		<< "\tstatic protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {\n"
		<< "\t\treturn " << cxx_ns << "Detect(ptr, end);\n"
		<< "\t}\n"
		<< "\tprotocache::Data Serialize(const uint32_t* end=nullptr) const {\n"
		<< "\t\tauto clean_head = __view__.CleanHead();\n"
		<< "\t\tif (clean_head != nullptr) {\n"
		<< "\t\t\tauto view = Detect(clean_head, end);\n"
		<< "\t\t\treturn {view.data(), view.size()};\n"
		<< "\t\t}\n"
		<< "\t\tstd::vector<protocache::Data> raw(" << max_id << ");\n"
		<< "\t\tstd::vector<protocache::Slice<uint32_t>> parts(" << max_id << ");\n";

	for (auto& one : proto.field()) {
		if (one.options().deprecated()) {
			continue;
		}
		oss << "\t\tparts[_::" << one.name() << "] = __view__.SerializeField(_::"
			<< one.name() << ", end, _" << one.name() << ", raw[_::" << one.name() << "]);\n";
	}
	oss << "\t\treturn protocache::SerializeMessage(parts);\n"
		<< "\t}\n\n";

	std::vector<std::string> types(proto.field_size());
	for (int i = 0; i < proto.field_size(); i++) {
		auto& one = proto.field(i);
		if (one.options().deprecated()) {
			continue;
		}
		if (one.name() == "_") {
			std::cerr << "found illegal field in message " << fullname << std::endl;
			return {};
		}
		auto& type = types[i];
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
						type = "protocache::MapEX<";
						type += key;
						type += ',';
						type += value;
						type += '>';
					} else {
						type = "protocache::ArrayEX<";
						type += TypeName(one.type(), one.type_name(), true);
						type += '>';
					}
				} else {
					type = TypeName(one.type(), one.type_name(), true);
				}
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BYTES:
				type = repeated? "protocache::ArrayEX<protocache::Slice<uint8_t>>" : "std::string";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_STRING:
				type = repeated? "protocache::ArrayEX<protocache::Slice<char>>" : "std::string";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
				type = repeated? "protocache::ArrayEX<double>" : "double";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
				type = repeated? "protocache::ArrayEX<float>" : "float";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT64:
				type = repeated? "protocache::ArrayEX<uint64_t>" : "uint64_t";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_UINT32:
				type = repeated? "protocache::ArrayEX<uint32_t>" : "uint32_t";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT64:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT64:
				type = repeated? "protocache::ArrayEX<int64_t>" : "int64_t";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_SINT32:
			case ::google::protobuf::FieldDescriptorProto::TYPE_INT32:
				type = repeated? "protocache::ArrayEX<int32_t>" : "int32_t";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_BOOL:
				type = repeated? "protocache::ArrayEX<bool>" : "bool";
				break;
			case ::google::protobuf::FieldDescriptorProto::TYPE_ENUM:
				type = repeated? "protocache::ArrayEX<protocache::EnumValue>" : "protocache::EnumValue";
				break;
			default:
				std::cerr << "unsupported field " << one.name() << " in message " << proto.name() << std::endl;
				return {};
		}
		oss << '\t' << type << "& " << one.name() << "(const uint32_t* end=nullptr) { return __view__.GetField(_::"
			<< one.name() << ", end, _" << one.name() << "); }\n";
	}

	oss << "\nprivate:\n"
		<< "\tusing _ = " << cxx_ns << "_;\n"
		<< "\tprotocache::MessageEX<" << max_id << "> __view__;\n";
	for (int i = 0; i < proto.field_size(); i++) {
		auto &one = proto.field(i);
		if (one.options().deprecated()) {
			continue;
		}
		oss << '\t' << types[i] << " _" << one.name() << ";\n";
	}
	oss << "};\n\n";
	return oss.str();
}

static std::string ConvertExFilename(const std::string& name) {
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name + ".pc-ex.h";
	}
	return name.substr(0, pos+1) + "pc-ex.h";
}

static std::string GenFileEX(const ::google::protobuf::FileDescriptorProto& proto) {
	std::ostringstream oss;
	auto header_name = HeaderName(proto.name());
	oss << "#pragma once\n"
		<< "#ifndef PROTOCACHE_INCLUDED_EX_" << header_name << '\n'
		<< "#define PROTOCACHE_INCLUDED_EX_" << header_name << '\n';

	oss << "\n#include <protocache/access-ex.h>\n"
		<< "#include \"" << ConvertFilename(proto.name(), true) << "\"\n";
	for (auto& one : proto.dependency()) {
		auto it = g_predefined_proto.find(one);
		if (it != g_predefined_proto.end()) {
			oss << "#include <" << ConvertExFilename(it->second) << ">\n";
		} else if (!IsIgnoredDependency(one)) {
			oss << "#include \"" << ConvertExFilename(one) << "\"\n";
		}
	}
	oss << '\n';

	std::string ns;
	std::vector<std::string> ns_parts;
	if (!proto.package().empty()) {
		ns = '.' + proto.package();
		Split(proto.package(), '.', &ns_parts);
	}
	oss << "namespace ex {\n";
	for (auto& part : ns_parts) {
		oss << "namespace " << part << " {\n";
	}
	oss << '\n';

	for (auto& one : proto.message_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		oss << GenMessageEX(ns, one);
	}

	for (auto it = ns_parts.rbegin(); it != ns_parts.rend(); ++it) {
		oss << "} // " << *it << '\n';
	}
	oss << "} //ex\n";
	oss << "#endif // PROTOCACHE_INCLUDED_" << header_name << '\n';
	return oss.str();
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
			auto ex = response.add_file();
			ex->set_name(ConvertExFilename(proto.name()));
			ex->set_content(GenFileEX(proto));
		}
	}

	if (!response.SerializeToFileDescriptor(STDOUT_FILENO)) {
		std::cerr << "fail to response" << std::endl;
		return 2;
	}
	return 0;
}