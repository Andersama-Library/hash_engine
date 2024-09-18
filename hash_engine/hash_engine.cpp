// hash_engine.cpp : Defines the entry point for the application.
//

#include "hash_engine.h"
#include <iostream>

using namespace std;

uint32_t hash32(uint64_t x, uint64_t hash_0, uint64_t hash_1, uint64_t hash_2) {
	uint32_t low = (uint32_t)x;
	uint32_t high = (uint32_t)(x >> 32);
	return (uint32_t)((hash_0 * low + hash_1 * high + hash_2) >> 32);
}

uint64_t hash64(uint64_t x, uint64_t hash_0, uint64_t hash_1, uint64_t hash_2,
	uint64_t hash_3, uint64_t hash_4, uint64_t hash_5) {
	uint32_t low = (uint32_t)x;
	uint32_t high = (uint32_t)(x >> 32);
	uint64_t new_low = ((hash_0 * low + hash_1 * high + hash_2) >> 32);
	uint64_t new_high = ((hash_3 * low + hash_4 * high + hash_5) & 0xffffffff00000000);
	return new_low | new_high;
}

int main()
{
	hash_engine h;
	h.seed();
	//h.build(7, 0x80);
	h.build(12,0x1000,11,19);
	uint64_t hash_0 = h.pcg32_dxsm_random_r(&h.rng);
	uint64_t hash_1 = h.pcg32_dxsm_random_r(&h.rng);
	uint64_t hash_2 = h.pcg32_dxsm_random_r(&h.rng);
	uint64_t hash_3 = h.pcg32_dxsm_random_r(&h.rng);
	uint64_t hash_4 = h.pcg32_dxsm_random_r(&h.rng);
	uint64_t hash_5 = h.pcg32_dxsm_random_r(&h.rng);

	std::vector<uint32_t> checked;
	/*
	checked.resize(1 << 16);
	for (size_t shf = 1; shf < 64; shf++) {
		for (size_t i = 0; i < 0x10000; i++) {
			size_t v = (i << shf) ^ i;
			if (checked[v & 0xffff]) {
				std::cout << "bad transform\n";
				std::cout << shf << '\n' << i << '\n';
				return -1;
			}
			checked[v & 0xffff] = true;
		}
		checked.clear();
		checked.resize(1 << 16);
	}
	*/
	std::string out;
	for (size_t i = 0; i < ~0ull; i++) {
		size_t value = h.generate();
			//hash64(i,hash_0,hash_1,hash_2,hash_3,hash_4,hash_5);
			//h.pcg32_dxsm_random_r(&h.rng);
		size_t limited = value & h.output_mask;
		if ((i % h.period) == 0) {
			checked.clear();
			checked.assign(1ull << h.output_bits, 0);
		}
		if (checked[limited]) {
			std::cout << value << " previously seen\n";
			std::cout << i << " processed\n";
			break;
		}
		checked[limited] += 1;
		out.clear();
		out.reserve((1ull << h.output_bits) + 1);
		char *ptr = out.data();
		size_t s = 0;
		for (; s < (1ull << h.output_bits); s++) {
			ptr[s] = checked[s] ? '*' : ' ';
			//std::cout << (checked[i] ? "*" : " ");
			//out.append(checked[i] ? "*" : " ");
		}
		ptr[s] = '\n';
		ptr[s+1] = 0;
		//out.assign(out.data(), h.output_mask);
		std::cout << ptr;
		//std::cout << (uint16_t)value << '\n';
	}

	return 0;
}
