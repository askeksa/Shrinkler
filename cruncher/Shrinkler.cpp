// Copyright 1999-2015 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Main file for the cruncher.

*/

//#define SHRINKLER_TITLE ("Shrinkler executable file compressor by Blueberry - version 4.4 (2015-01-18)\n\n")

#ifndef SHRINKLER_TITLE
#define SHRINKLER_TITLE ("Shrinkler executable file compressor by Blueberry - development version (built " __DATE__ " " __TIME__ ")\n\n")
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

using std::string;

#include "HunkFile.h"
#include "DataFile.h"

void usage() {
	printf("Usage: Shrinkler <options> <input executable> <output executable>\n");
	printf("\n");
	printf("Available options are (default values in parentheses):\n");
	printf(" -d, --data           Treat input as raw data, rather than executable\n");
	printf(" -h, --hunkmerge      Merge hunks of the same memory type\n");
	printf(" -o, --overlap        Overlap compressed and decompressed data to save memory\n");
	printf(" -m, --mini           Use a smaller, but more restricted decrunch header\n");
	printf(" -i, --iterations     Number of iterations for the compression (2)\n");
	printf(" -l, --length-margin  Number of shorter matches considered for each match (2)\n");
	printf(" -a, --same-length    Number of matches of the same length to consider (20)\n");
	printf(" -e, --effort         Perseverance in finding multiple matches (200)\n");
	printf(" -s, --skip-length    Minimum match length to accept greedily (2000)\n");
	printf(" -r, --references     Number of reference edges to keep in memory (100000)\n");
	printf(" -t, --text           Print a text, followed by a newline, before decrunching\n");
	printf(" -T, --textfile       Print the contents of the given file before decrunching\n");
	printf(" -f, --flash          Poke into a register (e.g. DFF180) during decrunching\n");
	printf(" -p, --no-progress    Do not print progress info: no ANSI codes in output\n");
	printf(" -u, --only-hunkmerge Do not compress, only merge hunks of the same memory type\n");
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
					if (i+1 < argc && !consumed[i+1] && argv[i+1][0] != '-') {
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

	FlagParameter   data           ("-d", "--data",                                 argc, argv, consumed);
	FlagParameter   hunkmerge      ("-h", "--hunkmerge",                            argc, argv, consumed);
	FlagParameter   overlap        ("-o", "--overlap",                              argc, argv, consumed);
	FlagParameter   mini           ("-m", "--mini",                                 argc, argv, consumed);
	IntParameter    iterations     ("-i", "--iterations",      1,        9,      2, argc, argv, consumed);
	IntParameter    length_margin  ("-l", "--length-margin",   0,      100,      2, argc, argv, consumed);
	IntParameter    same_length    ("-a", "--same-length",     1,   100000,     20, argc, argv, consumed);
	IntParameter    effort         ("-e", "--effort",          0,   100000,    200, argc, argv, consumed);
	IntParameter    skip_length    ("-s", "--skip-length",     2,   100000,   2000, argc, argv, consumed);
	IntParameter    references     ("-r", "--references",   1000, 10000000, 100000, argc, argv, consumed);
	StringParameter text           ("-t", "--text",                                 argc, argv, consumed);
	StringParameter textfile       ("-T", "--textfile",                             argc, argv, consumed);
	HexParameter    flash          ("-f", "--flash",                             0, argc, argv, consumed);
	FlagParameter   no_progress    ("-p", "--no-progress",                          argc, argv, consumed);
	FlagParameter   only_hunkmerge ("-u", "--only-hunkmerge",                       argc, argv, consumed);

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

	if (data.seen && (hunkmerge.seen || overlap.seen || mini.seen || text.seen || textfile.seen || flash.seen)) {
		printf("Error: The data option cannot be used together with any of the\n");
		printf("hunkmerge, overlap, mini, text, textfile or flash options.\n\n");
		usage();
	}

	if (only_hunkmerge.seen && (data.seen || hunkmerge.seen || overlap.seen || mini.seen || iterations.seen || length_margin.seen || same_length.seen || effort.seen || skip_length.seen || references.seen || text.seen || textfile.seen || flash.seen || no_progress.seen)) {
		printf("Error: The only-hunkmerge option cannot be used together with any other option\n\n");
		usage();
	}

	if (overlap.seen && mini.seen) {
		printf("Error: The overlap and mini options cannot be used together.\n\n");
		usage();
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
	params.match_patience = effort.value;
	params.max_same_length = same_length.value;

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
			exit(1);
		}
		char c;
		while ((c = fgetc(decrunch_text_file)) != EOF) {
			decrunch_text.push_back(c);
		}
		fclose(decrunch_text_file);
		decrunch_text_ptr = &decrunch_text;
	}

	if (data.seen) {
		// Data file compression
		printf("Loading file %s...\n\n", infile);
		DataFile *orig = new DataFile;
		orig->load(infile);

		printf("Crunching...\n\n");
		RefEdgeFactory edge_factory(references.value);
		DataFile *crunched = orig->crunch(&params, &edge_factory, !no_progress.seen);
		delete orig;
		printf("References considered:%8d\n",  edge_factory.max_edge_count);
		printf("References discarded:%9d\n\n", edge_factory.max_cleaned_edges);

		printf("Saving file %s...\n\n", outfile);
		crunched->save(outfile);

		printf("Final file size: %d\n\n", crunched->size());
		delete crunched;

		if (edge_factory.max_edge_count > references.value) {
			printf("Note: compression may benefit from a larger reference buffer (-r option).\n\n");
		}

		return 0;
	}

	// Executable file compression
	printf("Loading file %s...\n\n", infile);
	HunkFile *orig = new HunkFile;
	orig->load(infile);
	if (!orig->analyze()) {
		printf("\nError while analyzing input file!\n\n");
		delete orig;
		exit(1);
	}
	if (hunkmerge.seen || only_hunkmerge.seen) {
		printf("Merging hunks...\n\n");
		HunkFile *merged = orig->merge_hunks(orig->merged_hunklist());
		delete orig;
		if (!merged->analyze()) {
			printf("\nError while analyzing merged file!\n\n");
			delete merged;
			internal_error();
		}

		if (only_hunkmerge.seen) {
			printf("Saving file %s...\n\n", outfile);
			merged->save(outfile);
			#ifdef S_IRWXU // Is the POSIX file permission API available?
				chmod(outfile, 0755); // Mark file executable
			#endif

			printf("Final file size: %d\n\n", merged->size());

			return 0;
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
	int orig_mem = orig->memory_usage(true);
	printf("Crunching...\n\n");
	RefEdgeFactory edge_factory(references.value);
	HunkFile *crunched = orig->crunch(&params, overlap.seen, mini.seen, decrunch_text_ptr, flash.value, &edge_factory, !no_progress.seen);
	delete orig;
	printf("References considered:%8d\n",  edge_factory.max_edge_count);
	printf("References discarded:%9d\n\n", edge_factory.max_cleaned_edges);
	if (!crunched->analyze()) {
		printf("\nError while analyzing crunched file!\n\n");
		delete crunched;
		internal_error();
	}
	int crunched_mem_during = crunched->memory_usage(true);
	int crunched_mem_after = crunched->memory_usage(mini.seen || overlap.seen);

	printf("Memory overhead during decrunching:  %9d\n",   crunched_mem_during - orig_mem);
	printf("Memory overhead after decrunching:   %9d\n\n", crunched_mem_after - orig_mem);

	printf("Saving file %s...\n\n", outfile);
	crunched->save(outfile);
#ifdef S_IRWXU // Is the POSIX file permission API available?
	chmod(outfile, 0755); // Mark file executable
#endif

	printf("Final file size: %d\n\n", crunched->size());
	delete crunched;

	if (edge_factory.max_edge_count > references.value) {
		printf("Note: compression may benefit from a larger reference buffer (-r option).\n\n");
	}

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
