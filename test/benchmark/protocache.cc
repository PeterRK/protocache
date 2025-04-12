// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include "protocache/extension/utils.h"
#include "protocache/extension/reflection.h"
#include "test.pc.h"
#include "test.pc-ex.h"
#include "twitter.pc-ex.h"
#include "common.h"

static inline uint32_t JunkHash(const protocache::Slice<char>& data) {
	return JunkHash(data.data(), data.size());
}

static inline uint32_t JunkHash(const protocache::Slice<uint8_t>& data) {
	return JunkHash(data.data(), data.size());
}

struct Junk2 : public Junk {
	void Traverse(const ::test::Small& root);
	void Traverse(const ::test::Main& root);

	void Access(const protocache::reflection::Field& descriptor, protocache::Field field);
	void Traverse(const protocache::reflection::Field& descriptor, protocache::Field field);
	void Traverse(const protocache::reflection::Descriptor& descriptor, protocache::Message root);

	void Traverse(::ex::test::Small& root);
	void Traverse(::ex::test::Main& root);
};

inline void Junk2::Traverse(const ::test::Small& root) {
	u32 += root.i32() + root.flag();
	u32 += JunkHash(root.str());
}

void Junk2::Traverse(const ::test::Main& root) {
	u32 += root.i32() + root.u32() + root.flag() + root.mode();
	u32 += root.t_i32() + root.t_s32() + root.t_u32();
	for (auto v : root.i32v()) {
		u32 += v;
	}
	for (auto v : root.flags()) {
		u32 += v;
	}
	u32 += JunkHash(root.str());
	u32 += JunkHash(root.data());
	for (auto v : root.strv()) {
		u32 += JunkHash(v);
	}
	for (auto v : root.datav()) {
		u32 += JunkHash(v);
	}

	u64 += root.i64() + root.u64();
	u64 += root.t_i64() + root.t_s64() + root.t_u64();
	for (auto v : root.u64v()) {
		u64 += v;
	}

	f32 += root.f32();
	for (auto v : root.f32v()) {
		f32 += v;
	}

	f64 += root.f64();
	for (auto v : root.f64v()) {
		f64 += v;
	}

	Traverse(*root.object());
	for (auto v : root.objectv()) {
		Traverse(*v);
	}

	for (auto p : root.index()) {
		u32 += JunkHash(p.Key()) + p.Value();
	}

	for (auto p : root.objects()) {
		u32 += p.Key();
		Traverse(*p.Value());
	}

	for (auto u : root.matrix()) {
		for (auto v : u) {
			f32 += v;
		}
	}
	for (auto u : root.vector()) {
		for (auto p : u) {
			u32 += JunkHash(p.Key());
			for (auto v : p.Value()) {
				f32 += v;
			}
		}
	}
	for (auto p : root.arrays()) {
		u32 += JunkHash(p.Key());
		for (auto v : p.Value()) {
			f32 += v;
		}
	}
}

void Junk2::Access(const protocache::reflection::Field& descriptor, protocache::Field field) {
	switch (descriptor.value) {
		case protocache::reflection::Field::TYPE_MESSAGE:
		{
			assert(descriptor.value_descriptor != nullptr);
			if (descriptor.value_descriptor->IsAlias()) {
				Traverse(descriptor.value_descriptor->alias, field);
			} else {
				Traverse(*descriptor.value_descriptor, protocache::Message(field.GetObject()));
			}
		}
			break;
		case protocache::reflection::Field::TYPE_BYTES:
			u32 += JunkHash(protocache::FieldT<protocache::Slice<uint8_t>>(field).Get());
			break;
		case protocache::reflection::Field::TYPE_STRING:
			u32 += JunkHash(protocache::FieldT<protocache::Slice<char>>(field).Get());
			break;
		case protocache::reflection::Field::TYPE_DOUBLE:
			f64 += protocache::FieldT<double>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_FLOAT:
			f32 += protocache::FieldT<float>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_UINT64:
			u64 += protocache::FieldT<uint64_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_UINT32:
			u32 += protocache::FieldT<uint32_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_INT64:
			u64 += protocache::FieldT<int64_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_INT32:
			u32 += protocache::FieldT<int32_t>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_BOOL:
			u32 += protocache::FieldT<bool>(field).Get();
			break;
		case protocache::reflection::Field::TYPE_ENUM:
			u32 += protocache::FieldT<protocache::EnumValue>(field).Get();
			break;
		default:
			break;
	}
}

