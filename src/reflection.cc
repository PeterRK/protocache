// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "protocache/reflection.h"

namespace protocache {
namespace reflection {

static inline std::string Fullname(const std::string& ns, const std::string& name) {
	if (ns.empty()) {
		return name;
	}
	std::string fullname;
	fullname.reserve(ns.size()+1+name.size());
	fullname = ns;
	fullname += '.';
	fullname += name;
	return fullname;
}

static inline Field::Type ConvertType(const google::protobuf::FieldDescriptorProto& field) noexcept {
	if (!field.has_type()) {
		return Field::TYPE_UNKNOWN;
	}
	switch (field.type()) {
		case google::protobuf::FieldDescriptorProto::TYPE_MESSAGE:
			return Field::TYPE_MESSAGE;
		case google::protobuf::FieldDescriptorProto::TYPE_BYTES:
			return Field::TYPE_BYTES;
		case google::protobuf::FieldDescriptorProto::TYPE_STRING:
			return Field::TYPE_STRING;
		case google::protobuf::FieldDescriptorProto::TYPE_DOUBLE:
			return Field::TYPE_DOUBLE;
		case google::protobuf::FieldDescriptorProto::TYPE_FLOAT:
			return Field::TYPE_FLOAT;
		case google::protobuf::FieldDescriptorProto::TYPE_FIXED64:
		case google::protobuf::FieldDescriptorProto::TYPE_UINT64:
			return Field::TYPE_UINT64;
		case google::protobuf::FieldDescriptorProto::TYPE_FIXED32:
		case google::protobuf::FieldDescriptorProto::TYPE_UINT32:
			return Field::TYPE_UINT32;
		case google::protobuf::FieldDescriptorProto::TYPE_SFIXED64:
		case google::protobuf::FieldDescriptorProto::TYPE_SINT64:
		case google::protobuf::FieldDescriptorProto::TYPE_INT64:
			return Field::TYPE_INT64;
		case google::protobuf::FieldDescriptorProto::TYPE_SFIXED32:
		case google::protobuf::FieldDescriptorProto::TYPE_SINT32:
		case google::protobuf::FieldDescriptorProto::TYPE_INT32:
			return Field::TYPE_INT32;
		case google::protobuf::FieldDescriptorProto::TYPE_BOOL:
			return Field::TYPE_BOOL;
		case google::protobuf::FieldDescriptorProto::TYPE_ENUM:
			return Field::TYPE_ENUM;
		default:
			return Field::TYPE_NONE;
	}
}

static inline bool CanBeKey(Field::Type type) noexcept {
	switch (type) {
		case Field::TYPE_STRING:
		case Field::TYPE_UINT64:
		case Field::TYPE_UINT32:
		case Field::TYPE_INT64:
		case Field::TYPE_INT32:
		case Field::TYPE_BOOL:
			return true;
		default:
			return false;
	}
}

bool DescriptorPool::FixUnknownType(const std::string& fullname, Descriptor& descriptor) const {
	auto bind_type = [this](const std::string& name, Field& field)->bool {
		if (enum_.find(name) != enum_.end()) {
			field.value = Field::TYPE_ENUM;
			field.value_type.clear();
			return true;
		}
		if (pool_.find(name) != pool_.end()) {
			field.value = Field::TYPE_MESSAGE;
			field.value_type = name;
			return true;
		}
		return false;
	};

	auto check_type = [this, &fullname, &bind_type](Field& field)->bool {
		if (field.value != Field::TYPE_UNKNOWN) {
			return true;
		}
		if (field.value_type.empty()) {
			return false;
		}
		if (bind_type(field.value_type, field)) {
			return true;
		}
		auto name = Fullname(fullname, field.value_type);
		if (bind_type(name, field)) {
			return true;
		}
		name.resize(fullname.size());
		while (true) {
			auto pos = name.rfind('.');
			if (pos == std::string::npos) {
				break;
			}
			name.resize(pos+1);
			name += field.value_type;
			if (bind_type(name, field)) {
				return true;
			}
			name.resize(pos);
		}
		return false;
	};

	if (descriptor.IsAlias()) {
		if (!check_type(descriptor.alias)) {
			return false;
		}
	} else {
		for (auto& p : descriptor.fields) {
			if (!check_type(p.second)) {
				return false;
			}
		}
	}
	return true;
}

const Descriptor* DescriptorPool::Find(const std::string& fullname) noexcept {
	auto it = pool_.find(fullname);
	if (it == pool_.end()) {
		return nullptr;
	}
	auto descriptor = &it->second;
	if (descriptor->alias.id == 0) {
		return descriptor;
	} else if (descriptor->alias.id != UINT_MAX) {
		return nullptr;
	}
	descriptor->alias.id--;
	if (!FixUnknownType(fullname, *descriptor)) {
		return nullptr;
	}
	descriptor->alias.id = 0;
	return descriptor;
}

bool DescriptorPool::Register(const std::string& ns, const google::protobuf::DescriptorProto& proto) {
	auto fullname = Fullname(ns, proto.name());
	for (auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			enum_.insert(Fullname(fullname, one.name()));
		}
	}
	std::unordered_map<std::string, const google::protobuf::DescriptorProto*> map_entries;
	for (auto& one : proto.nested_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (one.options().map_entry()) {
			map_entries.emplace(one.name(), &one);
			continue;
		}
		if (!Register(fullname, one)) {
			return false;
		}
	}

	auto convert_field = [&map_entries](const google::protobuf::FieldDescriptorProto& src, Field& out)->bool {
		out.repeated = src.label() == google::protobuf::FieldDescriptorProto::LABEL_REPEATED;
		out.value = ConvertType(src);
		if (out.value == Field::TYPE_NONE) {
			return false;
		}
		if (out.value == Field::TYPE_MESSAGE || out.value == Field::TYPE_UNKNOWN) {
			auto it = map_entries.find(src.type_name());
			if (it != map_entries.end()) {
				out.key = ConvertType(it->second->field(0));
				out.value = ConvertType(it->second->field(1));
				if (!CanBeKey(out.key) || out.value == Field::TYPE_NONE) {
					return false;
				}
				out.value_type = it->second->field(1).type_name();
			} else {
				out.value_type = src.type_name();
			}
		}
		return true;
	};

	Descriptor descriptor;
	descriptor.alias.id = UINT_MAX;
	if (proto.field_size() == 1 && proto.field(0).name() == "_") {
		convert_field(proto.field(0), descriptor.alias);
	} else {
		descriptor.fields.reserve(proto.field_size());
		for (auto& one : proto.field()) {
			if (one.options().deprecated()) {
				continue;
			}
			auto& field = descriptor.fields[one.name()];
			if (one.number() <= 0) {
				return false;
			}
			field.id = one.number() - 1;
			convert_field(one, field);
		}
	}
	return pool_.emplace(std::move(fullname), std::move(descriptor)).second;
}

bool DescriptorPool::Register(const google::protobuf::FileDescriptorProto& proto) {
	for (auto& one : proto.enum_type()) {
		if (!one.options().deprecated()) {
			enum_.insert(Fullname(proto.package(), one.name()));
		}
	}
	for (auto& one : proto.message_type()) {
		if (one.options().deprecated()) {
			continue;
		}
		if (!Register(proto.package(), one)) {
			return false;
		}
	}
	for (auto& p : pool_) {
		if (p.second.alias.id != 0 && FixUnknownType(p.first, p.second)) {
			p.second.alias.id = 0;
		}
	}
	return true;
}

} // reflection
} // protocache