// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_REFLECTION_H_
#define PROTOCACHE_REFLECTION_H_

#include <cstdint>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <google/protobuf/descriptor.pb.h>

namespace protocache {
namespace reflection {

struct Descriptor;

struct Field final {
	enum Type : uint8_t {
		TYPE_NONE = 0,
		TYPE_MESSAGE = 1,
		TYPE_BYTES = 2,
		TYPE_STRING = 3,
		TYPE_DOUBLE = 4,
		TYPE_FLOAT = 5,
		TYPE_UINT64 = 6,
		TYPE_UINT32 = 7,
		TYPE_INT64 = 8,
		TYPE_INT32 = 9,
		TYPE_BOOL = 10,
		TYPE_ENUM = 11,
		TYPE_UNKNOWN = 255,
	};
	unsigned id = UINT_MAX;
	bool repeated = false;
	Type key = TYPE_NONE;
	Type value = TYPE_NONE;
	std::string value_type;
	const Descriptor* value_descriptor = nullptr;
	//std::string default_value;

	bool operator!() const noexcept {
		return value == TYPE_NONE;
	}
	bool IsMap() const noexcept {
		return key != TYPE_NONE;
	}
};

struct Descriptor final {
	Field alias;
	std::unordered_map<std::string, Field> fields;

	bool IsAlias() const noexcept {
		return alias.value != Field::TYPE_NONE;
	}
};

class DescriptorPool final {
public:
	bool Register(const google::protobuf::FileDescriptorProto& proto);

	const Descriptor* Find(const std::string& fullname) noexcept;

private:
	std::unordered_set<std::string> enum_;
	std::unordered_map<std::string, Descriptor> pool_;

	bool Register(const std::string& ns, const google::protobuf::DescriptorProto& proto);
	bool FixUnknownType(const std::string& fullname, Descriptor& descriptor) const;
};

} // reflection
} // protocache
#endif //PROTOCACHE_REFLECTION_H_