#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <unistd.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/compiler/plugin.pb.h>


struct AliasUnit {
	::google::protobuf::FieldDescriptorProto_Type key_type;
	::google::protobuf::FieldDescriptorProto_Type value_type;
	std::string value_class;
};

static std::unordered_map<std::string, AliasUnit> g_alias_book;

static bool CollectAlias(const std::string& ns, const ::google::protobuf::DescriptorProto& proto) {
	std::string fullname = ns;
	fullname += '.';
	fullname += proto.name();
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;
	for (auto& one : proto.nested_type()) {
		if (one.options().map_entry()) {
			map_entries.emplace(fullname + '.' + one.name(), &one);
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
	if (field.label() != ::google::protobuf::FieldDescriptorProto_Label_LABEL_REPEATED) {
		std::cerr << "illegal alias: " << fullname << std::endl;
		return false;
	}
	AliasUnit unit;
	unit.key_type = static_cast<::google::protobuf::FieldDescriptorProto_Type>(-1);
	unit.value_type = field.type();
	if (field.type() != ::google::protobuf::FieldDescriptorProto_Type_TYPE_MESSAGE) {
		g_alias_book.emplace(fullname, unit);
		return true;
	}
	auto it = map_entries.find(field.type_name());
	if (it != map_entries.end()) {
		unit.key_type = it->second->field(0).type();
		unit.value_type = it->second->field(1).type();
		unit.value_class = it->second->field(1).type_name();
	} else {
		unit.value_class = field.type_name();
	}
	g_alias_book.emplace(fullname, unit);
	return true;
}

static std::string ConvertFilename(const std::string& name) {
	auto pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name + ".pc.h";
	}
	return name.substr(0, pos+1) + "pc.h";
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

static std::string TypeName(::google::protobuf::FieldDescriptorProto_Type type, const std::string& clazz) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_MESSAGE:
		{
			std::string name;
			for (auto ch : clazz) {
				if (ch == '.') {
					name += "::";
				} else {
					name += ch;
				}
			}
			if (g_alias_book.find(clazz) != g_alias_book.end()) {
				name += "::_";
			}
			return name;
		}
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_BYTES:
			return "protocache::Slice<uint8_t>";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_STRING:
			return "protocache::Slice<char>";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_DOUBLE:
			return "double";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FLOAT:
			return "float";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT64:
			return "uint64_t";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT32:
			return "uint32_t";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT64:
			return "int64_t";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT32:
			return "int32_t";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_BOOL:
			return "bool";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_ENUM:
			return "protocache::EnumValue";
		default:
			return {};
	}
}

static const char* TypeMark(::google::protobuf::FieldDescriptorProto_Type type) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_BYTES:
			return "bytes";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_STRING:
			return "str";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_DOUBLE:
			return "f64";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FLOAT:
			return "f32";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT64:
			return "u64";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT32:
			return "u32";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT64:
			return "i64";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT32:
			return "i32";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_BOOL:
			return "bool";
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_ENUM:
			return "enum";
		default:
			return nullptr;
	}
}

static bool CanBeKey(::google::protobuf::FieldDescriptorProto_Type type) {
	switch (type) {
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_STRING:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT64:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT32:
		case ::google::protobuf::FieldDescriptorProto_Type_TYPE_ENUM:
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
		oss << '\t' << value.name() << " = " << value.number();
		if (i != n-1) {
			oss << ',';
		}
		oss << '\n';
	}
	oss << "};\n\n";
	return oss.str();
}

