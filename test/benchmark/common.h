// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#include <cstdint>
#include <chrono>
#include <string>

static inline uint32_t JunkHash(const void* data, size_t len) {
	uint32_t out = 0;
	auto p = reinterpret_cast<const uint32_t*>(data);
	for (; len >= 4; len -= 4) {
		out ^= *p++;
	}
	return out;
}

static inline uint32_t JunkHash(const std::string& data) {
	return JunkHash(data.data(), data.size());
}

struct Junk {
	uint32_t u32 = 0;
	float f32 = 0;
	uint64_t u64 = 0;
	double f64 = 0;

	uint64_t Fuse() {
		union {
			uint64_t u64;
			double f64;
		} t;
		t.f64 = f64 + f32;
		return t.u64 ^ (u64 + u32);
	}
};

using TraceTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
static inline long DeltaMs(TraceTimePoint start, TraceTimePoint finish = std::chrono::steady_clock::now()) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
}

constexpr size_t kLoop = 1000000;
constexpr size_t kSmallLoop = 1000;

extern int BenchmarkProtobuf();
extern int BenchmarkProtobufReflect();
extern int BenchmarkProtoCache();
extern int BenchmarkProtoCacheReflect();
extern int BenchmarkFlatBuffers();
extern int BenchmarkFlatBuffersReflect();
extern int BenchmarkCapnProto(bool packed);
extern int BenchmarkCapnProtoReflect(bool packed);

extern int BenchmarkProtoCacheEX();
extern int BenchmarkProtobufSerialize(bool flat=false);
extern int BenchmarkProtoCacheSerialize(bool partly=false);

extern int BenchmarkCompress(const char* name, const std::string& filepath);

extern int BenchmarkTwitterSerializePB(bool flat=false);
extern int BenchmarkTwitterSerializePC();
