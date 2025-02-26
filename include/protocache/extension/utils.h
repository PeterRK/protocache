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

extern Data Serialize(const google::protobuf::Message& message);
extern bool Deserialize(const Slice<uint32_t>& raw, google::protobuf::Message* message);

} // protocache
#endif //PROTOCACHE_EXT_UTILS_H_