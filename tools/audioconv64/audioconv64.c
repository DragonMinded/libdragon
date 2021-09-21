#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

bool flag_verbose = false;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LE32_TO_HOST(i) (((uint32_t)((i) & 0xFF000000) >> 24) | ((uint32_t)((i) & 0x00FF0000) >>  8) | ((uint32_t)((i) & 0x0000FF00) <<  8) | ((uint32_t)((i) & 0x000000FF) << 24))
#define BE32_TO_HOST(i) (i)
#define HOST_TO_LE32(i) BE32_TO_HOST(i)
#define HOST_TO_BE32(i) (i)
#define LE16_TO_HOST(i) ((((i) & 0xFF00) >> 8)  | (((i) & 0xFF) << 8))
#define NE16_TO_HOST(i) (i)
#define HOST_TO_LE16(i) ((((i) & 0xFF00) >> 8)  | (((i) & 0xFF) << 8))
#define HOST_TO_BE16(i) (i)
#else
#define BE32_TO_HOST(i) (((uint32_t)((i) & 0xFF000000) >> 24) | ((uint32_t)((i) & 0x00FF0000) >>  8) | ((uint32_t)((i) & 0x0000FF00) <<  8) | ((uint32_t)((i) & 0x000000FF) << 24))
#define LE32_TO_HOST(i) (i)
#define HOST_TO_BE32(i) BE32_TO_HOST(i)
#define HOST_TO_LE32(i) (i)
#define BE16_TO_HOST(i) ((((i) & 0xFF00) >> 8)  | (((i) & 0xFF) << 8))
#define LE16_TO_HOST(i) (i)
#define HOST_TO_BE16(i) ((((i) & 0xFF00) >> 8)  | (((i) & 0xFF) << 8))
#define HOST_TO_LE16(i) (i)
#endif

/************************************************************************************
 *  CONVERTERS
 ************************************************************************************/

#include "conv_wav64.c"

/************************************************************************************
 *  MAIN
 ************************************************************************************/

void usage(void) {
	printf("audioconv64 -- Audio conversion tool for libdragon\n");
	printf("\n");
	printf("Usage:\n");
	printf("   audioconv64 [flags] <file-or-dir> [[flags] <file-or-dir>..]\n");
	printf("\n");
	printf("Supported conversions:\n");
	printf("   * WAV => WAV64 (Waveforms)\n");
	printf("\n");
	printf("Global options:\n");
	printf("   -o / --output <dir>       Specify output directory\n");
	printf("   -v / --verbose            Verbose mode\n");
	printf("\n");
	printf("WAV options:\n");
	printf("   --wav-loop <true|false>   Activate playback loop by default\n");
	printf("   --wav-loop-offset <N>     Set looping offset (in samples; default: 0)\n");
	printf("\n");
}

void convert(char *infn, char *outfn1) {
	char *ext = strrchr(infn, '.');
	if (!ext) {
		fprintf(stderr, "unknown file type: %s\n", infn);
		return;
	}

	char *infn_basename = strrchr(infn, '/');
	if (!infn_basename) infn_basename = infn;

	char *outfn;
	if (strcmp(ext, ".wav") == 0 || strcmp(ext, ".WAV") == 0) {
		asprintf(&outfn, "%s/%s64", outfn1, infn_basename);
		wav_convert(infn, outfn);
		free(outfn);
	} else {
		fprintf(stderr, "WARNING: ignoring unknown file: %s\n", infn);
	}
}

bool isfile(const char *path) {
	struct stat st;
	stat(path, &st);
	return (st.st_mode & S_IFREG) != 0;
}

bool isdir(const char *path) {
	struct stat st;
	stat(path, &st);
	return (st.st_mode & S_IFDIR) != 0;
}

void walkdir(char *inpath, char *outpath, void (*func)(char *, char*)) {
	if (isdir(inpath)) {
		DIR* d = opendir(inpath);
		struct dirent *de;
		while ((de = readdir(d))) {
			if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
				continue;
			char *inpathsub, *outpathsub;
			asprintf(&inpathsub, "%s/%s", inpath, de->d_name);
			asprintf(&outpathsub, "%s/%s", outpath, de->d_name);
			walkdir(inpathsub, outpathsub, func);
			free(inpathsub);
			free(outpathsub);
		}
		closedir(d);
	} else if (isfile(inpath)) {
		func(inpath, outpath);
	} else {
		fprintf(stderr, "WARNING: ignoring special file: %s\n", inpath);
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage();
		return 1;
	}

	char *outdir = ".";

	int i;
	for (i=1; i<argc; i++) {
		if (argv[i][0] == '-') {	
			if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
				flag_verbose = true;
			} else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for -o/--output\n");
					return 1;
				}
				outdir = argv[i];
			} else if (!strcmp(argv[i], "--wav-loop")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --wav-loop\n");
					return 1;
				}
				if (!strcmp(argv[i], "true") || !strcmp(argv[i], "1"))
					flag_wav_looping = true;
				else if (!strcmp(argv[i], "false") || !strcmp(argv[i], "0"))
					flag_wav_looping = true;
				else {
					fprintf(stderr, "invalid boolean argument for --wav-loop: %s\n", argv[i]);
					return 1;
				}
			} else if (!strcmp(argv[i], "--wav-loop-offset")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --wav-loop-offset\n");
					return 1;
				}
				char extra;
				if (sscanf(argv[i], "%d%c", &flag_wav_looping_offset, &extra) != 1) {
					fprintf(stderr, "invalid integer arugment for --wav-loop-offset: %s\n", argv[i]);
					return 1;
				}
				flag_wav_looping = true;
			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				return 1;
			}
		} else {
			// Positional argument. It's either a file or a directory. Convert it
			walkdir(argv[i], outdir, convert);
		}
	}

	return 0;
}
