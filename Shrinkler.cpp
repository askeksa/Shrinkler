// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Main file for the cruncher.

*/

//#define SHRINKLER_TITLE ("Shrinkler executable file compressor by Blueberry - version 4.1 (2014-02-08)\n\n")

#ifndef SHRINKLER_TITLE
#define SHRINKLER_TITLE ("Shrinkler executable file compressor by Blueberry - development version (built " __DATE__ " " __TIME__ ")\n\n")
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

using std::string;

#include "HunkFile.h"

void usage() {
	printf("Usage: Shrinkler <options> <input executable> <output executable>\n");
	printf("\n");
	printf("Available options are:\n");
	printf(" -h, --hunkmerge      Merge hunks of the same memory type\n");
	printf(" -m, --mini           Use a smaller, but more restricted decrunch header\n");
	printf(" -i, --iterations     Number of iterations for the compression (default 2)\n");
	printf(" -l, --length-margin  Number of shorter matches considered for each match (default 0)\n");
	printf(" -s, --skip-length    Minimum match length to accept greedily (default 200)\n");
	printf(" -c, --consecutive    Number of match positions to consider in same-valued blocks (default 20)\n");
	printf(" -a, --same-length    Number of matches of the same length to consider (default 20)\n");
	printf(" -r, --references     Number of references to keep track of during LZ parsing (default 10000)\n");
	printf(" -t, --text           Print the given text, followed by a newline, before decrunching\n");
	printf(" -T, --textfile       Print the contents of the given file before decrunching\n");
	printf(" -f, --flash          Poke stuff into the given address (e.g. DFF180) during decrunching\n");
	printf("\n");
	exit(0);
}

class Parameter {
public:
	bool seen;

	virtual ~Parameter() {}
protected:
	void parse(const char *form1, const char *form2, const char *arg_kind, int argc, const char *argv[], vector<bool>& consumed) {
		seen = false;
		for (int i = 1 ; i < argc ; i++) {
			if (strcmp(argv[i], form1) == 0 || strcmp(argv[i], form2) == 0) {
				if (seen) {
					printf("Error: %s specified multiple times.\n\n", argv[i]);
					usage();
				}
				consumed[i] = true;
				if (arg_kind) {
					if (i+1 < argc) {
						seen = parseArg(argv[i], argv[i+1]);
					}
					if (!seen) {
						printf("Error: %s requires a %s argument.\n\n", argv[i], arg_kind);
						usage();
					}
					consumed[i+1] = true;
					i = i+1;
				} else {
					seen = true;
				}
			}
		}
	}

	virtual bool parseArg(const char *param, const char *arg) = 0;
};

class IntParameter : public Parameter {
	int min_value;
	int max_value;
public:
	int value;

	IntParameter(const char *form1, const char *form2, int min_value, int max_value, int default_value,
	             int argc, const char *argv[], vector<bool>& consumed)
		: min_value(min_value), max_value(max_value), value(default_value)
    {
		parse(form1, form2, "numeric", argc, argv, consumed);
    }

protected:
	virtual bool parseArg(const char *param, const char *arg) {
		char *endptr;
		value = strtol(arg, &endptr, 10);
		if (endptr == &arg[strlen(arg)]) {
			if (value < min_value || value > max_value) {
				printf("Error: Argument of %s must be between %d and %d.\n\n", param, min_value, max_value);
				usage();
			}
			return true;
		}
		return false;
	}
};

class HexParameter : public Parameter {
public:
	unsigned value;

	HexParameter(const char *form1, const char *form2, int default_value,
	             int argc, const char *argv[], vector<bool>& consumed)
		: value(default_value)
    {
		parse(form1, form2, "hexadecimal", argc, argv, consumed);
    }

protected:
	virtual bool parseArg(const char *param, const char *arg) {
		char *endptr;
		value = strtol(arg, &endptr, 16);
		if (endptr == &arg[strlen(arg)]) {
			return true;
		}
		return false;
	}
};

class StringParameter : public Parameter {
public:
	const char *value;

	StringParameter(const char *form1, const char *form2, int argc, const char *argv[], vector<bool>& consumed)
		: value(NULL)
    {
		parse(form1, form2, "string", argc, argv, consumed);
    }

protected:
	virtual bool parseArg(const char *param, const char *arg) {
		value = arg;
		return true;
	}
};

