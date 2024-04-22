// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Abstract interface for entropy decoding.

*/

#pragma once

class Decoder {
public:
	// Decode a bit in the given context.
	// Returns the decoded bit value.
	virtual int decode(int context) = 0;

	// Decode a number >= 2 using a variable-length encoding.
	// Returns the decoded number.
	int decodeNumber(int base_context) {
		int context;
		int i;
		for (i = 0 ;; i++) {
			context = base_context + (i * 2 + 2);
			if (decode(context) == 0) break;
		}

		int number = 1;
		for (; i >= 0 ; i--) {
			context = base_context + (i * 2 + 1);
			int bit = decode(context);
			number = (number << 1) | bit;
		}

		return number;
	}

	virtual ~Decoder() {}
};
