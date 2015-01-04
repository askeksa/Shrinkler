// Copyright 1999-2015 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Abstract interface for entropy coding.

*/

#pragma once

#include "assert.h"

class Coder {
public:
	// Number of fractional bits in the bit sizes returned by coding functions.
	static const int BIT_PRECISION = 6;

	// Code the given bit value in the given context.
	// Returns the coded size of the bit (in fractional bits).
	virtual int code(int context, int bit) = 0;

	// Encode a number >= 2 using a variable-length encoding.
	// Returns the coded size of the number (in fractional bits).
	int encodeNumber(int base_context, int number) {
		assert(number >= 2);
		int size = 0;
		int context;
		int i;
		for (i = 0 ; (4 << i) <= number ; i++) {
			context = base_context + (i * 2 + 2);
			size += code(context, 1);
		}
		context = base_context + (i * 2 + 2);
		size += code(context, 0);

		for (; i >= 0 ; i--) {
			int bit = ((number >> i) & 1);
			context = base_context + (i * 2 + 1);
			size += code(context, bit);
		}

		return size;
	}

	virtual ~Coder() {}
};