void Junk2::Traverse(const protocache::reflection::Field& descriptor, protocache::Field field) {
	if (descriptor.IsMap()) {
		for (auto p : protocache::Map(field.GetObject())) {
			switch (descriptor.key) {
				case protocache::reflection::Field::TYPE_STRING:
					u32 += JunkHash(protocache::FieldT<protocache::Slice<char>>(p.Key()).Get());
					break;
				case protocache::reflection::Field::TYPE_UINT64:
					u32 += protocache::FieldT<uint64_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_UINT32:
					u32 += protocache::FieldT<uint32_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_INT64:
					u32 += protocache::FieldT<int64_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_INT32:
					u32 += protocache::FieldT<int32_t>(p.Key()).Get();
					break;
				case protocache::reflection::Field::TYPE_BOOL:
					u32 += protocache::FieldT<bool>(p.Key()).Get();
					break;
				default:
					break;
			}
			Access(descriptor, p.Value());
		}
		return;
	}
	if (descriptor.repeated) {
		switch (descriptor.value) {
			case protocache::reflection::Field::TYPE_MESSAGE:
			{
				assert(descriptor.value_descriptor != nullptr);
				if (descriptor.value_descriptor->IsAlias()) {
					for (auto v : protocache::Array(field.GetObject())) {
						Traverse(descriptor.value_descriptor->alias, v);
					}
				} else {
					for (auto v : protocache::ArrayT<protocache::Message>(field.GetObject())) {
						Traverse(*descriptor.value_descriptor, v);
					}
				}
			}
				break;
			case protocache::reflection::Field::TYPE_BYTES:
				for (auto u : protocache::Array(field.GetObject())) {
					u32 += JunkHash(protocache::FieldT<protocache::Slice<uint8_t>>(u).Get());
				}
				break;
			case protocache::reflection::Field::TYPE_STRING:
				for (auto u : protocache::Array(field.GetObject())) {
					u32 += JunkHash(protocache::FieldT<protocache::Slice<char>>(u).Get());
				}
				break;
			case protocache::reflection::Field::TYPE_DOUBLE:
				for (auto v : protocache::ArrayT<double>(field.GetObject())) {
					f64 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_FLOAT:
				for (auto v : protocache::ArrayT<float>(field.GetObject())) {
					f32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_UINT64:
				for (auto v : protocache::ArrayT<uint64_t>(field.GetObject())) {
					u64 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_UINT32:
				for (auto v : protocache::ArrayT<uint32_t>(field.GetObject())) {
					u32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_INT64:
				for (auto v : protocache::ArrayT<int64_t>(field.GetObject())) {
					u64 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_INT32:
				for (auto v : protocache::ArrayT<int32_t>(field.GetObject())) {
					u32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_BOOL:
				for (auto v : protocache::ArrayT<bool>(field.GetObject())) {
					u32 += v;
				}
				break;
			case protocache::reflection::Field::TYPE_ENUM:
				for (auto v : protocache::ArrayT<protocache::EnumValue>(field.GetObject())) {
					u32 += v;
				}
				break;
			default:
				break;
		}

	} else {
		Access(descriptor, field);
	}
}

void Junk2::Traverse(const protocache::reflection::Descriptor& descriptor, protocache::Message root) {
	for (auto& p : descriptor.fields) {
		auto field = root.GetField(p.second.id);
		if (!field) {
			continue;
		}
		Traverse(p.second, field);
	}
}

void Junk2::Traverse(::ex::test::Small& root) {
	u32 += root.i32() + root.flag();
	u32 += JunkHash(root.str());
}

void Junk2::Traverse(::ex::test::Main& root) {
	u32 += root.i32() + root.u32() + root.flag() + root.mode();
	u32 += root.t_i32() + root.t_s32() + root.t_u32();
	for (auto v : root.i32v()) {
		u32 += v;
	}
	for (auto v : root.flags()) {
		u32 += v;
	}
	u32 += JunkHash(root.str());
	u32 += JunkHash(root.data());
	for (auto& v : root.strv()) {
		u32 += JunkHash(v);
	}
	for (auto& v : root.datav()) {
		u32 += JunkHash(v);
	}

	u64 += root.i64() + root.u64();
	u64 += root.t_i64() + root.t_s64() + root.t_u64();
	for (auto v : root.u64v()) {
		u64 += v;
	}

	f32 += root.f32();
	for (auto v : root.f32v()) {
		f32 += v;
	}

	f64 += root.f64();
	for (auto v : root.f64v()) {
		f64 += v;
	}

	Traverse(*root.object());
	for (auto& v : root.objectv()) {
		Traverse(*v);
	}

	for (auto& p : root.index()) {
		u32 += JunkHash(p.first) + p.second;
	}

	for (auto& p : root.objects()) {
		u32 += p.first;
		Traverse(*p.second);
	}

	for (auto& u : root.matrix()) {
		for (auto v : u) {
			f32 += v;
		}
	}
	for (auto& u : root.vector()) {
		for (auto& p : u) {
			u32 += JunkHash(p.first);
			for (auto v : p.second) {
				f32 += v;
			}
		}
	}
	for (auto& p : root.arrays()) {
		u32 += JunkHash(p.first);
		for (auto v : p.second) {
			f32 += v;
		}
	}
}

int BenchmarkProtoCache() {
	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}
	Junk2 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		auto& root = *protocache::Message(reinterpret_cast<const uint32_t*>(raw.data())).Cast<::test::Main>();
		junk.Traverse(root);
	}
	auto delta_ms = DeltaMs(start);

	printf("protocache: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkProtoCacheReflect() {
	std::string err;
	google::protobuf::FileDescriptorProto file;
	if(!protocache::ParseProtoFile("test.proto", &file, &err)) {
		puts("fail to load test.proto");
		return -1;
	}
	protocache::reflection::DescriptorPool pool;
	if (!pool.Register(file)) {
		puts("fail to prepare descriptor pool");
		return -2;
	}
	auto descriptor = pool.Find("test.Main");
	if (descriptor == nullptr) {
		puts("fail to get entry descriptor");
		return -2;
	}

	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}

	Junk2 junk;
	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		protocache::Message root(reinterpret_cast<const uint32_t*>(raw.data()));
		junk.Traverse(*descriptor, root);
	}
	auto delta_ms = DeltaMs(start);

	printf("protocache-reflect: %ldms %016lx\n", delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkProtoCacheEX() {
	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}
	Junk2 junk;

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		::ex::test::Main root(reinterpret_cast<const uint32_t*>(raw.data()));
		junk.Traverse(root);
	}
	auto delta_ms = DeltaMs(start);

	printf("protocache-ex: %luB %ldms %016lx\n", raw.size(), delta_ms, junk.Fuse());
	return 0;
}

int BenchmarkProtoCacheSerialize(bool partly) {
	std::string raw;
	if (!protocache::LoadFile("test.pc", &raw)) {
		puts("fail to load test.pc");
		return -1;
	}
	::ex::test::Main root(reinterpret_cast<const uint32_t*>(raw.data()));

	if (partly) {
		root.i32();
		root.u32();
		root.i64();
		root.u64();
		root.flag();
		root.mode();
		root.str();
		root.data();
		root.f32();
		root.f64();
		//root.object();
		//root.objectv();
		//root.objects();
	} else {
		Junk2 junk;
		junk.Traverse(root);
	}

	unsigned cnt = 0;
	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		protocache::Data data;
		root.Serialize(&data);
		cnt += data.size();
	}
	auto delta_ms = DeltaMs(start);

	if (partly) {
		printf("protocache-partly: %ldms %x\n", delta_ms, cnt);
	} else {
		printf("protocache-fully: %ldms %x\n", delta_ms, cnt);
	}
	return 0;
}

int BenchmarkCompress(const char* name, const std::string& filepath) {
	std::string raw;
	if (!protocache::LoadFile(filepath, &raw)) {
		printf("fail to load %s\n", filepath.c_str());
		return -1;
	}
	std::string cooked;
	cooked.reserve(raw.size());

	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		protocache::Compress(raw, &cooked);
	}
	auto delta_ms = DeltaMs(start);
	printf("%s-compress: %luB %ldms\n", name, cooked.size(), delta_ms);

	start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kLoop; i++) {
		if (!protocache::Decompress(cooked, &raw)) {
			puts("fail to decompress");
			return 1;
		}
	}
	delta_ms = DeltaMs(start);
	printf("%s-decompress: %ldms\n", name, delta_ms);
	return 0;
}

