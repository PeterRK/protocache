// Copyright (c) 2023, Ruan Kunliang.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <string>
#include <fstream>

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

} // protocache