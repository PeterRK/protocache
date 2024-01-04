// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once
#ifndef PROTOCACHE_SERIALIZE_H_
#define PROTOCACHE_SERIALIZE_H_

#include <cstdint>
#include <string>
#include <google/protobuf/message.h>

namespace protocache {

using Data = std::basic_string<uint32_t>;

extern Data Serialize(const google::protobuf::Message& message);

} // protocache
#endif //PROTOCACHE_SERIALIZE_H_