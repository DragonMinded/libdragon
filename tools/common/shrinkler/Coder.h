// Copyright 1999-2015 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Abstract interface for entropy coding.

*/

#pragma once

#include "assert.h"

#include <vector>

using std::vector;

class Coder {
	bool cacheable;
	bool has_cache;
	int number_context_offset;
	int n_number_contexts;
	vector<vector<unsigned short> > cache;

protected:
	Coder() : cacheable(false), has_cache(false)
	{}

	// Mark coder as cacheable
	void setCacheable(bool cacheable) {
		this->cacheable = cacheable;
	}

public:
	// Set parameters for number size cache
	void setNumberContexts(int number_context_offset, int n_number_contexts, int max_number) {
		if (!cacheable) return;

		this->number_context_offset = number_context_offset;
		this->n_number_contexts = n_number_contexts;
		cache.clear();
		for (int context_index = 0 ; context_index < n_number_contexts ; context_index++) {
			int base_context = number_context_offset + (context_index << 8);
			cache.push_back(vector<unsigned short>());
			vector<unsigned short>& c = cache.back();
			c.resize(4);
			c[2] = code(base_context + 2, 0) + code(base_context + 1, 0);
			c[3] = code(base_context + 2, 0) + code(base_context + 1, 1);
			int prev_base = 2;
			for (int data_bits = 2 ; data_bits < 30 ; data_bits++) {
				int base = c.size();
				int base_sizedif = - code(base_context + data_bits * 2 - 2, 0)
				                   + code(base_context + data_bits * 2 - 2, 1)
				                   + code(base_context + data_bits * 2, 0);
				for (int msb = 0 ; msb <= 1 ; msb++) {
					int sizedif = base_sizedif + code(base_context + data_bits * 2 - 1, msb);
					for (int tail = 0 ; tail < 1 << (data_bits - 1) ; tail++) {
						int size = c[prev_base + tail] + sizedif;
						c.push_back(size);
						if (c.size() > max_number) goto next_context;
					}
				}
				prev_base = base;
			}
			next_context:;
#if 0
			for (int i = 2 ; i < c.size() ; i++) {
				assert(c[i] == encodeNumber(base_context, i));
			}
#endif
		}

		has_cache = true;
	}

	// Number of fractional bits in the bit sizes returned by coding functions.
	static const int BIT_PRECISION = 6;

	// Code the given bit value in the given context.
	// Returns the coded size of the bit (in fractional bits).
	virtual int code(int context, int bit) = 0;

	// Encode a number >= 2 using a variable-length encoding.
	// Returns the coded size of the number (in fractional bits).
	int encodeNumber(int base_context, int number) {
		assert(number >= 2);

		if (has_cache) {
			int context_index = (base_context - number_context_offset) >> 8;
			vector<unsigned short>& cache_for_context = cache[context_index];
			if (number < cache_for_context.size()) {
				return cache_for_context[number];
			}
		}

		int size = 0;
		int context;
		int i;
		for (i = 0 ; (4 << i) <= number ; i++) {
			context = base_context + (i * 2 + 2);
			size += code(context, 1);
		}
		context = base_context + (i * 2 + 2);
		size += code(context, 0);

		for (; i >= 0 ; i--) {
			int bit = ((number >> i) & 1);
			context = base_context + (i * 2 + 1);
			size += code(context, bit);
		}

		return size;
	}

	virtual ~Coder() {}
};
