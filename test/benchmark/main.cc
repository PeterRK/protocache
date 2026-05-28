// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <iostream>
#include "common.h"


int main() {
	BenchmarkProtobuf();
	BenchmarkProtobufReflect();
	BenchmarkProtoCache();
	BenchmarkProtoCacheReflect();
	BenchmarkFlatBuffers();
	BenchmarkFlatBuffersReflect();
	BenchmarkCapnProto(false);
	BenchmarkCapnProto(true);
	BenchmarkCapnProtoReflect(false);
	BenchmarkCapnProtoReflect(true);
	BenchmarkFory();

	BenchmarkProtoCacheEX();
	std::cout << "========serialize========" << std::endl;
	BenchmarkProtobufSerialize(false);
	BenchmarkProtobufSerialize(true);
	BenchmarkProtoCacheSerialize(true);
	BenchmarkProtoCacheSerialize(false);

	std::cout << "========compress========" << std::endl;
	BenchmarkCompress("pb", "test.pb");
	BenchmarkCompress("pc", "test.pc");
	BenchmarkCompress("fb", "test.fb");
	BenchmarkCompress("fr", "test.fr");
	return 0;
}