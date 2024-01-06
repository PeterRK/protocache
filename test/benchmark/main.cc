// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "common.h"

int main() {
	BenchmarkProtobuf();
	BenchmarkProtobufReflect();
	BenchmarkProtoCache();
	BenchmarkProtoCacheReflect();
	BenchmarkFlatBuffers();
	BenchmarkFlatBuffersReflect();
	return 0;
}