// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

A dummy entropy coder which counts the occurrences of symbols for estimating
the sizes using a SizeMeasuringCoder during the next compression pass.

*/

#pragma once

#include <vector>

using std::vector;

#include "Coder.h"

struct ContextCounts {
	int counts[2];
};

class CountingCoder : public Coder {
	vector<ContextCounts> context_counts;

	friend class SizeMeasuringCoder;
public:
	CountingCoder(int n_contexts) {
		struct ContextCounts init_counts = { { 0, 0 } };
		context_counts.resize(n_contexts, init_counts);
	}

	CountingCoder(CountingCoder *old_counts, CountingCoder *new_counts) {
		for (int i = 0 ; i < old_counts->context_counts.size() ; i++) {
			struct ContextCounts old_count = old_counts->context_counts[i];
			struct ContextCounts new_count = new_counts->context_counts[i];
			struct ContextCounts mixed_count = { {
				(old_count.counts[0] * 3 + new_count.counts[0]) / 4,
				(old_count.counts[1] * 3 + new_count.counts[1]) / 4
			} };
			context_counts.push_back(mixed_count);
		}
	}

	virtual int code(int context_index, int bit) {
		context_counts[context_index].counts[bit]++;
		return 0;
	}

	void printRange(FILE *out, int first, int num) {
		fprintf(out, "[");
		for (int i = 0 ; i < num ; i++) {
			if (i > 0) {
				fprintf(out, " ");
			}
			fprintf(out, "%d/%d", context_counts[first + i].counts[0], context_counts[first + i].counts[1]);
		}
		fprintf(out, "]");
	}

};
