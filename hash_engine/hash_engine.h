#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <chrono>

struct hash_engine {
	enum instruction : uint8_t {
		bld,
		add,
		//sub,
		mul,
		exor,
		rot,
		//xshl,
		xshr
	};

	uint64_t current_index;
	uint64_t current_parameter;

	std::vector<instruction> instructions;
	std::vector<size_t>      parameters;
	
	uint64_t output_bits;
	uint64_t output_mask;
	uint64_t period;
	
	uint32_t min_instructions;
	uint32_t max_instructions;

	typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

	pcg32_random_t rng;

	static uint32_t pcg32_random_r(pcg32_random_t* rng)
	{
		uint64_t oldstate = rng->state;
		// Advance internal state
		rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
		// Calculate output function (XSH RR), uses old state for max ILP
		uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
		uint32_t rot = oldstate >> 59u;
		return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	}

	static uint64_t pcg32_dxsm_random_r(pcg32_random_t* rng)
	{
		uint64_t hi = rng->state;
		uint64_t lo = rng->inc;

		constexpr uint32_t quarter_bits = (sizeof(uint32_t) * 8) / 4;
		constexpr uint32_t half_bits = quarter_bits * 2;
		constexpr uint32_t three_quarters = quarter_bits * 3;
		constexpr uint64_t lcg_multiplier = 0xda942042e4dd58b5ULL;

		lo |= 1;
		rng->state = hi * lcg_multiplier + lo;

		hi ^= hi >> half_bits;
		hi *= lcg_multiplier;
		hi ^= hi >> three_quarters;
		hi *= lo;
		return hi;
	}

	static int64_t multiplicative_inverse(int64_t mul, int64_t mod_n) {
		int64_t t = 0;
		int64_t newt = 1;
		int64_t r = mod_n;
		int64_t newr = mul;

		while (newr) {
			int64_t q = r / newr;
			
			int64_t old_r = r;
			r = newr;
			newr = old_r - (q * newr);
			
			int64_t old_t = t;
			t = newt;
			newt = old_t - (q * newt);
		}

		if (r > 1) {
			// not invertible, mul may be a multiple of mod_n
		}
		if (t < 0) {
			t += mod_n;
		}
		return t;
	}

	uint32_t bounded_rand(pcg32_random_t* rng, uint32_t range) {
		uint32_t t = (-range) % range;
		uint64_t m;
		uint32_t l;
		do {
			uint32_t x = pcg32_dxsm_random_r(rng);
			m = uint64_t(x) * uint64_t(range);
			l = uint32_t(m);
		} while (l < t);
		return m >> 32;
	}

	void seed() {
		current_index = 0;

		rng.state = (uint64_t)this;
		rng.inc = std::chrono::steady_clock::now().time_since_epoch().count();

		rng.state = pcg32_dxsm_random_r(&rng);
		rng.inc   = pcg32_dxsm_random_r(&rng);
	}

	const char* instruction_name(instruction interp_code) {
		switch (interp_code) {
		case bld:
			return "bld";
		case add:
			return "add";
		case mul:
			return "mul";
		case exor:
			return "xor";
		case rot:
			return "rot";
		case xshr:
			return "xshr";
		default:
			return "nop";
		}
	}
	
	/* generates a parameter, avoiding inverse operations of previous instructions */
	size_t build_parameter(instruction interp_code) {
		size_t rng_value = 0;
		switch (interp_code) {
		case bld:
			rng_value = pcg32_dxsm_random_r(&rng);
			break;
		case add: {
			bool is_inverse = false;
			do {
				rng_value = pcg32_dxsm_random_r(&rng);
				size_t inverse = -rng_value;
				size_t lim = instructions.size() < parameters.size() ? instructions.size() : parameters.size();
				for (size_t i = 0; i < lim; i++) {
					if (instructions[i] == add && parameters[i] == inverse) {
						is_inverse = true;
						break;
					}
				}
			} while (rng_value == 0 || is_inverse);
			break;
		}
		case mul:
		{
			bool is_inverse = false;
			do {
				rng_value = pcg32_dxsm_random_r(&rng) | 1;
				size_t inverse = multiplicative_inverse(rng_value, 1ull << output_bits);
				size_t lim = instructions.size() < parameters.size() ? instructions.size() : parameters.size();
				for (size_t i = 0; i < lim; i++) {
					if (instructions[i] == mul && parameters[i] == inverse) {
						is_inverse = true;
						break;
					}
				}
			} while ((rng_value & output_mask) <= 1 || is_inverse);
			break;
		}
		case exor: {
			bool is_inverse = false;
			do {
				rng_value = pcg32_dxsm_random_r(&rng);
				size_t inverse = rng_value;
				size_t lim = instructions.size() < parameters.size() ? instructions.size() : parameters.size();
				for (size_t i = 0; i < lim; i++) {
					if (instructions[i] == exor && parameters[i] == inverse) {
						is_inverse = true;
						break;
					}
				}
			} while (rng_value == 0 || is_inverse);
			break;
		}
		case rot:
		case xshr:
			rng_value = bounded_rand(&rng, output_bits-1)+1;
			break;
		default:
			rng_value = pcg32_dxsm_random_r(&rng);
			break;
		}
		return rng_value;
	}

