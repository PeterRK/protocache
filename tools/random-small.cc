#include <cstdint>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <algorithm>
#include "random-small.pc-ex.h"


int main() {
	std::mt19937 engine(999);
	std::uniform_int_distribution<unsigned> char_dist;
	std::normal_distribution<double> num_dist(0, 1000);
	std::normal_distribution<float> len_dist(0, 10);

	// from bible
	unsigned book[96] = {
			0,385,0,0,0,0,0,0,286,287,0,0,70457,3185,25964,0,
			5298,30443,21665,12261,9141,7041,6465,5911,5769,5632,40197,14163,0,0,0,3219,
			0,19836,4858,4227,3192,5880,2444,7260,4052,14667,18118,2081,4807,5110,3680,1998,
			5533,13,1876,6972,7496,305,113,2396,0,667,1145,76,0,1,0,0,
			0,279448,47461,55282,151067,432204,81437,51725,295130,194946,2505,25443,128304,85602,234193,254558,
			41323,933,172077,204922,313544,90948,39229,63602,2697,59593,3736,0,0,0,0,0,
	};
	for (unsigned i = 1; i < 96; i++) {
		book[i] += book[i-1];
	}

	::ex::Dummy dummy;
	std::string tmp_str;
	auto random_str = [&tmp_str, &engine, &len_dist, &char_dist, book]()->const std::string& {
		auto len = static_cast<unsigned>(std::abs(len_dist(engine)));
		tmp_str.resize(len);
		for (unsigned j = 0; j < len; j++) {
			auto x = char_dist(engine) % book[95];
			auto p = std::upper_bound(book, book+96, x);
			tmp_str[j] = 0x20 + (p - book);
		}
		return tmp_str;
	};

	for (unsigned i = 0; i < 1000; i++) {
		auto len = static_cast<unsigned>(std::abs(len_dist(engine)));
		auto& ivec = dummy.ivec();
		ivec.resize(len);
		for (unsigned j = 0; j < len; j++) {
			ivec[j] = static_cast<int32_t>(num_dist(engine));
		}
		auto sz = static_cast<unsigned>(std::abs(len_dist(engine)));
		auto& imap = dummy.imap();
		imap.clear();
		for (unsigned k = 0; k < sz; k++) {
			imap.emplace(random_str(), static_cast<int64_t>(num_dist(engine)));
		}
		auto data = dummy.Serialize();
		char path[32];
		sprintf(path, "%u.bin", i);
		std::ofstream ofs(path);
		if (!ofs.write(reinterpret_cast<const char*>(data.data()), data.size()*sizeof(uint32_t))) {
			return -1;
		}
	}

	return 0;
}