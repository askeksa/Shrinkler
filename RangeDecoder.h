// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

A decoder for the range coder.

*/

#pragma once

#include <cassert>
#include <cmath>
#include <algorithm>
#include <vector>

using std::fill;
using std::vector;

#include "Decoder.h"

#ifndef ADJUST_SHIFT
#define ADJUST_SHIFT 4
#endif

class RangeDecoder : public Decoder {
	vector<unsigned short> contexts;
	vector<unsigned>& data;
	int bit_index;
	unsigned intervalsize;
	unsigned intervalvalue;
	unsigned uncertainty;

	int getBit() {
		if (bit_index >= data.size() * 32) {
			uncertainty <<= 1;
			return 0;
		}
		int long_index = bit_index >> 5;
		int bit_in_long = (~bit_index) & 31;
		int bit = (data[long_index] >> bit_in_long) & 1;
		bit_index++;
		return bit;
	}

public:
	RangeDecoder(int n_contexts, vector<unsigned>& data) : data(data) {
		contexts.resize(n_contexts, 0x8000);
		bit_index = 0;
		intervalsize = 1;
		intervalvalue = 0;
		uncertainty = 1;
	}

	virtual int decode(int context_index) {
		assert(context_index < contexts.size());
		unsigned prob = contexts[context_index];
		while (intervalsize < 0x8000) {
			intervalsize <<= 1;
			intervalvalue = (intervalvalue << 1) | getBit();
		}

		int bit;
		unsigned new_prob;
		unsigned threshold = (intervalsize * prob) >> 16;
		if (intervalvalue >= threshold) {
			// Zero
			bit = 0;
			intervalvalue -= threshold;
			intervalsize -= threshold;
			new_prob = prob - (prob >> ADJUST_SHIFT);
		} else {
			// One
			assert(intervalvalue + uncertainty <= threshold);
			bit = 1;
			intervalsize = threshold;
			new_prob = prob + (0xffff >> ADJUST_SHIFT) - (prob >> ADJUST_SHIFT);
		}
		assert(new_prob > 0);
		assert(new_prob < 0x10000);
		contexts[context_index] = new_prob;

		return bit;
	}

	void reset() {
		fill(contexts.begin(), contexts.end(), 0x8000);
	}

};