	/* randomly generate a list of instructions to output a specified target number of bits
		where the generator rebuilds after a set period.
		IE: if output_bits == 7 and the period is 128, the generator will produce the numbers
		[0,1 << output_bits) uniformally because the period matches the # of potential outputs
	*/
	void build(size_t output_bits, size_t period, uint32_t min_instruction_count = 3, uint32_t max_instruction_count = 7) {
		this->output_bits = output_bits;
		this->period  = period;
		this->output_mask = ~(~0ull << output_bits);

		min_instruction_count |= 1;
		if ((max_instruction_count & 0x1) == 0)
			max_instruction_count -= 1;

		this->min_instructions = min_instruction_count < max_instruction_count ? min_instruction_count : max_instruction_count;
		this->max_instructions = min_instruction_count < max_instruction_count ? max_instruction_count : min_instruction_count;

		instructions.clear();
		parameters.clear();
		
		if (output_bits == 0)
			return;

		uint32_t count_instructions = bounded_rand(&rng, max_instructions - min_instructions) + min_instructions;
		//generate one of the instructions (except 0)
		uint32_t last_rand = bounded_rand(&rng, xshr); //[0,xhr) +1 -> [1,xhr]
		uint32_t interp_code = last_rand + 1;

		parameters.emplace_back(build_parameter((instruction)interp_code));
		instructions.emplace_back((instruction)interp_code);

		for (size_t i = 1; i < count_instructions; i++) {
			//generate a different instruction (not including 0)
			uint32_t new_rand = bounded_rand(&rng, xshr-1)+1; //[0,xhr-1) +1 -> [1,xhr-1]
			new_rand = ((last_rand + new_rand) % xshr);
			uint32_t interp_code = new_rand + 1;
			last_rand = new_rand;

			parameters.emplace_back(build_parameter((instruction)interp_code));
			instructions.emplace_back((instruction)interp_code);
		}
	}

	/* Transforms: Takes an input number and modifies it (potentially with additional parameters), such that the result would be uniform in the range [0,1<<output_bits) assuming a period of 1<<output_bits.
	* 
	* The extra parameters are expected to have been generated inside build() > build_parameter()
	* The current design of the api is pretty basic, each of these transforms expects a
	* single additional parameter.
	*/

	size_t do_build(uint64_t input) {
		bool rebuild = current_index >= period;
		current_index *= !rebuild;
		if (rebuild)
			build(output_bits, period, min_instructions, max_instructions);
		return input;
	}

	size_t do_add(uint64_t input) {
		uint64_t result = input + parameters[current_parameter];
		current_parameter += 1;
		return result;
	}

	size_t do_mul(uint64_t input) {
		uint64_t result = input * parameters[current_parameter];
		current_parameter += 1;
		return result;
	}

	size_t do_sub(uint64_t input) {
		uint64_t result = input - parameters[current_parameter];
		current_parameter += 1;
		return result;
	}

	size_t do_xor(uint64_t input) {
		uint64_t mask   = parameters[current_parameter];
		uint64_t result = input ^ mask;
		current_parameter += 1;
		return result;
	}

	size_t do_rot(uint64_t input) {
		uint64_t rshf = parameters[current_parameter];
		uint64_t lshf = output_bits - rshf;
		uint64_t input_masked = input & output_mask;

		uint64_t result = (input_masked >> rshf) | (input_masked << lshf) |
			(input & ~output_mask);
		current_parameter += 1;
		return result;
	}

	size_t do_xshl(uint64_t input) {
		uint64_t shf = parameters[current_parameter];

		uint64_t input_masked = input & output_mask;
		uint64_t result = ((input_masked << shf) ^ input_masked);
		current_parameter += 1;
		return result;
	}

	size_t do_xshr(uint64_t input) {
		uint64_t shf = parameters[current_parameter];

		uint64_t input_masked = input & output_mask;
		uint64_t result = ((input_masked >> shf) ^ input_masked) |
			(input & ~output_mask);;
		current_parameter += 1;
		return result;
	}

	size_t hash(size_t value) {
		current_parameter ^= current_parameter;

		for (size_t i = 0; i < instructions.size(); i++) {
			uint8_t interp_code = (uint8_t)instructions[i];
			switch (interp_code) {
			case bld:
				value = do_build(value);
				break;
			case add:
				value = do_add(value);
				break;
			case mul:
				value = do_mul(value);
				break;
			case exor:
				value = do_xor(value);
				break;
			case rot:
				value = do_rot(value);
				break;
			case xshr:
				value = do_xshr(value);
				break;
			}
		}
		
		return value;
	}

	size_t generate() {
		size_t value = current_index;
		current_index += 1;
		value = hash(value);
		value = do_build(value);
		return value;
	}
};