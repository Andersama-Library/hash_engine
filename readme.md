hash engine
===========

A single header file to create random hash functions.

Notes and Usage
---------------

```c
#include <iostream>
#include "hash_engine.h"
#include <vector>

int main() {
	hash_engine h;
	// seed rng state
	h.seed();
	
	size_t output_bits = 12;
	size_t period      = 4096; // same as 1ull << output_bits
	
	// these parameters control the complexity of the generated functions
	// NOTE: internally these are garunteed to be odd to garuntee that least
	// one instruction is performed w/o the potential of being inverted
	size_t minimum_instructions = 11;
	size_t maximum_instructions = 19;

	// setup the hash engine
	h.build(output_bits,period,minimum_instructions,maximum_instructions);

	// the configuration is kept so that the engine can
	// rebuild a new hash function when the period is complete

	// usage:
	for (size_t i = 0; i < 1000000; i++) {
		// return the hashed value using the currently
		// "built" hash function
		size_t alternate = h.hash(h.current_index);

		// return a hash value from an internal counter inside hash_engine
		// increment the internal counter and
		// builds a new hash function after the period is complete
		size_t value = h.generate();
		
		// these values should be ==
		std::cout << alternate << '\t' << value << '\n';

		// the neat property of hash_engine's output is
		// that the result of the following is injective
		// for [0,1<<output_bits)!
		size_t masked_value = value & h.output_mask;
	}

	// card shuffle example:
	std::vector<size_t> deck;
	h.build(6,64,minimum_instructions,maximum_instructions);

	const char* card_suits[] = {"hearts","dimonds","spades","clubs"};
	const char* card_ranks[] = {"two of", "three of",
		"four of",
		"five of",
		"six of",
		"seven of",
		"eight of",
		"nine of",
		"ten of",
		"jack of",
		"queen of",
		"king of",
		"ace of" 
		};

	for (size_t i = 0; i < h.period; i++) {
		size_t card_value = h.hash(i) & h.output_mask;
		// we can use the injective property to garuntee the shuffle order
		if (card_value < 52) {
			deck.emplace_back(card_value);
			std::cout << card_ranks[card_value >> 2] << ' ' << card_suits[card_value & 0x3] << '\n';
		}
	}

	return 0;
}
```

Why?
====

I found myself in need of a hash function which held the following properties:

1. I could control the period of the cycle
2. The hash function had a range which I could garuntee was injective

Essentially I needed a way to remap a range of integers [x,y) -> [x,y), and so
I needed hash functions which would not collide within [x,y).

But I had one additional requirement, I needed multiple hash functions on the fly!