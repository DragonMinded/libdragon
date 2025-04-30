#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef __cplusplus
#define _Static_assert static_assert
#endif

#include "../common/binout.c"
#include "../common/binout.h"
#include "../common/polyfill.h"

bool flag_verbose = false;
bool flag_debug = false;

__attribute__((noreturn, format(printf, 1, 2)))
void fatal(const char *str, ...) {
	va_list va;
	va_start(va, str);
	vfprintf(stderr, str, va);
	fprintf(stderr, "\n");
	va_end(va);
	exit(1);
}

/************************************************************************************
 *  CONVERTERS
 ************************************************************************************/

#include "conv_wav64.cpp"
#include "conv_xm64.cpp"
#include "conv_ym64.cpp"

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
	printf("   * WAV/MP3 => WAV64 (Waveforms)\n");
	printf("   * XM  => XM64  (MilkyTracker, OpenMPT)\n");
	printf("   * YM  => YM64  (Arkos Tracker II)\n");
	printf("\n");
	printf("Global options:\n");
	printf("   -o / --output <dir>       	Specify output directory\n");
	printf("   -v / --verbose            	Verbose mode\n");
	printf("   -d / --debug              	Dump uncompressed files in output directory for debugging\n");
	printf("   -h / --help               	Show this help message\n");
	printf("        --help-compress      	Show detailed help for compression options\n");
	printf("\n");
	printf("WAV/MP3 options:\n");
	printf("   --wav-mono                	Force mono output\n");
	printf("   --wav-resample <N>        	Resample to a different sample rate\n");
	printf("   --wav-compress <0|1|3>    	Enable compression: 0=none, 1=vadpcm (default), 3=opus\n");
	printf("   --wav-loop <true|false>   	Activate playback loop by default\n");
	printf("   --wav-loop-offset <N>     	Set looping offset (in samples; default: 0)\n");
	printf("\n");
	printf("XM options:\n");
	printf("   --xm-8bit                 	Convert all samples to 8-bit\n");
	printf("   --xm-ext-samples <dir>    	Export samples externally as wav64 files in the specified directory\n");
	printf("   --xm-compress <0|1>          Compression level for XM samples (default: 1=vadpcm)\n");
	printf("   --xm-compress-data <0..3>    Compression level for XM binary data (default: 1)\n");
	printf("\n");
	printf("YM options:\n");
	printf("   --ym-compress <true|false>  	Compress output file\n");
	printf("\n");
}

void usage_compress(void)
{
	printf("audioconv64 -- Audio conversion tool for libdragon\n");
	printf("\n");
	printf("This help describes the compression options that can be passed to --wav-compress or --xm-compress:\n");
	printf("\n");
	printf("     none (or 0)            No compression, store raw samples\n");
	printf("     vadpcm (or 1)          Use RSP-optimized VADPCM codec. This is the default\n");
	printf("     opus (or 3)            Use RSP-optimized Opus codec. Slower at runtime, smaller disk size\n");
	printf("                            (unsupported for xm64)\n");
	printf("\n");
	printf("It is also possible to specify additional compression flags, separated by commas:\n");
	printf("\n");
	printf("     vadpcm,huffman=true    Enable Huffman compression in VADPCM.\n");
	printf("                            (default: true for wav64, false for xm64))\n");
	printf("     vadpcm,bits=<2|3|4>    Specify how many bits per sample use in VADPCM coding (default: 4)\n");
	printf("                            For values less than 4, huffman compression should be enabled.\n");
	printf("\n");
}

char* changeext(const char* fn, const char *ext) {
	char buf[4096];
	strcpy(buf, fn);
	*strrchr(buf, '.') = '\0';
	strcat(buf, ext);
	return strdup(buf);
}

void convert(const char *infn, const char *outfn1) {
	const char *ext = strrchr(infn, '.');
	if (!ext) {
		fprintf(stderr, "unknown file type: %s\n", infn);
		return;
	}

	if (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".aiff") == 0 || strcasecmp(ext, ".mp3") == 0) {
		char *outfn = changeext(outfn1, ".wav64");
		wav_convert(infn, outfn);
		free(outfn);
	} else if (strcasecmp(ext, ".xm") == 0) {
		char *outfn = changeext(outfn1, ".xm64");
		xm_convert(infn, outfn);
		free(outfn);
	} else if (strcasecmp(ext, ".ym") == 0) {
		char *outfn = changeext(outfn1, ".ym64");
		ym_convert(infn, outfn);
		free(outfn);
	} else {
		fprintf(stderr, "WARNING: ignoring unknown file: %s\n", infn);
	}
}

bool exists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
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

