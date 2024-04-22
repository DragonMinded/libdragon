// Copyright 1999-2015 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

A dummy entropy coder which estimates the size of coded symbols based on
counts from a CountingCoder.

*/

#pragma once

#include <vector>

using std::vector;

#include "CountingCoder.h"

struct ContextSizes {
	unsigned short sizes[2];
};

class SizeMeasuringCoder : public Coder {
	static const int MIN_SIZE = 2;
	static const int MAX_SIZE = 12 << BIT_PRECISION;

	vector<ContextSizes> context_sizes;

	int sizeForCount(int count, int total) {
		int size = (int) floor(0.5 + log(total / (double) count) / log(2.0) * (1 << BIT_PRECISION));
		if (size < MIN_SIZE) size = MIN_SIZE;
		if (size > MAX_SIZE) size = MAX_SIZE;
		return size;
	}

public:
	SizeMeasuringCoder(int n_contexts) {
		struct ContextSizes default_sizes = { { 1 << BIT_PRECISION, 1 << BIT_PRECISION } };
		context_sizes.resize(n_contexts, default_sizes);
		setCacheable(true);
	}

	SizeMeasuringCoder(CountingCoder *counting_coder) {
		context_sizes.resize(counting_coder->context_counts.size());
		for (int i = 0 ; i < counting_coder->context_counts.size() ; i++) {
			struct ContextSizes s;
			struct ContextCounts c = counting_coder->context_counts[i];
			int count0 = 1 + c.counts[0];
			int count1 = 1 + c.counts[1];
			int sum = count0 + count1;
			s.sizes[0] = sizeForCount(count0, sum);
			s.sizes[1] = sizeForCount(count1, sum);
			context_sizes[i] = s;
		}
		setCacheable(true);
	}

	virtual int code(int context_index, int bit) {
		return context_sizes[context_index].sizes[bit];
	}
};