static void Touch(::ex::twitter::Url& url) {
	url.url();
	for (auto& u : url.urls()) {
		Touch(*u);
	}
	url.expanded_url();
	url.display_url();
	url.indices();
}

static void Touch(::ex::twitter::Entities& entries) {
	Touch(*entries.url());
	for (auto& u : entries.urls()) {
		Touch(*u);
	}
	for (auto& m : entries.user_mentions()) {
		m->screen_name();
		m->name();
		m->id();
		m->id_str();
		m->indices();
	}
	for (auto& u : entries.description()->urls()) {
		Touch(*u);
	}
	for (auto& m : entries.media()) {
		m->id();
		m->id_str();
		m->indices();
		m->media_url();
		m->media_url_https();
		m->url();
		m->display_url();;
		m->expanded_url();
		m->type();
		for (auto& p : m->sizes()) {
			p.second->w();
			p.second->h();
			p.second->resize();
		}
		m->source_status_id();
		m->source_status_id_str();;
	}
}

static void Touch(::ex::twitter::Status& status) {
	status.metadata()->result_type();
	status.metadata()->iso_language_code();
	status.created_at();
	status.id();
	status.id_str();
	status.text();
	status.source();
	status.truncated();
	status.in_reply_to_status_id();
	status.in_reply_to_status_id_str();
	status.in_reply_to_user_id();
	status.in_reply_to_user_id_str();
	status.in_reply_to_screen_name();

	status.user()->id();
	status.user()->id_str();
	status.user()->name();
	status.user()->screen_name();
	status.user()->location();
	status.user()->description();
	Touch(*status.user()->entities());

	status.user()->followers_count();
	status.user()->friends_count();
	status.user()->listed_count();
	status.user()->created_at();
	status.user()->favourites_count();
	status.user()->utc_offset();
	status.user()->time_zone();
	status.user()->geo_enabled();
	status.user()->verified();
	status.user()->statuses_count();
	status.user()->lang();
	status.user()->contributors_enabled();
	status.user()->is_translator();
	status.user()->is_translation_enabled();
	status.user()->profile_background_color();
	status.user()->profile_background_image_url();
	status.user()->profile_background_image_url_https();
	status.user()->profile_background_tile();
	status.user()->profile_image_url();
	status.user()->profile_image_url_https();
	status.user()->profile_banner_url();
	status.user()->profile_link_color();
	status.user()->profile_sidebar_border_color();
	status.user()->profile_sidebar_fill_color();
	status.user()->profile_text_color();
	status.user()->profile_use_background_image();
	status.user()->default_profile();
	status.user()->default_profile_image();
	status.user()->following();
	status.user()->follow_request_sent();
	status.user()->notifications();
	if (status.HasField(::twitter::Status::_::retweeted_status)) {
		Touch(*status.retweeted_status());
	}
	status.retweet_count();
	status.favorite_count();
	for (auto& entries : status.entities()) {
		Touch(*entries);
	}
	status.favorited();
	status.retweeted();
	status.possibly_sensitive();
	status.lang();
}

static void Touch(::ex::twitter::Root::SearchMetadata& meta) {
	meta.completed_in();
	meta.max_id();
	meta.max_id_str();
	meta.next_results();
	meta.query();
	meta.refresh_url();
	meta.count();
	meta.since_id();
	meta.since_id_str();
}

int BenchmarkTwitterSerializePC() {
	std::string raw;
	if (!protocache::LoadFile("twitter.pc", &raw)) {
		puts("fail to load twitter.pc");
		return -1;
	}
	::ex::twitter::Root root(reinterpret_cast<const uint32_t*>(raw.data()));

	for (auto& status : root.statuses()) {
		Touch(*status);
	}
	Touch(*root.search_metadata());

	unsigned cnt = 0;
	auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < kSmallLoop; i++) {
		protocache::Data data;
		root.Serialize(&data);
		cnt += data.size();
	}
	auto delta_ms = DeltaMs(start);

	printf("pcex-twitter: %ldms %x\n", delta_ms, cnt);
	return 0;
}