void walkdir(char *inpath, const char *outpath, void (*func)(const char *, const char*)) {
	if (isdir(inpath)) {
		// We're walking a directory. Make sure there's also a matching
		// output directory or create it otherwise.
		if (!isdir(outpath)) {
			// If there's an obstructing file, exit with an error.
			if (isfile(outpath)) {				
				fprintf(stderr, "ERROR: %s is a file but should be a directory\n", outpath);
				return;
			}
			mkdir(outpath, 0777);
		}
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
		if (isdir(outpath)) {
			// We support the format "audioconv64 -o <dir> <file>" as special case
			char *outpathsub;
			char *basename = strrchr(inpath, '/');
			if (!basename) basename = inpath;
			asprintf(&outpathsub, "%s/%s", outpath, basename);

			func(inpath, outpathsub);

			free(outpathsub);
		} else {
			func(inpath, outpath);
		}
	} else {
		fprintf(stderr, "WARNING: ignoring special file: %s\n", inpath);
	}
}
int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage();
		return 1;
	}

	const char *outdir = ".";

	int i;
	for (i=1; i<argc; i++) {
		if (argv[i][0] == '-') {	
			if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
				flag_verbose = true;
			} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				usage();
				return 0;
			} else if (!strcmp(argv[i], "--help-compress")) {
				usage_compress();
				return 0;
			} else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for -o/--output\n");
					return 1;
				}
				outdir = argv[i];
			} else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
				flag_debug = true;
			} else if (!strcmp(argv[i], "--wav-loop")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --wav-loop\n");
					return 1;
				}
				if (!strcmp(argv[i], "true") || !strcmp(argv[i], "1"))
					flag_wav_looping = true;
				else if (!strcmp(argv[i], "false") || !strcmp(argv[i], "0"))
					flag_wav_looping = false;
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
			} else if (!strcmp(argv[i], "--wav-mono")) {
				flag_wav_mono = true;
			} else if (!strcmp(argv[i], "--wav-compress") || !strcmp(argv[i], "--xm-compress")) {
				int *flag_compress = (!strcmp(argv[i], "--wav-compress")) ? &flag_wav_compress : &flag_xm_compress_samples;
				if (++i == argc) {
					fprintf(stderr, "missing argument for %s\n", argv[i-1]);
					return 1;
				}
				char *opts = strchr(argv[i], ',');
				if (opts) *opts++ = '\0';
				if (!strcmp(argv[i], "0") || !strcmp(argv[i], "none"))
					*flag_compress = 0;
				else if (!strcmp(argv[i], "1") || !strcmp(argv[i], "vadpcm"))
					*flag_compress = 1;
				else if (!strcmp(argv[i], "3") || !strcmp(argv[i], "opus"))
					if (flag_compress == &flag_xm_compress_samples) {
						fprintf(stderr, "opus compression not supported for XM64\n");
						return 1;
					} else
						*flag_compress = 3;
				else {
					fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
					return 1;
				}
				while (opts && *opts) {
					char *key = opts;
					char *value = strchr(opts, '=');
					if (!value) {
						fprintf(stderr, "invalid option for %s: %s\n", argv[i-1], opts);
						return 1;
					}
					*value = '\0';
					value++;
					opts = strchr(value, ',');
					if (opts) {
						*opts = '\0';
						opts++;
					}
					if (!strcmp(key, "huffman")) {
						if (*flag_compress != 1) {
							fprintf(stderr, "compression option 'huffman' only allowed for VADPCM (%s 1)\n", argv[i-1]);
							return 1;
						}
						if (!strcmp(value, "true") || !strcmp(value, "1"))
							flag_wav_compress_vadpcm_huffman = true;
						else if (!strcmp(value, "false") || !strcmp(value, "0"))
							flag_wav_compress_vadpcm_huffman = false;
						else {
							fprintf(stderr, "invalid value for compression option 'huffman': %s\n", value);
							return 1;
						}
					} else if (!strcmp(key, "bits")) {
						if (*flag_compress != 1) {
							fprintf(stderr, "compression option 'bits' only allowed for VADPCM (%s 1)\n", argv[i-1]);
							return 1;
						}
						flag_wav_compress_vadpcm_bits = atoi(value);
						if (flag_wav_compress_vadpcm_bits < 2 || flag_wav_compress_vadpcm_bits > 4) {
							fprintf(stderr, "invalid value for compression option 'bits': %s\n", value);
							return 1;
						}
					} else {
						fprintf(stderr, "invalid option for %s: %s\n", key, argv[i-1]);
						return 1;
					}
				}
			} else if (!strcmp(argv[i], "--wav-resample")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --wav-resample\n");
					return 1;
				}
				flag_wav_resample = atoi(argv[i]);
				if (flag_wav_resample < 1 || flag_wav_resample > 48000) {
					fprintf(stderr, "invalid argument for --wav-resample: %s\n", argv[i]);
					return 1;
				}
			} else if (!strcmp(argv[i], "--xm-8bit")) {
				flag_xm_8bit = true;
			} else if (!strcmp(argv[i], "--xm-ext-samples")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --xm-ext-samples\n");
					return 1;
				}
				flag_xm_extsampledir = argv[i];
				mkdir(flag_xm_extsampledir, 0777);
			} else if (!strcmp(argv[i], "--xm-compress-data")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --xm-compress\n");
					return 1;
				}
				flag_xm_compress_meta = atoi(argv[i]);
				if (flag_xm_compress_meta < 0 || flag_xm_compress_meta > MAX_COMPRESSION) {
					fprintf(stderr, "invalid argument for --xm-compress: %s\n", argv[i]);
					return 1;
				}
			} else if (!strcmp(argv[i], "--ym-compress")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --ym-compress\n");
					return 1;
				}
				if (!strcmp(argv[i], "true") || !strcmp(argv[i], "1"))
					flag_ym_compress = true;
				else if (!strcmp(argv[i], "false") || !strcmp(argv[i], "0"))
					flag_ym_compress = false;
				else {
					fprintf(stderr, "invalid boolean argument for --ym-compress: %s\n", argv[i]);
					return 1;
				}
			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				return 1;
			}
		} else {
			// Positional argument. It's either a file or a directory. Convert it
			if (!exists(argv[i])) {
				fprintf(stderr, "ERROR: file %s does not exist\n", argv[i]);
			} else {
				walkdir(argv[i], outdir, convert);
			}
		}
	}

	return 0;
}
