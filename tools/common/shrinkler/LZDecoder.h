// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Decoder for the LZ encoder.

*/

#pragma once

#include "Decoder.h"
#include "LZEncoder.h"

class LZReceiver {
public:
	virtual bool receiveLiteral(unsigned char value) = 0;
	virtual bool receiveReference(int offset, int length) = 0;
	virtual ~LZReceiver() {}
};

class LZDecoder {
	Decoder *decoder;
	int parity_mask;

	int decode(int context) const {
		return decoder->decode(LZEncoder::NUM_SINGLE_CONTEXTS + context);
	}

	int decodeNumber(int context_group) const {
		return decoder->decodeNumber(LZEncoder::NUM_SINGLE_CONTEXTS + (context_group << 8));
	}

public:
	LZDecoder(Decoder *decoder, bool parity_context) : decoder(decoder), parity_mask(parity_context ? 1 : 0) {

	}

	bool decode(LZReceiver& receiver) {
		bool ref = false;
		bool prev_was_ref = false;
		int pos = 0;
		int offset = 0;
		do {
			if (ref) {
				bool repeated = false;
				if (!prev_was_ref) {
					repeated = decode(LZEncoder::CONTEXT_REPEATED);
				}
				if (!repeated) {
					offset = decodeNumber(LZEncoder::CONTEXT_GROUP_OFFSET) - 2;
					if (offset == 0) break;
				}
				int length = decodeNumber(LZEncoder::CONTEXT_GROUP_LENGTH);
				if (!receiver.receiveReference(offset, length)) return false;
				pos += length;
				prev_was_ref = true;
			} else {
				int parity = pos & parity_mask;
				int context = 1;
				for (int i = 7 ; i >= 0 ; i--) {
					int bit = decode((parity << 8) | context);
					context = (context << 1) | bit;
				}
				unsigned char lit = context;
				if (!receiver.receiveLiteral(lit)) return false;
				pos += 1;
				prev_was_ref = false;
			}
			int parity = pos & parity_mask;
			ref = decode(LZEncoder::CONTEXT_KIND + (parity << 8));
		} while (true);
		return true;
	}

};
