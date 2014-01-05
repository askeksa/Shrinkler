// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Find repeated strings in a data block.

Matches are reported from nearest (lowest offset, highest position)
to farthest (highest offset, lowest position). A match with a
higher offset is only reported if it is at least as long as the
previous match.

Two parameters control the speed/precision tradeoff of the matcher:

The max_same_length parameter controls how many matches of the same
length are reported before the matcher skips ahead to the first longer
match.

The max_consecutive parameter controls how many matches with a distance
of one byte (i.e. in a block of same-valued bytes) are reported before
the matcher skips ahead to the earliest match in the block.

*/

#pragma once

#include <vector>

using std::vector;

class MatchFinder {
	static const int END_OF_CHAIN = -2;

	unsigned char *data;
	int length;
	int max_same_length;
	int max_consecutive;

	int current_pos;
	int match_pos;
	int longest_match;
	int consecutive;
	int same_length;
	vector<int> last_match;
	vector<int> same_match;
public:
	MatchFinder(unsigned char *data, int length, int max_same_length, int max_consecutive) :
		data(data), length(length), max_same_length(max_same_length), max_consecutive(max_consecutive) {
		reset();
	}

	void reset() {
		last_match.clear();
		last_match.resize(256 * 256, END_OF_CHAIN);
		same_match.clear();
		same_match.resize(length, END_OF_CHAIN);
		current_pos = 0;
		consecutive = 0;
		same_length = 0;
	}

	// Start finding matches between strings starting at pos and earlier strings.
	void beginMatching(int pos) {
		assert(pos >= current_pos);
		while(current_pos < pos) {
			unsigned short hash = (data[current_pos]<<8) | data[current_pos+1];
			same_match[current_pos] = last_match[hash];
			if (same_match[current_pos] == current_pos - 1) {
				if (++consecutive >= max_consecutive) {
					same_match[current_pos - max_consecutive + 1] = same_match[current_pos - max_consecutive];
				}
			} else {
				consecutive = 0;
			}
			last_match[hash] = current_pos;
			current_pos++;
		}
		unsigned short hash = (data[pos]<<8) | data[pos+1];
		match_pos = last_match[hash];
		longest_match = 0;
	}

	// Report next match. Returns whether a match was found.
	bool nextMatch(int *match_pos_out, int *match_length_out) {
		for (; match_pos != END_OF_CHAIN ; match_pos = same_match[match_pos]) {
			int max_length = length - match_pos;
			int match_length = 2;
			while (match_length < max_length && data[match_pos + match_length] == data[current_pos + match_length]) match_length++;
			if (match_length >= longest_match) {
				if (match_length == longest_match) {
					if (++same_length >= max_same_length) continue;
				} else {
					same_length = 0;
				}
				longest_match = match_length;
				*match_pos_out = match_pos;
				*match_length_out = match_length;
				match_pos = same_match[match_pos];
				return true;
			}
		}
		return false;
	}
};
