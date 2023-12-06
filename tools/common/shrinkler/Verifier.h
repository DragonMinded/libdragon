// Copyright 1999-2022 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Verifying the correctness of crunched data and computing safety margin.

*/

#pragma once

#include "RangeDecoder.h"
#include "LZDecoder.h"


class LZVerifier : public LZReceiver, public CompressedDataReadListener {
	int hunk;
	unsigned char *data;
	int data_length;
	int hunk_mem;
	int read_size;
	int pos;

	unsigned char getData(int i) {
		if (data == NULL || i >= data_length) return 0;
		return data[i];
	}

public:
	int compressed_read_count;
	int front_overlap_margin;

	LZVerifier(int hunk, unsigned char *data, int data_length, int hunk_mem, int read_size)
		: hunk(hunk), data(data), data_length(data_length), hunk_mem(hunk_mem), read_size(read_size), pos(0) {
		compressed_read_count = 0;
		front_overlap_margin = 0;
	}

	bool receiveLiteral(unsigned char lit) {
		if (pos >= hunk_mem) {
			printf("Verify error: literal at position %d in hunk %d overflows hunk!\n",
				pos, hunk);
			return false;
		}
		if (lit != getData(pos)) {
			printf("Verify error: literal at position %d in hunk %d has incorrect value (0x%02X, should be 0x%02X)!\n",
				pos, hunk, lit, getData(pos));
			return false;
		}
		pos += 1;
		updateMargin();
		return true;
	}

	bool receiveReference(int offset, int length) {
		if (offset < 1 || offset > pos) {
			printf("Verify error: reference at position %d in hunk %d has invalid offset (%d)!\n",
				pos, hunk, offset);
			return false;
		}
		if (length > hunk_mem - pos) {
			printf("Verify error: reference at position %d in hunk %d overflows hunk (length %d, %d bytes past end)!\n",
				pos, hunk, length, pos + length - hunk_mem);
			return false;
		}
		for (int i = 0 ; i < length ; i++) {
			if (getData(pos - offset + i) != getData(pos + i)) {
				printf("Verify error: reference at position %d in hunk %d has incorrect value for byte %d of %d (0x%02X, should be 0x%02X)!\n",
					pos, hunk, i, length, getData(pos - offset + i), getData(pos + i));
				return false;
			}
		}
		pos += length;
		updateMargin();
		return true;
	}

	int size() {
		return pos;
	}

	void read(int index) {
		// Another byte of compresed data read
		if ((index & (read_size - 1)) == 0) {
			compressed_read_count += read_size;
			if (compressed_read_count > data_length)
				compressed_read_count = data_length;
			updateMargin();
		}
	}

	void updateMargin(void) {
		int margin = pos - compressed_read_count;
		if (margin > front_overlap_margin) {
			front_overlap_margin = margin;
		}
	}
};
