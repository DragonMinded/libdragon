/*
    audioconv64: convert audio files to the format used by the Libdragon SDK
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
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

#include "../common/binout.h"
#include "../common/polyfill.h"
#include "../common/assetcomp.h"
#include "audioconv64.h"

int flag_verbose = 0;
bool flag_debug = false;
static bool had_error = false;

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
	printf("   * SF2 => SF64  (SoundFont 2)\n");
	printf("   * MID => MID64 (Standard MIDI File)\n");
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
	printf("   --wav-compress <0|1|2|3>  	Enable compression: 0=none, 1=vadpcm (default), 2=ulc, 3=opus\n");
	printf("   --wav-loop <true|false>   	Activate playback loop by default\n");
	printf("   --wav-loop-offset <N>     	Set looping offset (in samples; default: 0)\n");
	printf("   --wav-seek <SEC|FILE>     	Enable seeking support:\n");
	printf("                             	- if SEC is a float, add a seekpoint every SEC seconds\n");
	printf("                             	- if FILE, read a list of seekpoints (one per line):\n");
	printf("                             	  * integer sample offsets, or\n");
	printf("                             	  * timestamps in [hh:]mm:ss[.mmm] format\n");
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
	printf("SF2 options:\n");
	printf("   --sf-compress <0|1>          Compression for SF samples (default: 1=vadpcm)\n");
	printf("\n");
	printf("MID options:\n");
	printf("   --mid-compress <0..3>        Asset compression level for MID64 (default: 1)\n");
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
	printf("     ulc (or 2)             Use RSP-optimized ULC codec. A simple and fast transform codec.\n");
	printf("                            Worse compression than Opus, but better runtime performance.\n");
	printf("     opus (or 3)            Use RSP-optimized Opus codec. Slower at runtime, smaller disk size\n");
	printf("                            (unsupported for xm64)\n");
	printf("\n");
	printf("It is also possible to specify additional compression flags, separated by commas:\n");
	printf("\n");
	printf("     vadpcm,huffman=true    Enable Huffman compression in VADPCM.\n");
	printf("                            (default: true for wav64, false for xm64))\n");
	printf("     vadpcm,bits=<2|3|4>    Specify how many bits per sample use in VADPCM coding (default: 4)\n");
	printf("                            For values less than 4, huffman compression should be enabled.\n");
	printf("     ulc,mode=<vbr|abr|cbr> Select ULC rate-control mode (default: vbr)\n");
	printf("     ulc,quality=<1..100>   Set ULC VBR quality (default: 50)\n");
	printf("     ulc,bitrate=<kbps>     Set ULC ABR/CBR bitrate (default: 64)\n");
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
		had_error = true;
		return;
	}

	if (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".aiff") == 0 || strcasecmp(ext, ".mp3") == 0) {
		char *outfn = changeext(outfn1, ".wav64");
		if (wav_convert(infn, outfn) != 0) had_error = true;
		free(outfn);
	} else if (strcasecmp(ext, ".xm") == 0) {
		char *outfn = changeext(outfn1, ".xm64");
		if (xm_convert(infn, outfn) != 0) had_error = true;
		free(outfn);
	} else if (strcasecmp(ext, ".ym") == 0) {
		char *outfn = changeext(outfn1, ".ym64");
		if (ym_convert(infn, outfn) != 0) had_error = true;
		free(outfn);
	} else if (strcasecmp(ext, ".sf2") == 0) {
		char *outfn = changeext(outfn1, ".sf64");
		if (sf_convert(infn, outfn) != 0) had_error = true;
		free(outfn);
	} else if (strcasecmp(ext, ".mid") == 0 || strcasecmp(ext, ".midi") == 0) {
		char *outfn = changeext(outfn1, ".mid64");
		if (mid_convert(infn, outfn) != 0) had_error = true;
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
				had_error = true;
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
	winconsole_utf8();

	if (argc < 2) {
		usage();
		return 1;
	}

	const char *outdir = ".";

	int i;
	for (i=1; i<argc; i++) {
		if (argv[i][0] == '-') {	
			if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
				flag_verbose++;
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
				else if (!strcmp(argv[i], "2") || !strcmp(argv[i], "ulc"))
					if (flag_compress == &flag_xm_compress_samples) {
						fprintf(stderr, "ulc compression not supported for XM64\n");
						return 1;
					} else
						*flag_compress = 2;
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
					} else if (!strcmp(key, "mode")) {
						if (*flag_compress != 2) {
							fprintf(stderr, "compression option 'mode' only allowed for ULC (%s ulc)\n", argv[i-1]);
							return 1;
						}
						if (!strcmp(value, "vbr")) flag_wav_compress_ulc_mode = ULC_MODE_VBR;
						else if (!strcmp(value, "abr")) flag_wav_compress_ulc_mode = ULC_MODE_ABR;
						else if (!strcmp(value, "cbr")) flag_wav_compress_ulc_mode = ULC_MODE_CBR;
						else {
							fprintf(stderr, "invalid value for ULC compression option 'mode': %s\n", value);
							return 1;
						}
					} else if (!strcmp(key, "quality")) {
						char extra;
						if (*flag_compress != 2 || sscanf(value, "%f%c", &flag_wav_compress_ulc_quality, &extra) != 1 ||
							flag_wav_compress_ulc_quality < 1.0f || flag_wav_compress_ulc_quality > 100.0f) {
							fprintf(stderr, "invalid ULC quality (expected 1..100): %s\n", value);
							return 1;
						}
					} else if (!strcmp(key, "bitrate")) {
						char extra;
						if (*flag_compress != 2 || sscanf(value, "%f%c", &flag_wav_compress_ulc_bitrate, &extra) != 1 ||
							flag_wav_compress_ulc_bitrate <= 0.0f) {
							fprintf(stderr, "invalid ULC bitrate: %s\n", value);
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
			} else if (!strcmp(argv[i], "--wav-seek")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --wav-seek\n");
					return 1;
				}
				const char *param = argv[i];
				char *end = NULL;
				double sec = strtod(param, &end);
				if (end != param && *end == '\0' && sec > 0.0) {
					flag_wav_seek_interval_sec = sec;
				} else {
					// Defer parsing until after resampling so timestamps can be converted using the final sample rate.
					flag_wav_seek_file = param;
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
			} else if (!strcmp(argv[i], "--sf-compress")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --sf-compress\n");
					return 1;
				}
				if (!strcmp(argv[i], "0") || !strcmp(argv[i], "none"))
					flag_sf_compress = 0;
				else if (!strcmp(argv[i], "1") || !strcmp(argv[i], "vadpcm"))
					flag_sf_compress = 1;
				else {
					fprintf(stderr, "invalid argument for --sf-compress: %s\n", argv[i]);
					return 1;
				}
			} else if (!strcmp(argv[i], "--mid-compress")) {
				if (++i == argc) {
					fprintf(stderr, "missing argument for --mid-compress\n");
					return 1;
				}
				flag_mid_compress = atoi(argv[i]);
				if (flag_mid_compress < 0 || flag_mid_compress > MAX_COMPRESSION) {
					fprintf(stderr, "invalid argument for --mid-compress: %s\n", argv[i]);
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
				had_error = true;
			} else {
				walkdir(argv[i], outdir, convert);
			}
		}
	}

	return had_error ? 1 : 0;
}
