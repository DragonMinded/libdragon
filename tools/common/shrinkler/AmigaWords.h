// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Helper classes to access big-endian Amiga words and longwords.

*/

#pragma once

// A big-endian 16-bit integer with implicit conversions to and from unsigned short
class Word {
	unsigned short value;

	static unsigned short conv(unsigned short val) {
#ifndef AMIGA
		return
			((val & 0x00ff) << 8) |
			((val & 0xff00) >> 8);
#else
		return val;
#endif
	}
public:
	Word(unsigned short val) : value(conv(val)) {}
	Word() : value(0) {}

	operator unsigned short() const {
		return conv(value);
	}

	bool operator<(const Word& other) const {
		return conv(value) < conv(other.value);
	}

	unsigned short operator+=(unsigned short a) {
		unsigned short res = conv(value) + a;
		value = conv(res);
		return res;
	}
};

// A big-endian 32-bit integer with implicit conversions to and from unsigned int
class Longword {
	unsigned int value;

	static unsigned int conv(unsigned int val) {
#ifndef AMIGA
		return
			((val & 0x000000ff) << 24) |
			((val & 0x0000ff00) << 8) |
			((val & 0x00ff0000) >> 8) |
			((val & 0xff000000) >> 24);
#else
		return val;
#endif
	}
public:
	Longword(unsigned int value) : value(conv(value)) {}
	Longword() : value(0) {}

	operator unsigned int() const {
		return conv(value);
	}

	bool operator<(const Longword& other) const {
		return conv(value) < conv(other.value);
	}

	unsigned int operator+=(unsigned int a) {
		unsigned int res = conv(value) + a;
		value = conv(res);
		return res;
	}
};