static std::string GenMessage(const std::string& ns, const ::google::protobuf::DescriptorProto& proto, std::vector<std::string>& book) {
	std::string fullname = ns;
	fullname += '.';
	fullname += proto.name();
	book.push_back(fullname);

	std::ostringstream oss;
	std::unordered_map<std::string, const ::google::protobuf::DescriptorProto*> map_entries;

	auto alias_mode = proto.field_size() == 1 && proto.field(0).name() == "_";

	if (alias_mode) {
		oss << "struct " << proto.name() << " {\n";
	} else {
		oss << "class " << proto.name() << " final {\n"
			<< "private:\n"
			<< "\tprotocache::Message core_;\n"
			<< "public:\n"
			<< "\tstruct _ {\n";
		for (auto& one : proto.field()) {
			if (one.name() == "_") {
				std::cerr << "found illegal field in message " << fullname << std::endl;
				return {};
			}
			oss << "\t\tstatic constexpr unsigned " << one.name() << " = " << (one.number()-1) << ";\n";
		}
		oss << "\t};\n\n";
	}

	for (auto& one : proto.enum_type()) {
		oss << AddIndent(GenEnum(one));
	}
	for (auto& one : proto.nested_type()) {
		if (one.options().map_entry()) {
			map_entries.emplace(fullname + '.' + one.name(), &one);
			continue;
		}
		auto piece = GenMessage(fullname, one, book);
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
		if (it->second.key_type == -1) {
			oss << "\tusing _ = protocache::ArrayT<" << value << ">;\n";
		} else {
			if (!CanBeKey(it->second.key_type)) {
				std::cerr << "illegal key type: " << it->second.key_type << std::endl;
				return {};
			}
			auto key = TypeName(it->second.key_type, {});
			oss << "\tusing _ = protocache::MapT<" << key << ',' << value << ">;\n";
		}
		oss << "};\n\n";
		return oss.str();
	}

	oss << "\texplicit " << proto.name() << "(const protocache::Message& message) : core_(message) {}\n"
		<< "\texplicit " << proto.name() << "(const uint32_t* ptr) : core_(ptr) {}\n"
		<< "\tbool operator!() const noexcept { return !core_; }\n\n";

	for (auto& one : proto.field()) {
		auto repeated = one.label() == ::google::protobuf::FieldDescriptorProto_Label_LABEL_REPEATED;
		switch (one.type()) {
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_MESSAGE:
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
					auto it = g_alias_book.find(one.type_name());
					if (it == g_alias_book.end()) {
						oss << '\t' << name << ' ' << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
							<< "\t\treturn " << name << "(protocache::GetMessage(core_, _::" << one.name() << ", end));\n"
							<< "\t}\n";
					} else {
						oss << '\t' << name << ' ' << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
							<< "\t\treturn " << name << "(protocache::" << (it->second.key_type != -1? "Map" : "Array")
								<< "(core_.GetField(_::" << one.name() << ", end).GetObject(end), end));\n"
							<< "\t}\n";
					}
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_BYTES:
				if (repeated) {
					oss << "\tprotocache::ArrayT<protocache::Slice<uint8_t>> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetArray<protocache::Slice<uint8_t>>(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tprotocache::Slice<uint8_t> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetBytes(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_STRING:
				if (repeated) {
					oss << "\tprotocache::ArrayT<protocache::Slice<char>> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetArray<protocache::Slice<char>>(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tprotocache::Slice<char> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetString(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_DOUBLE:
				if (repeated) {
					oss << "\tprotocache::Slice<double> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetDoubleArray(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tdouble " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetDouble(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FLOAT:
				if (repeated) {
					oss << "\tprotocache::Slice<float> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetFloatArray(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tfloat " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetFloat(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED64:
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT64:
				if (repeated) {
					oss << "\tprotocache::Slice<uint64_t> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetUInt64Array(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tuint64_t " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetUInt64(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_FIXED32:
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_UINT32:
				if (repeated) {
					oss << "\tprotocache::Slice<uint32_t> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetUInt32Array(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tuint32_t " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetUInt32(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED64:
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT64:
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT64:
				if (repeated) {
					oss << "\tprotocache::Slice<int64_t> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetInt64Array(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tint64_t " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetInt64(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SFIXED32:
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_SINT32:
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_INT32:
				if (repeated) {
					oss << "\tprotocache::Slice<int32_t> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetInt32Array(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tint32_t " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetInt32(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_BOOL:
				if (repeated) {
					oss << "\tprotocache::Slice<bool> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetBoolArray(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tbool " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetBool(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			case ::google::protobuf::FieldDescriptorProto_Type_TYPE_ENUM:
				if (repeated) {
					oss << "\tprotocache::Slice<protocache::EnumValue> " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetEnumValueArray(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				} else {
					oss << "\tprotocache::EnumValue " << one.name() << "(const uint32_t* end=nullptr) const noexcept {\n"
						<< "\t\treturn protocache::GetEnumValue(core_, _::" << one.name() << ", end);\n"
						<< "\t}\n";
				}
				break;
			default:
				std::cerr << "unsupported field " << one.name() << " in message " << proto.name() << std::endl;
				return {};
		}
	}

	oss << "};\n\n";
	return oss.str();
}

static std::string GenFile(const ::google::protobuf::FileDescriptorProto& proto) {
	std::ostringstream oss;
	std::string header_name = proto.name();
	for (auto& ch : header_name) {
		if (!std::isalnum(ch)) {
			ch = '_';
		}
	}
	oss << "#pragma once\n"
		<< "#ifndef PROTOCACHE_INCLUDED_" << header_name << '\n'
		<< "#define PROTOCACHE_INCLUDED_" << header_name << '\n';

	oss << '\n' << "#include \"access.h\"\n";
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
	for (auto& ns : ns_parts) {
		oss << "namespace " << ns << " {\n";
	}
	oss << '\n';

	std::vector<std::string> book;

	for (auto& one : proto.enum_type()) {
		oss << GenEnum(one);
	}
	for (auto& one : proto.message_type()) {
		auto piece = GenMessage(ns, one, book);
		if (piece.empty()) {
			std::cerr << "fail to gen code for message: " << one.name() << std::endl;
			return {};
		}
		oss << piece;
	}
	for (auto it = ns_parts.rbegin(); it != ns_parts.rend(); ++it) {
		oss << "} // " << *it << '\n';
	}

	oss << '\n';

	for (auto& one : book) {
		auto fullname = TypeName(::google::protobuf::FieldDescriptorProto_Type_TYPE_MESSAGE, one);
		auto it = g_alias_book.find(one);
		if (it == g_alias_book.end()) {
			oss << "template<>\n"
				<< "inline " << fullname << " protocache::FieldT<" << fullname << ">::Get(const uint32_t *end) const noexcept {\n"
				<< "\treturn " << fullname << "(core_.GetObject(end));\n"
				<< "}\n\n";
			continue;
		}
		std::string mark;
		while (true) {
			if (it->second.key_type == -1) {
				mark += 'V';
			} else {
				mark += 'M';
				auto tm = TypeMark(it->second.key_type);
				if (tm == nullptr) {
					return {};
				}
				mark += tm;
			}
			if (it->second.value_type != ::google::protobuf::FieldDescriptorProto_Type_TYPE_MESSAGE) {
				auto tm = TypeMark(it->second.value_type);
				if (tm == nullptr) {
					return {};
				}
				mark += tm;
				break;
			}
			auto& value_class = it->second.value_class;
			it = g_alias_book.find(value_class);
			if (it == g_alias_book.end()) {
				auto tmp = value_class;
				tmp[0] = 'X';
				for (auto& ch : tmp) {
					if (ch == '.') {
						ch = '_';
					}
				}
				mark += tmp;
				break;
			}
		}
		oss << "#ifndef PROTOCACHE_ALIAS_" << mark << '\n'
			<< "#define PROTOCACHE_ALIAS_" << mark << '\n'
			<< "template<>\n"
			<< "inline " << fullname << " protocache::FieldT<" << fullname << ">::Get(const uint32_t *end) const noexcept {\n"
			<< "\treturn " << fullname << "(protocache::" << (mark.front() == 'M'? "Map": "Array") << "(core_.GetObject(end)));\n"
			<< "}\n"
			<< "#endif // PROTOCACHE_ALIAS_" << mark << "\n\n";
	}

	oss << "#endif // PROTOCACHE_INCLUDED_" << header_name << '\n';
	return oss.str();
}

int main() {
	::google::protobuf::Arena arena;
	auto request = ::google::protobuf::Arena::CreateMessage<::google::protobuf::compiler::CodeGeneratorRequest>(&arena);
	auto response = ::google::protobuf::Arena::CreateMessage<::google::protobuf::compiler::CodeGeneratorResponse>(&arena);

	if (!request->ParseFromFileDescriptor(STDIN_FILENO)) {
		std::cerr << "fail to get request" << std::endl;
		return 1;
	}

	for (auto& proto : request->proto_file()) {
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
	files.reserve(request->file_to_generate_size());
	for (auto& one : request->file_to_generate()) {
		files.insert(one);
	}

	for (auto& proto : request->proto_file()) {
		if (files.find(proto.name()) == files.end()) {
			continue;
		}
		auto one = response->add_file();
		one->set_name(ConvertFilename(proto.name()));
		one->set_content(GenFile(proto));
		if (one->content().empty()) {
			std::cerr << "fail to generate code for file: " << proto.name() << std::endl;
			return -2;
		}
	}

	if (!response->SerializeToFileDescriptor(STDOUT_FILENO)) {
		std::cerr << "fail to response" << std::endl;
		return 2;
	}
	return 0;
}