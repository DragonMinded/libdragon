// Copyright 1999-2019 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Suffix array construction based on the SA-IS algorithm.

*/

#pragma once

#include <vector>
#include <algorithm>

using std::vector;
using std::fill;

#define UNINITIALIZED (-1)
#define IS_LMS(i) ((i) > 0 && stype[(i)] && !stype[(i) - 1])

void induce(const int *data, int *suffix_array, int length, int alphabet_size, const vector<bool>& stype, const int *buckets, int *bucket_index) {
	// Induce L suffixes
	for (int b = 0 ; b < alphabet_size ; b++) {
		bucket_index[b] = buckets[b];
	}
	for (int s = 0 ; s < length ; s++) {
		int index = suffix_array[s];
		if (index > 0 && !stype[index - 1]) {
			suffix_array[bucket_index[data[index - 1]]++] = index - 1;
		}
	}
	// Induce S suffixes
	for (int b = 0 ; b < alphabet_size ; b++) {
		bucket_index[b] = buckets[b + 1];
	}
	for (int s = length - 1 ; s >= 0 ; s--) {
		int index = suffix_array[s];
		assert(index != UNINITIALIZED);
		if (index > 0 && stype[index - 1]) {
			suffix_array[--bucket_index[data[index - 1]]] = index - 1;
		}
	}
}

bool substrings_equal(const int *data, int i1, int i2, const vector<bool>& stype) {
	while (data[i1++] == data[i2++]) {
		if (IS_LMS(i1) && IS_LMS(i2)) return true;
	}
	return false;
}

// Compute the suffix array of a string over an integer alphabet.
// The last character in the string (the sentinel) must be uniquely smallest in the string.
void computeSuffixArray(const int *data, int *suffix_array, int length, int alphabet_size) {
	// Handle empty string
	assert(length >= 1);
	if (length == 1) {
		suffix_array[0] = 0;
		return;
	}

	vector<bool> stype(length);
	vector<int> buckets(alphabet_size + 1, 0);
	vector<int> bucket_index(alphabet_size);

	// Compute suffix types and count symbols
	stype[length - 1] = true;
	buckets[data[length - 1]] = 1;
	bool is_s = true;
	int lms_count = 0;
	for (int i = length - 2; i >= 0; i--) {
		buckets[data[i]]++;
		if (data[i] > data[i + 1]) {
			if (is_s) lms_count++;
			is_s = false;
		} else if (data[i] < data[i + 1]) {
			is_s = true;
		}
		stype[i] = is_s;
	}

	// Accumulate bucket sizes
	int l = 0;
	for (int b = 0; b <= alphabet_size; b++) {
		int l_next = l + buckets[b];
		buckets[b] = l;
		l = l_next;
	}
	assert(l == length);

	// Put LMS suffixes at the ends of buckets
	fill(&suffix_array[0], &suffix_array[length], UNINITIALIZED);
	for (int b = 0 ; b < alphabet_size ; b++) {
		bucket_index[b] = buckets[b + 1];
	}
	for (int i = length - 1; i >= 1; i--) {
		if (IS_LMS(i)) {
			suffix_array[--bucket_index[data[i]]] = i;
		}
	}

	// Induce to sort LMS strings
	induce(data, suffix_array, length, alphabet_size, stype, &buckets[0], &bucket_index[0]);

	// Compact LMS indices at the beginning of the suffix array
	int j = 0;
	for (int s = 0; s < length; s++) {
		int index = suffix_array[s];
		if (IS_LMS(index)) {
			suffix_array[j++] = index;
		}
	}
	assert(j == lms_count);

	// Name LMS strings, using the second half of the suffix array
	int *sub_data = &suffix_array[length / 2];
	int sub_capacity = length - length / 2;
	fill(sub_data, &sub_data[sub_capacity], UNINITIALIZED);
	int name = 0;
	int prev_index = UNINITIALIZED;
	for (int s = 0; s < lms_count; s++) {
		int index = suffix_array[s];
		assert(index != UNINITIALIZED);
		if (prev_index != UNINITIALIZED && !substrings_equal(data, prev_index, index, stype)) {
			name += 1;
		}
		assert(sub_data[index / 2] == UNINITIALIZED);
		sub_data[index / 2] = name;
		prev_index = index;
	}
	int new_alphabet_size = name + 1;

	if (new_alphabet_size != lms_count) {
		// Order LMS strings using suffix array of named LMS symbols

		// Compact named LMS symbols
		j = 0;
		for (int i = 0; i < sub_capacity; i++) {
			int name = sub_data[i];
			if (name != UNINITIALIZED) {
				sub_data[j++] = name;
			}
		}
		assert(j == lms_count);

		// Sort named LMS symbols recursively
		computeSuffixArray(sub_data, suffix_array, lms_count, new_alphabet_size);

		// Map named LMS symbol indices to LMS string indices in input string
		j = 0;
		for (int i = 1; i < length ; i++) {
			if (IS_LMS(i)) {
				sub_data[j++] = i;
			}
		}
		assert(j == lms_count);
		for (int s = 0 ; s < lms_count ; s++) {
			assert(suffix_array[s] < lms_count);
			suffix_array[s] = sub_data[suffix_array[s]];
		}
	}

	// Put LMS suffixes in sorted order at the ends of buckets
	j = length;
	int s = lms_count - 1;
	for (int b = alphabet_size - 1; b >= 0; b--) {
		while (s >= 0 && data[suffix_array[s]] == b) {
			suffix_array[--j] = suffix_array[s--];
		}
		assert(j >= buckets[b]);
		while (j > buckets[b]) {
			suffix_array[--j] = UNINITIALIZED;
		}
	}

	// Induce from sorted LMS strings to sort all suffixes
	induce(data, suffix_array, length, alphabet_size, stype, &buckets[0], &bucket_index[0]);
}
