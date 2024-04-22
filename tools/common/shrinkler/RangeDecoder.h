// Copyright 1999-2022 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

A decoder for the range coder.

*/

#pragma once

#include <cmath>
#include <algorithm>
#include <vector>

using std::fill;
using std::vector;

#include "Decoder.h"
#include "assert.h"

#ifndef ADJUST_SHIFT
#define ADJUST_SHIFT 4
#endif

class CompressedDataReadListener {
public:
	virtual void read(int index) = 0;

	virtual ~CompressedDataReadListener() {}
};

class RangeDecoder : public Decoder {
	vector<unsigned short> contexts;
	vector<unsigned char>& data;
	CompressedDataReadListener* listener;
	int bit_index;
	unsigned intervalsize;
	unsigned intervalvalue;
	unsigned uncertainty;

	int getBit() {
		int byte_index = bit_index >> 3;
		int bit_in_byte = (~bit_index) & 7;
		if (bit_in_byte == 7) {
			if (listener) listener->read(byte_index);
		}
		if (bit_index++ >= data.size() * 8) {
			uncertainty <<= 1;
			return 0;
		}
		int bit = (data[byte_index] >> bit_in_byte) & 1;
		return bit;
	}

public:
	RangeDecoder(int n_contexts, vector<unsigned char>& data) : data(data) {
		contexts.resize(n_contexts, 0x8000);
		bit_index = 0;
		intervalsize = 1;
		intervalvalue = 0;
		uncertainty = 1;
		listener = NULL;
	}

	virtual int decode(int context_index) {
		assert(context_index < contexts.size());
		unsigned prob = contexts[context_index];
		while (intervalsize < 0x8000) {
			intervalsize <<= 1;
			intervalvalue = (intervalvalue << 1) | getBit();
		}

		int bit;
		unsigned new_prob;
		unsigned threshold = (intervalsize * prob) >> 16;
		if (intervalvalue >= threshold) {
			// Zero
			bit = 0;
			intervalvalue -= threshold;
			intervalsize -= threshold;
			new_prob = prob - (prob >> ADJUST_SHIFT);
		} else {
			// One
			assert(intervalvalue + uncertainty <= threshold);
			bit = 1;
			intervalsize = threshold;
			new_prob = prob + (0xffff >> ADJUST_SHIFT) - (prob >> ADJUST_SHIFT);
		}
		assert(new_prob > 0);
		assert(new_prob < 0x10000);
		contexts[context_index] = new_prob;

		return bit;
	}

	void reset() {
		fill(contexts.begin(), contexts.end(), 0x8000);
	}

	void setListener(CompressedDataReadListener* listener) {
		this->listener = listener;
	}
};
