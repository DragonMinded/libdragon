// Copyright 1999-2022 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

An entropy coder based on range coding.

*/

#pragma once

#include <cassert>
#include <cmath>
#include <algorithm>
#include <vector>

using std::fill;
using std::vector;

#include "Coder.h"

#ifndef ADJUST_SHIFT
#define ADJUST_SHIFT 4
#endif

class RangeCoder : public Coder {
	vector<unsigned short> contexts;
	vector<unsigned char>& out;
	int dest_bit;
	unsigned intervalsize;
	unsigned intervalmin;

	static int sizetable[128];
	static bool sizetable_init;

	static bool init_sizetable() {
		for (int i = 0 ; i < 128 ; i++) {
			sizetable[i] = (int) floor(0.5 + (8.0 - log((double) (128 + i)) / log(2.0)) * (1 << BIT_PRECISION));
		}

		return true;
	}

	void addBit() {
		int pos = dest_bit;
		int bytepos;
		int bitmask;
		do {
			pos--;
			if (pos < 0) return;
			bytepos = pos >> 3;
			bitmask = 0x80 >> (pos & 7);
			while (bytepos >= out.size()) {
				out.push_back(0);
			}
			out[bytepos] ^= bitmask;
		} while ((out[bytepos] & bitmask) == 0);
	}

public:
	RangeCoder(int n_contexts, vector<unsigned char>& out) : out(out) {
		contexts.resize(n_contexts, 0x8000);
		dest_bit = -1;
		intervalsize = 0x8000;
		intervalmin = 0;
		out.clear();
	}

	virtual int code(int context_index, int bit) {
		assert(context_index < contexts.size());
		assert(bit == 0 || bit == 1);
		int size_before = (dest_bit << BIT_PRECISION) + sizetable[(intervalsize - 0x8000) >> 8];
		unsigned prob = contexts[context_index];
		unsigned threshold = (intervalsize * prob) >> 16;
		unsigned new_prob;
		if (!bit) {
			// Zero
			intervalmin += threshold;
			if (intervalmin & 0x10000) {
				addBit();
			}
			intervalsize = intervalsize - threshold;
			new_prob = prob - (prob >> ADJUST_SHIFT);
		} else {
			// One
			intervalsize = threshold;
			new_prob = prob + (0xffff >> ADJUST_SHIFT) - (prob >> ADJUST_SHIFT);
		}
		assert(new_prob > 0);
		assert(new_prob < 0x10000);
		contexts[context_index] = new_prob;
		while (intervalsize < 0x8000) {
			dest_bit++;
			intervalsize <<= 1;
			intervalmin <<= 1;
			if (intervalmin & 0x10000) {
				addBit();
			}
		}
		intervalmin &= 0xffff;

		int size_after = (dest_bit << BIT_PRECISION) + sizetable[(intervalsize - 0x8000) >> 8];
		return size_after - size_before;
	}

	void reset() {
		fill(contexts.begin(), contexts.end(), 0x8000);
	}

	void finish() {
		int intervalmax = intervalmin + intervalsize;
		int final_min = 0;
		int final_size = 0x10000;
		while (final_min < intervalmin || final_min + final_size >= intervalmax) {
			if (final_min + final_size < intervalmax) {
				addBit();
				final_min += final_size;
			}
			dest_bit++;
			final_size >>= 1;
		}

		while ((dest_bit - 1) >> 3 >= out.size()) {
			out.push_back(0);
		}
	}

	int sizeInBits() {
		return dest_bit + 1;
	}

};


int RangeCoder::sizetable[128];
bool RangeCoder::sizetable_init = init_sizetable();
