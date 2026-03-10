// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_EXT_UTILS_H_
#define PROTOCACHE_EXT_UTILS_H_

#include <cstdint>
#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.pb.h>
#include "../utils.h"

namespace protocache {

extern bool ParseProto(const std::string& data, google::protobuf::FileDescriptorProto* result, std::string* err=nullptr);
extern bool ParseProtoFile(const std::string& filename, google::protobuf::FileDescriptorProto* result, std::string* err=nullptr);

extern bool LoadJson(const std::string& path, google::protobuf::Message* message);
extern bool DumpJson(const google::protobuf::Message& message, const std::string& path);

// Serialize protobuf into ProtoCache format.
// Returns false when the schema cannot be represented by ProtoCache, including:
// - empty messages
// - overly sparse field ids
// - duplicate field ids after filtering deprecated fields
// - map key types outside ProtoCache's supported key set
// - alias messages whose only field is not a repeated field named "_"
extern bool Serialize(const google::protobuf::Message& message, Buffer* buf);

// Deserialize ProtoCache data into protobuf.
// The protobuf schema is expected to obey the same ProtoCache schema constraints
// as Serialize().
extern bool Deserialize(const Slice<uint32_t>& raw, google::protobuf::Message* message);

} // protocache
#endif //PROTOCACHE_EXT_UTILS_H_
