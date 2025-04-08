// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <string>
#include <fstream>
#include "protocache/utils.h"

namespace protocache {

bool LoadFile(const std::string& path, std::string* out) {
	std::ifstream ifs(path, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	if (!ifs) {
		return false;
	}
	size_t size = ifs.tellg();
	ifs.seekg(0);
	out->resize(size);
	if (!ifs.read(const_cast<char*>(out->data()), size)) {
		return false;
	}
	return true;
}

void Compress(const uint8_t* src, size_t len, std::string* out) {
	out->clear();
	if (len == 0) {
		return;
	}
	auto n = len;
	while ((n & ~0x7fU) != 0) {
		out->push_back(static_cast<char>(0x80U | (n & 0x7fU)));
		n >>= 7U;
	}
	out->push_back(static_cast<char>(n));

	auto end = src + len;
	auto pick = [&src, end]()->uint8_t {
		uint8_t cnt = 1;
		auto ch = *src++;
		if (ch == 0) {
			while (src < end && cnt < 4 && *src == 0) {
				src++;
				cnt++;
			}
			return 0x8 | (cnt-1);
		} else if (ch == 0xff) {
			while (src < end && cnt < 4 && *src == 0xff) {
				src++;
				cnt++;
			}
			return 0xC | (cnt-1);
		} else {
			while (src < end && cnt < 7 && *src != 0 && *src != 0xff) {
				src++;
				cnt++;
			}
			return cnt;
		}
	};

	unsigned off = out->size();
	out->resize(off + len + (len+13)/14);
	auto dest = reinterpret_cast<uint8_t*>(const_cast<char*>(out->data())) + off;

	auto copy = [&dest, end](const uint8_t* s, unsigned l) {
		if (s+8 <= end) {
			*reinterpret_cast<uint64_t*>(dest) = *reinterpret_cast<const uint64_t*>(s);
		} else {
			memcpy(dest, s, l);
		}
		dest += l;
	};

	while (src < end) {
		auto x = src;
		auto a = pick();
		if (src == end) {
			*dest++ = a;
			if ((a & 0x8) == 0) {
				copy(x, a);
			}
			break;
		}
		auto y = src;
		auto b = pick();
		*dest++ = a | (b<<4);
		if ((a & 0x8) == 0) {
			copy(x, a);
		}
		if ((b & 0x8) == 0) {
			copy(y, b);
		}
	}
	out->resize(reinterpret_cast<char*>(dest) - out->data());
}

bool Decompress(const uint8_t* src, size_t len, std::string* out) {
	out->clear();
	if (len == 0) {
		return true;
	}
	auto end = src + len;

	unsigned size = 0;
	for (unsigned sft = 0; sft < 32U; sft += 7U) {
		if (src >= end) {
			return false;
		}
		uint8_t b = *src++;
		if (b & 0x80U) {
			size |= static_cast<unsigned>(b & 0x7fU) << sft;
		} else {
			size |= static_cast<unsigned>(b) << sft;
			break;
		}
	}

	out->resize(size);
	auto dest = reinterpret_cast<uint8_t*>(const_cast<char*>(out->data()));
	auto tail = reinterpret_cast<const uint8_t*>(out->data()) + size;

	auto unpack = [&src, end, &dest, tail](uint8_t mark)->bool {
		if (mark & 8) {
			auto cnt = (mark & 3) + 1;
			if (dest + cnt > tail) {
				return false;
			}
			if (mark & 4) {
				if (dest+4 <= tail) {
					*reinterpret_cast<uint32_t*>(dest) = 0xffffffff;
				} else {
					memset(dest, 0xff, cnt);
				}
			} else {
				if (dest+4 <= tail) {
					*reinterpret_cast<uint32_t*>(dest) = 0;
				} else {
					memset(dest, 0, cnt);
				}
			}
			dest += cnt;
		} else {
			auto l = mark & 7;
			if (src+l > end || dest+l > tail) {
				return false;
			}
			if (src+8 <= end && dest+8 <= tail) {
				*reinterpret_cast<uint64_t*>(dest) = *reinterpret_cast<const uint64_t*>(src);
			} else {
				memcpy(dest, src, l);
			}
			src += l;
			dest += l;
		}
		return true;
	};

	while (src < end) {
		auto mark = *src++;
		if (!unpack(mark & 0xf) || !unpack(mark >> 4)) {
			return false;
		}
	}
	return dest == tail;
}

} // protocache