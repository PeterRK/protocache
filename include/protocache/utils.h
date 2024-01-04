// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_UTILS_H_
#define PROTOCACHE_UTILS_H_

#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.pb.h>

namespace protocache {

extern bool ParseProtoFile(std::string filename, google::protobuf::FileDescriptorProto* result, std::string* err=nullptr);

extern bool LoadFile(const std::string& path, std::string* out);

extern bool LoadJson(const std::string& path, google::protobuf::Message* message);

} // protocache
#endif //PROTOCACHE_UTILS_H_