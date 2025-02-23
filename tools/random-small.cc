#include <cstdint>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include "random-small.pc-ex.h"


int main() {
	std::mt19937 engine(999);
	std::uniform_int_distribution<char> char_dist;
	std::normal_distribution<double> num_dist(0, 1000);
	std::normal_distribution<float> len_dist(0, 10);

	::ex::Dummy dummy;
	std::string tmp_str;
	char path[32];

	for (unsigned i = 0; i < 1000; i++) {
		auto len = static_cast<unsigned>(std::abs(len_dist(engine)));
		dummy.ivec.resize(len);
		for (unsigned j = 0; j < len; j++) {
			dummy.ivec[j] = static_cast<int32_t>(num_dist(engine));
		}
		auto sz = static_cast<unsigned>(std::abs(len_dist(engine)));
		dummy.imap.clear();
		for (unsigned k = 0; k < sz; k++) {
			len = static_cast<unsigned>(std::abs(len_dist(engine)));
			tmp_str.resize(len);
			for (unsigned j = 0; j < len; j++) {
				tmp_str[j] = char_dist(engine);
			}
			dummy.imap.emplace(tmp_str, static_cast<int64_t>(num_dist(engine)));
		}
		auto data = dummy.Serialize();
		sprintf(path, "%u.bin", i);
		std::ofstream ofs(path);
		if (!ofs.write(reinterpret_cast<const char*>(data.data()), data.size()*sizeof(uint32_t))) {
			return -1;
		}
	}

	return 0;
}