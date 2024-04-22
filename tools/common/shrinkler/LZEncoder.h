// Copyright 1999-2015 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

The LZ encoder defines the encoding of LZ symbols (literal bytes and references) into data bytes.

The encoding consists of three layers:

Layer 1 defines a plain encoding into bits. It is as follows:

  The first synbol is always a literal, and it is encoded as

    bit7 .. bit0

  Subsequent symbols can be either literals or references, and are encoded as one of

    0 bit7 .. bit0 (literal byte)
    1 0 <offset+2> <length> (reference)
    1 1 <length> (reference with same offset as previous reference)

  and the data block for each hunk is terminated by

    1 0 <2>

  The data block is followed by relocation entries, specifying positions within the data where the
  address of some hunk must be added. The entries are separated into one list for each hunk.
  Within each list, each entry is encoded as

    <delta from previous position> (the position before the first entry is assumed to be -4)

  and each list is terminated by

    <2>

  The <number> encodings in the above are variable-length numbers with a value of 2 or greater.
  The number 1 bit(n-1) .. bit0 is encoded as

  1^(n-1) 0 bit(n-1) .. bit0

Layer 2 defines a context for each bit of the Layer 1 encoding. The probability distribution
between 0 and 1 is modelled adaptively for each context.

  The first bit of the general symbol encoding (the one that selects between literal and reference)
  has one context for each parity of the byte position in the data (i.e. one for even bytes and one
  for odd bytes).

  The second bit of the reference symbol encoding (the one that selects between new and repeated
  offset) has a single context for itself.

  Literal bits have one context for each combination of parity and all higher numbered bits within
  the same literal byte. Thus, there are 510 different literal contexts.

  Numbers have one context group for each of offset, length and relocation entry. Within each group,
  there is one context for each of the prefix bits, and one context for each data bit number (i.e.
  bit(i) always uses the same context for all numbers with more than i data bits).

Layer 3 performs entropy coding of the Layer 1 bits based on the probabilities estimated by Layer 2.
The entropy coder defines the final compressed data contents.

*/

#pragma once

#include "Coder.h"

class LZState {
	unsigned after_first:1;
	unsigned prev_was_ref:1;
	unsigned parity:1;
	unsigned last_offset:28;

	friend class LZEncoder;
};

class LZEncoder {
	static const int NUM_SINGLE_CONTEXTS = 1;
	static const int NUM_CONTEXT_GROUPS = 4;
	static const int CONTEXT_GROUP_SIZE = 256;

	static const int CONTEXT_KIND = 0;
	static const int CONTEXT_REPEATED = -1;

	static const int CONTEXT_GROUP_LIT = 0;
	static const int CONTEXT_GROUP_OFFSET = 2;
	static const int CONTEXT_GROUP_LENGTH = 3;

	Coder *coder;
	int parity_mask;

	int code(int context, int bit) const {
		return coder->code(NUM_SINGLE_CONTEXTS + context, bit);
	}

	int encodeNumber(int context_group, int number) const {
		return coder->encodeNumber(NUM_SINGLE_CONTEXTS + (context_group << 8), number);
	}

	friend class LZDecoder;

public:
	static const int KIND_LIT = 0;
	static const int KIND_REF = 1;

	static const int NUM_CONTEXTS = (NUM_SINGLE_CONTEXTS + NUM_CONTEXT_GROUPS * CONTEXT_GROUP_SIZE);
	static const int NUMBER_CONTEXT_OFFSET = (NUM_SINGLE_CONTEXTS + CONTEXT_GROUP_OFFSET * CONTEXT_GROUP_SIZE);
	static const int NUM_NUMBER_CONTEXTS = 2;

	LZEncoder(Coder *coder, bool parity_context) : coder(coder), parity_mask(parity_context ? 1 : 0) {

	}

	void setInitialState(LZState *state) const {
		state->after_first = 0;
		state->prev_was_ref = 0;
		state->parity = 0;
		state->last_offset = 0;
	}

	void constructState(LZState *state, int pos, bool prev_was_ref, int last_offset) const {
		state->after_first = pos > 0;
		state->prev_was_ref = prev_was_ref;
		state->parity = pos;
		state->last_offset = last_offset;
	}

	int encodeLiteral(unsigned char value, const LZState *state_before, LZState *state_after) const {
		int parity_offset = (state_before->parity & parity_mask) << 8;
		int size = 0;
		if (state_before->after_first) {
			size += code(CONTEXT_KIND + parity_offset, KIND_LIT);
		}
		int context = 1;
		for (int i = 7 ; i >= 0 ; i--) {
			int bit = ((value >> i) & 1);
			size += code(parity_offset | context, bit);
			context = (context << 1) | bit;
		}

		state_after->after_first = 1;
		state_after->prev_was_ref = 0;
		state_after->parity = state_before->parity + 1;
		state_after->last_offset = state_before->last_offset;

		return size;
	}

	int encodeReference(int offset, int length, const LZState *state_before, LZState *state_after) const {
		assert(offset >= 1);
		assert(length >= 2);
		assert(state_before->after_first);

		int parity_offset = (state_before->parity & parity_mask) << 8;
		int size = code(CONTEXT_KIND + parity_offset, KIND_REF);
		int rep_offset = offset == state_before->last_offset;
		if (!state_before->prev_was_ref) {
			size += code(CONTEXT_REPEATED, rep_offset);
		} else {
			assert(!rep_offset);
		}
		if (!rep_offset) {
			size += encodeNumber(CONTEXT_GROUP_OFFSET, offset + 2);
		}
		size += encodeNumber(CONTEXT_GROUP_LENGTH, length);

		state_after->after_first = 1;
		state_after->prev_was_ref = 1;
		state_after->parity = state_before->parity + length;
		state_after->last_offset = offset;

		return size;
	}

	int finish(const LZState *state_before) const {
		int parity_offset = (state_before->parity & parity_mask) << 8;
		int size = code(CONTEXT_KIND + parity_offset, KIND_REF);
		if (!state_before->prev_was_ref) {
			size += code(CONTEXT_REPEATED, 0);
		}
		int context_group = CONTEXT_GROUP_OFFSET;
		int number = 2;
		size += encodeNumber(context_group, number);

		return size;
	}
};