class FlagParameter : public Parameter {
public:
	FlagParameter(const char *form1, const char *form2, int argc, const char *argv[], vector<bool>& consumed)
	{
		parse(form1, form2, NULL, argc, argv, consumed);
	}

protected:
	virtual bool parseArg(const char *param, const char *arg) {
		// Not used
		return true;
	}
};

int main2(int argc, const char *argv[]) {
	printf(SHRINKLER_TITLE);

	vector<bool> consumed(argc);

	FlagParameter   hunkmerge     ("-h", "--hunkmerge",                            argc, argv, consumed);
	FlagParameter   mini          ("-m", "--mini",                                 argc, argv, consumed);
	IntParameter    iterations    ("-i", "--iterations",      1,        9,      2, argc, argv, consumed);
	IntParameter    length_margin ("-l", "--length-margin",   0,      100,      0, argc, argv, consumed);
	IntParameter    skip_length   ("-s", "--skip-length",     2,   100000,    200, argc, argv, consumed);
	IntParameter    consecutive   ("-c", "--consecutive",     1,   100000,     20, argc, argv, consumed);
	IntParameter    same_length   ("-a", "--same-length",     1,   100000,     20, argc, argv, consumed);
	IntParameter    references    ("-r", "--references",  10000, 10000000, 100000, argc, argv, consumed);
	StringParameter text          ("-t", "--text",                                 argc, argv, consumed);
	StringParameter textfile      ("-T", "--textfile",                             argc, argv, consumed);
	HexParameter    flash         ("-f", "--flash",                             0, argc, argv, consumed);

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

	if (text.seen && textfile.seen) {
		printf("Error: The text and textfile options cannot both be specified.\n\n");
		usage();
	}

	if (mini.seen && (text.seen || textfile.seen)) {
		printf("Error: The text and textfile options cannot be used in mini mode.\n\n");
		usage();
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

	string *decrunch_text_ptr = NULL;
	string decrunch_text;
	if (text.seen) {
		decrunch_text = text.value;
		decrunch_text.push_back('\n');
		decrunch_text_ptr = &decrunch_text;
	} else if (textfile.seen) {
		FILE *decrunch_text_file = fopen(textfile.value, "r");
		if (!decrunch_text_file) {
			printf("Error: Could not open text file %s\n", textfile.value);
		}
		char c;
		while ((c = fgetc(decrunch_text_file)) != EOF) {
			decrunch_text.push_back(c);
		}
		fclose(decrunch_text_file);
		decrunch_text_ptr = &decrunch_text;
	}

	printf("Loading file %s...\n\n", infile);
	HunkFile *orig = new HunkFile;
	orig->load(infile);
	if (!orig->analyze()) {
		printf("\nError while analyzing input file!\n\n");
		delete orig;
		exit(1);
	}
	if (hunkmerge.seen) {
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
	if (mini.seen && !orig->valid_mini()) {
		printf("Input executable not suitable for mini crunching.\n"
		       "Must contain only one non-empty hunk and no relocations,\n"
		       "and the final file size must be less than 24k.\n\n");
		delete orig;
		exit(1);
	}
	printf("Crunching...\n\n");
	HunkFile *crunched = orig->crunch(&params, mini.seen, decrunch_text_ptr, flash.value);
	delete orig;
	printf("References considered: %d\nReferences discarded:  %d\n\n", RefEdge::max_edge_count, RefEdge::edges_cleaned);
	if (!crunched->analyze()) {
		printf("\nError while analyzing crunched file!\n\n");
		delete crunched;
	}

	printf("Saving file %s...\n\n", outfile);
	crunched->save(outfile);
	chmod(outfile, 0755); // Mark file executable

	printf("Final file size: %d\n\n", crunched->size());
	delete crunched;

	return 0;
}

int main(int argc, const char *argv[]) {
	try {
		return main2(argc, argv);
	} catch (std::bad_alloc& e) {
		fflush(stdout);
		fprintf(stderr,
			"\n\nShrinkler ran out of memory.\n\n"
			"Some things you can try:\n"
			" - Free up some memory\n"
			" - Run it on a machine with more memory\n"
			" - Reduce the size of the reference buffer (-r option)\n"
			" - Split up your biggest hunk into smaller ones\n\n");
		fflush(stderr);
		return 1;
	}
}
