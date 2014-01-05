// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Main file for the cruncher.

*/

#include <cstdio>
#include <cstdlib>

#include "HunkFile.h"

void usage() {
	printf("Usage: Shrinkler <options> <input executable> <output executable>\n");
	printf("\n");
	printf("Available options are:\n");
	printf(" -h, --hunkmerge      Merge hunks of the same memory type\n");
	printf(" -m, --mini           Use a smaller, but more restricted decrunch header\n");
	printf(" -i, --iterations     Number of iterations for the compression (default 2)\n");
	printf(" -l, --length-margin  Number of shorter matches considered for each match (default 0)\n");
	printf(" -s, --skip-length    Minimum match length to accept greedily (default 1000)\n");
	printf(" -c, --consecutive    Number of match positions to consider in same-valued blocks (default 20)\n");
	printf(" -a, --same-length    Number of matches of the same length to consider (default 20)\n");
	printf(" -r, --references     Number of references to keep track of during LZ parsing (default 10000)\n");
	printf("\n");
	exit(0);
}

class IntParameter {
public:
	int value;

	IntParameter(const char *form1, const char *form2, int min_value, int max_value, int default_value,
	             int argc, const char *argv[], vector<bool>& consumed) {
		value = default_value;
		bool parsed = false;
		for (int i = 1 ; i < argc ; i++) {
			if (strcmp(argv[i], form1) == 0 || strcmp(argv[i], form2) == 0) {
				if (parsed) {
					printf("Error: %s specified multiple times.\n\n", argv[i]);
					usage();
				}
				if (i+1 < argc) {
					const char *param = argv[i+1];
					char *endptr;
					value = strtol(param, &endptr, 10);
					if (endptr == &param[strlen(param)]) {
						if (value < min_value || value > max_value) {
							printf("Error: Argument of %s must be between %d and %d.\n\n", argv[i], min_value, max_value);
							usage();
						}
						parsed = true;
					}
				}
				if (!parsed) {
					printf("Error: %s requires a numeric argument.\n\n", argv[i]);
					usage();
				}
				consumed[i] = true;
				consumed[i+1] = true;
				i = i+1;
			}
		}
	}
};

class FlagParameter {
public:
	bool value;

	FlagParameter(const char *form1, const char *form2, int argc, const char *argv[], vector<bool>& consumed) {
		value = false;
		for (int i = 1 ; i < argc ; i++) {
			if (strcmp(argv[i], form1) == 0 || strcmp(argv[i], form2) == 0) {
				if (value) {
					printf("Error: %s specified multiple times.\n\n", argv[i]);
					usage();
				}
				value = true;
				consumed[i] = true;
			}
		}
	}
};

int main(int argc, const char *argv[]) {
	printf("Shrinkler executable file compressor by Blueberry - version 4.0 (2014-01-05)\n\n");

	vector<bool> consumed(argc);

	FlagParameter hunkmerge     ("-h", "--hunkmerge",                            argc, argv, consumed);
	FlagParameter mini          ("-m", "--mini",                                 argc, argv, consumed);
	IntParameter  iterations    ("-i", "--iterations",      1,        9,      2, argc, argv, consumed);
	IntParameter  length_margin ("-l", "--length-margin",   0,      100,      0, argc, argv, consumed);
	IntParameter  skip_length   ("-s", "--skip-length",     2,   100000,    200, argc, argv, consumed);
	IntParameter  consecutive   ("-c", "--consecutive",     1,   100000,     20, argc, argv, consumed);
	IntParameter  same_length   ("-a", "--same-length",     1,   100000,     20, argc, argv, consumed);
	IntParameter  references    ("-r", "--references",  10000, 10000000, 100000, argc, argv, consumed);

	vector<const char*> files;

	for (int i = 1 ; i < argc ; i++) {
		if (!consumed[i]) {
			if (argv[i][0] == '-') {
				printf("Error: Unknown option %s\n\n", argv[i]);
				usage();
			}
			files.push_back(argv[i]);
		}
	}

	if (files.size() == 0) {
		printf("Error: No input file specified.\n\n");
		usage();
	}
	if (files.size() == 1) {
		printf("Error: No output file specified.\n\n");
		usage();
	}
	if (files.size() > 2) {
		printf("Error: Too many files specified.\n\n");
		usage();
	}

	const char *infile = files[0];
	const char *outfile = files[1];

	PackParams params;
	params.iterations = iterations.value;
	params.length_margin = length_margin.value;
	params.skip_length = skip_length.value;
	params.max_same_length = same_length.value;
	params.max_consecutive = consecutive.value;
	params.max_edges = references.value;

	printf("Loading file %s...\n\n", infile);
	HunkFile *orig = new HunkFile;
	orig->load(infile);
	if (!orig->analyze()) {
		printf("\nError while analyzing input file!\n\n");
		delete orig;
		exit(1);
	}
	if (hunkmerge.value) {
		printf("Merging hunks...\n\n");
		HunkFile *merged = orig->merge_hunks(orig->merged_hunklist());
		delete orig;
		if (!merged->analyze()) {
			printf("\nError while analyzing merged file!\n\n");
			delete merged;
			exit(1);
		}
		orig = merged;
	}
	if (mini.value && !orig->valid_mini()) {
		printf("Input executable not suitable for mini crunching.\n"
		       "Must contain only one non-empty hunk and no relocations,\n"
		       "and the final file size must be less than 24k.\n\n");
		delete orig;
		exit(1);
	}
	printf("Crunching...\n\n");
	HunkFile *crunched = orig->crunch(&params, mini.value);
	delete orig;
	printf("References considered: %d\nReferences discarded:  %d\n\n", RefEdge::max_edge_count, RefEdge::edges_cleaned);
	if (!crunched->analyze()) {
		printf("\nError while analyzing crunched file!\n\n");
		delete crunched;
	}

	printf("Saving file %s...\n\n", outfile);
	crunched->save(outfile);

	printf("Final file size: %d\n\n", crunched->size());
	delete crunched;

	return 0;
}
