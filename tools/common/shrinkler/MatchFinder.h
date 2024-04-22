// Copyright 1999-2020 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Find repeated strings in a data block.

Matches are reported from longest to shortest. A match is only reported
if it is closer (smaller offset, higher position) than all longer matches.

Two parameters control the speed/precision tradeoff of the matcher:

The match_patience parameter controls how many matches outside the current
reporting range (between last longer match and current position) are skipped
before the matcher gives up finding more matches.

The max_same_length parameter controls how many matches of the same length
are reported. The matches reported will be the closest ones of that length.

*/

#pragma once

#include <vector>
#include <algorithm>
#include <queue>
#include <functional>

using std::vector;

#include "SuffixArray.h"

class MatchFinder {
	// Inputs
	unsigned char *data;
	int length;
	int min_length;
	int match_patience;
	int max_same_length;

	// Suffix array
	vector<int> suffix_array;
	vector<int> rev_suffix_array;
	vector<int> longest_common_prefix;

	// Matcher parameters
	int current_pos;
	int min_pos;

	// Matcher state
	int left_index;
	int left_length;
	int right_index;
	int right_length;
	int current_length;

	// Best matches seen with current length
	std::priority_queue<int, vector<int>, std::greater<int> > match_buffer;

	void make_suffix_array() {
		// Use reverse suffix array to store string as integers with sentinel
		rev_suffix_array.resize(length + 1);
		for (int i = 0; i < length ; i++) {
			rev_suffix_array[i] = data[i] + 1;
		}
		rev_suffix_array[length] = 0;

		// Compute suffix array
		suffix_array.resize(length + 1);
		computeSuffixArray(&rev_suffix_array[0], &suffix_array[0], length + 1, 257);

		// Compute reverse suffix array
		for (int i = 0 ; i <= length ; i++) {
			rev_suffix_array[suffix_array[i]] = i;
		}

		// Compute LCP array
		longest_common_prefix.resize(length + 1);
		longest_common_prefix[0] = 0;
		longest_common_prefix[length] = 0;
		int h = 0;
		for (int i = 0 ; i < length ; i++) {
			int r = rev_suffix_array[i];
			if (r < length) {
				int j = suffix_array[r + 1];
				int m = length - std::max(i, j);
				while (h < m && data[i + h] == data[j + h]) {
					h = h + 1;
				}
				longest_common_prefix[r] = h;
				if (h > 0) h = h - 1;
			}
		}
	}

	void extend_left() {
		int iter = 0;
		while (left_length >= min_length) {
			left_length = std::min(left_length, longest_common_prefix[--left_index]);
			int pos = suffix_array[left_index];
			if (pos < current_pos && pos >= min_pos) break;
			if (++iter > match_patience) {
				left_length = 0;
				break;
			}
		}
	}

	void extend_right() {
		int iter = 0;
		while (true) {
			right_length = std::min(right_length, longest_common_prefix[right_index]);
			if (right_length < min_length) break;
			int pos = suffix_array[++right_index];
			if (pos < current_pos && pos >= min_pos) break;
			if (++iter > match_patience) {
				right_length = 0;
				break;
			}
		}
	}

	int next_length() {
		return std::max(left_length, right_length);
	}

public:
	MatchFinder(unsigned char *data, int length, int min_length, int match_patience, int max_same_length) :
		data(data), length(length), min_length(min_length), match_patience(match_patience), max_same_length(max_same_length) {
		make_suffix_array();
		reset();
	}

	void reset() {
	}

	// Start finding matches between strings starting at pos and earlier strings.
	void beginMatching(int pos) {
		current_pos = pos;
		min_pos = 0;

		left_index = rev_suffix_array[pos];
		left_length = length - pos;
		extend_left();
		right_index = rev_suffix_array[pos];
		right_length = length - pos;
		extend_right();
	}

	// Report next match. Returns whether a match was found.
	bool nextMatch(int *match_pos_out, int *match_length_out) {
		if (match_buffer.empty()) {
			// Fill match buffer
			current_length = next_length();
			if (current_length < min_length) return false;
			int new_min_pos = min_pos;
			do {
				int match_pos;
				if (left_length > right_length) {
					match_pos = suffix_array[left_index];
					extend_left();
				} else {
					match_pos = suffix_array[right_index];
					extend_right();
				}
				new_min_pos = std::max(new_min_pos, match_pos);
				if (match_buffer.size() < max_same_length) {
					match_buffer.push(match_pos);
				} else {
					if (match_pos > match_buffer.top()) {
						match_buffer.pop();
						match_buffer.push(match_pos);
					}
					min_pos = match_buffer.top();
				}
			} while (next_length() == current_length);
			assert(!match_buffer.empty());
			min_pos = new_min_pos;
		}

		*match_length_out = current_length;
		*match_pos_out = match_buffer.top();
		match_buffer.pop();
		assert(*match_pos_out < current_pos);
		return true;
	}
};
