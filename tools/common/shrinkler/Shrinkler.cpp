// Copyright 1999-2022 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Main file for the cruncher.

*/

//#define SHRINKLER_TITLE ("Shrinkler executable file compressor by Blueberry - version 4.7 (2022-02-22)\n\n")

#ifndef SHRINKLER_TITLE
#define SHRINKLER_TITLE ("Shrinkler executable file compressor by Blueberry - development version (built " __DATE__ " " __TIME__ ")\n\n")
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

using std::string;

#include "DataFile.h"

__attribute__((noreturn)) static
void usage() {
	printf("Usage: Shrinkler <options> <input executable> <output executable>\n");
	printf("\n");
	printf("Available options are (default values in parentheses):\n");
	printf(" -d, --data           Treat input as raw data, rather than executable\n");
	printf(" -b, --bytes          Disable parity context - better on byte-oriented data\n");
	printf(" -w, --header         Write data file header for easier loading\n");
	printf(" -h, --hunkmerge      Merge hunks of the same memory type\n");
	printf(" -u, --no-crunch      Process hunks without crunching\n");
	printf(" -o, --overlap        Overlap compressed and decompressed data to save memory\n");
	printf(" -m, --mini           Use a smaller, but more restricted decrunch header\n");
	printf(" -c, --commandline    Support passing commandline arguments to the program\n");
	printf(" -1, ..., -9          Presets for all compression options (-3)\n");
	printf(" -i, --iterations     Number of iterations for the compression (3)\n");
	printf(" -l, --length-margin  Number of shorter matches considered for each match (3)\n");
	printf(" -a, --same-length    Number of matches of the same length to consider (30)\n");
	printf(" -e, --effort         Perseverance in finding multiple matches (300)\n");
	printf(" -s, --skip-length    Minimum match length to accept greedily (3000)\n");
	printf(" -r, --references     Number of reference edges to keep in memory (100000)\n");
	printf(" -t, --text           Print a text, followed by a newline, before decrunching\n");
	printf(" -T, --textfile       Print the contents of the given file before decrunching\n");
	printf(" -f, --flash          Poke into a register (e.g. DFF180) during decrunching\n");
	printf(" -p, --no-progress    Do not print progress info: no ANSI codes in output\n");
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

class DigitParameter : public Parameter {
public:
	int value;

	DigitParameter(int default_value, int argc, const char *argv[], vector<bool>& consumed)
	: value(default_value)
	{
		seen = false;
		for (int i = 1 ; i < argc ; i++) {
			const char *a = argv[i];
			if (strlen(a) == 2 && a[0] == '-' && a[1] >= '0' && a[1] <= '9') {
				if (seen) {
					printf("Error: Numeric parameter specified multiple times.\n\n");
					usage();
				}
				consumed[i] = true;
				value = a[1] - '0';
				seen = true;
			}
		}
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

	DigitParameter  preset        (                                             3, argc, argv, consumed);
	int p = preset.value;

	FlagParameter   data          ("-d", "--data",                                 argc, argv, consumed);
	FlagParameter   bytes         ("-b", "--bytes",                                argc, argv, consumed);
	FlagParameter   header        ("-w", "--header",                               argc, argv, consumed);
	FlagParameter   hunkmerge     ("-h", "--hunkmerge",                            argc, argv, consumed);
	FlagParameter   no_crunch     ("-u", "--no-crunch",                            argc, argv, consumed);
	FlagParameter   overlap       ("-o", "--overlap",                              argc, argv, consumed);
	FlagParameter   mini          ("-m", "--mini",                                 argc, argv, consumed);
	FlagParameter   commandline   ("-c", "--commandline",                          argc, argv, consumed);
	IntParameter    iterations    ("-i", "--iterations",      1,        9,    1*p, argc, argv, consumed);
	IntParameter    length_margin ("-l", "--length-margin",   0,      100,    1*p, argc, argv, consumed);
	IntParameter    same_length   ("-a", "--same-length",     1,   100000,   10*p, argc, argv, consumed);
	IntParameter    effort        ("-e", "--effort",          0,   100000,  100*p, argc, argv, consumed);
	IntParameter    skip_length   ("-s", "--skip-length",     2,   100000, 1000*p, argc, argv, consumed);
	IntParameter    references    ("-r", "--references",   1000,100000000, 100000, argc, argv, consumed);
	StringParameter text          ("-t", "--text",                                 argc, argv, consumed);
	StringParameter textfile      ("-T", "--textfile",                             argc, argv, consumed);
	HexParameter    flash         ("-f", "--flash",                             0, argc, argv, consumed);
	FlagParameter   no_progress   ("-p", "--no-progress",                          argc, argv, consumed);

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

	if (data.seen && (commandline.seen || hunkmerge.seen || overlap.seen || mini.seen || text.seen || textfile.seen || flash.seen)) {
		printf("Error: The data option cannot be used together with any of the\n");
		printf("commandline, hunkmerge, overlap, mini, text, textfile or flash options.\n\n");
		usage();
	}

	if (bytes.seen && !data.seen) {
		printf("Error: The bytes option can only be used together with the data option.\n\n");
		usage();
	}

	if (header.seen && !data.seen) {
		printf("Error: The header option can only be used together with the data option.\n\n");
		usage();
	}

	if (no_crunch.seen && (data.seen || overlap.seen || mini.seen || preset.seen || iterations.seen || length_margin.seen || same_length.seen || effort.seen || skip_length.seen || references.seen || text.seen || textfile.seen || flash.seen)) {
		printf("Error: The no-crunch option cannot be used together with any of the\n");
		printf("crunching options.\n\n");
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

	//rasky: this version only works in data mode
	if (!data.seen) {
		printf("Error: Only data mode is supported in this version.\n\n");
		usage();
	}

	const char *infile = files[0];
	const char *outfile = files[1];

	PackParams params;
	params.parity_context = !bytes.seen;
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
	crunched->save(outfile, header.seen);

	printf("Final file size: %d\n\n", crunched->size(header.seen));
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
