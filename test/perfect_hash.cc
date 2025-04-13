// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <gtest/gtest.h>
#include <protocache/perfect_hash.h>

class StrKeyReader : public protocache::KeyReader {
public:
	explicit StrKeyReader(const std::vector<std::string>& keys) : keys_(keys) {}

	void Reset() {
		idx_ = 0;
	}
	size_t Total() {
		return keys_.size();
	}
	protocache::Slice<uint8_t> Read() {
		if (idx_ >= keys_.size()) {
			return {nullptr, 0};
		}
		auto& key = keys_[idx_++];
		return {reinterpret_cast<const uint8_t*>(key.data()), key.size()};
	}

private:
	const std::vector<std::string>& keys_;
	size_t idx_ = 0;
};

static void DoTest(unsigned size) {
	std::vector<std::string> keys(size);
	for (unsigned i = 0; i < keys.size(); i++) {
		keys[i] = std::to_string(i);
	}
	StrKeyReader reader(keys);

	auto table = protocache::PerfectHashObject::Build(reader);
	ASSERT_FALSE(!table);

	std::vector<bool> mark(keys.size());
	for (auto& key : keys) {
		auto pos = table.Locate(reinterpret_cast<const uint8_t *>(key.data()), key.size());
		ASSERT_LT(pos, keys.size());
		ASSERT_FALSE(mark[pos]);
		mark[pos] = true;
	}
}

TEST(PerfectHash, Tiny) {
	DoTest(0);
	DoTest(1);
	DoTest(2);
	DoTest(24);
}

TEST(PerfectHash, Small) {
	DoTest(200);
	DoTest(1000);
}

TEST(PerfectHash, Big) {
	DoTest(100000);
}
