// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Pack a data block in multiple iterations, reporting progress along the way.

*/

#pragma once

#include "RangeCoder.h"
#include "MatchFinder.h"
#include "CountingCoder.h"
#include "SizeMeasuringCoder.h"
#include "LZEncoder.h"
#include "LZParser.h"

struct PackParams {
	int iterations;
	int length_margin;
	int skip_length;
	int max_same_length;
	int max_consecutive;
	int max_edges;
};

class PackProgress : public LZProgress {
	int size;
	int steps;
	int next_step_threshold;
	int textlength;

	void print() {
		textlength = printf("[%d.%d%%]", steps / 10, steps % 10);
		fflush(stdout);
	}

	void rewind() {
		printf("\033[%dD", textlength);
	}
public:
	virtual void begin(int size) {
		this->size = size;
		steps = 0;
		next_step_threshold = size / 1000;
		print();
	}

	virtual void update(int pos) {
		if (pos < next_step_threshold) return;
		while (pos >= next_step_threshold) {
			steps += 1;
			next_step_threshold = size * (steps + 1) / 1000;
		}
		rewind();
		print();
	}

	virtual void end() {
		rewind();
		printf("\033[K");
		fflush(stdout);
	}
};

void packData(unsigned char *data, int data_length, int zero_padding, PackParams *params, Coder *result_coder) {
	MatchFinder finder(data, data_length, params->max_same_length, params->max_consecutive);
	LZParser parser(data, data_length, zero_padding, finder, params->length_margin, params->skip_length, params->max_edges);
	int real_size = 0;
	int best_size = 999999999;
	int best_result = 0;
	vector<LZParseResult> results(2);
	CountingCoder *counting_coder = new CountingCoder(LZEncoder::NUM_CONTEXTS);
	printf("%8d", data_length);
	for (int i = 0 ; i < params->iterations ; i++) {
		printf("  ");

		// Parse data into LZ symbols
		LZParseResult& result = results[1 - best_result];
		PackProgress progress;
		Coder *measurer = new SizeMeasuringCoder(counting_coder);
		finder.reset();
		result = parser.parse(LZEncoder(measurer), &progress);

		// Encode result using adaptive range coding
		vector<unsigned> dummy_result;
		RangeCoder *range_coder = new RangeCoder(LZEncoder::NUM_CONTEXTS, dummy_result);
		real_size = result.encode(LZEncoder(range_coder));
		range_coder->finish();
		delete range_coder;

		// Choose if best
		if (real_size < best_size) {
			best_result = 1 - best_result;
			best_size = real_size;
		}

		// Print size
		printf("%14.3f", real_size / (double) (8 << Coder::BIT_PRECISION));

		// Count symbol frequencies
		CountingCoder *new_counting_coder = new CountingCoder(LZEncoder::NUM_CONTEXTS);
		result.encode(LZEncoder(counting_coder));
	
		// New size measurer based on frequencies
		CountingCoder *old_counting_coder = counting_coder;
		counting_coder = new CountingCoder(old_counting_coder, new_counting_coder);
		delete old_counting_coder;
		delete new_counting_coder;
	}
	delete counting_coder;

	results[best_result].encode(LZEncoder(result_coder));
}
