/*
	n64tool V1.0, a program used to append an n64 header to an arbitrary
	sized Nintendo64 binary.
	Copyright (C) 2009  DragonMinded (dragonminded@dragonminded.com)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef __MINGW32__
#include <sys/errno.h>
#endif

#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

// Minimum ROM size alignment, used by default. We currently know of two constraints:
//  * 64drive firmware has a bug and can only transfer chunks of 512 bytes. Some
//    tools like UNFloader and g64drive work around this bug by padding ROMs,
//    but others (like the official one) don't. So it's better in general to
//    pad to 512 bytes.
//  * iQue player only allows loading ROMs which are multiple of 16 Kbyte in size.
//  
// To allow the maximum compatibility, we pad to 16Kb by default. Users can still
// force a specific length with --size, if they need to.
#define PAD_ALIGN    16384

#define WRITE_SIZE   (1024 * 1024)
#define HEADER_SIZE  0x1000          // Size of the header (which includes IPL3)
#define MIN_SIZE     0x100000        // Minimum size of a ROM after header (mandated by IPL3)

#define TITLE_OFFSET 0x20
#define TITLE_SIZE   20

#define STATUS_OK       0
#define STATUS_ERROR    1
#define STATUS_BADUSAGE 2

static const unsigned char zero[1024] = {0};
static char * tmp_output = NULL;


int print_usage(const char * prog_name)
{
	fprintf(stderr, "Usage: %s [-t <title>] [-l <size>B/K/M] -h <file> -o <file> <file> [[-s <offset>B/K/M] <file>]*\n\n", prog_name);
	fprintf(stderr, "This program creates an N64 ROM from a header and a list of files,\n");
	fprintf(stderr, "the first being an Nintendo64 binary and the rest arbitrary data.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Command-line flags:\n");
	fprintf(stderr, "\t-t, --title <title>    Title of ROM (max %d characters).\n", TITLE_SIZE);
	fprintf(stderr, "\t-l, --size <size>      Force ROM output file size to <size> (min 1 mebibyte).\n");
	fprintf(stderr, "\t-h, --header <file>    Use <file> as IPL3 header.\n");
	fprintf(stderr, "\t-o, --output <file>    Save output ROM to <file>.\n");
	fprintf(stderr, "\t-s, --offset <offset>  Next file starts at <offset> from top of memory. Offset must be 32-bit aligned.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Binary byte size/offset suffix notation:\n");
	fprintf(stderr, "\tB for bytes.\n");
	fprintf(stderr, "\tK for kibibytes (KiB) [1024 bytes].\n");
	fprintf(stderr, "\tM for mebibytes (MiB) [1024 kibibytes].\n");
	return STATUS_BADUSAGE;
}

bool check_flag(const char * arg, const char * shortFlag, const char * longFlag)
{
	return !strcmp(arg, shortFlag) || !strcmp(arg, longFlag);
}

size_t get_file_size(FILE * fp)
{
	size_t restore = ftell(fp);
	size_t end = 0;

	fseek(fp, 0, SEEK_END);
	end = ftell(fp);
	fseek(fp, restore, SEEK_SET);

	return end;
}

ssize_t copy_file(FILE * dest, const char * file)
{
	FILE *read_file = fopen(file, "rb");
	uint8_t *buffer;

	if(!read_file)
	{
		fprintf(stderr, "ERROR: Cannot open %s for reading!\n", file);
		return -1;
	}

	ssize_t fsize = get_file_size(read_file);
	ssize_t rsize = fsize;

	/* Should probably make this read in increments, but whatever */
	buffer = malloc(WRITE_SIZE);

	if(!buffer)
	{
		fprintf(stderr, "ERROR: Out of memory!\n");

		fclose(read_file);

		return -1;
	}

	/* Weird Windows bug fix, plus this is the right way to do things */
	while(fsize > 0)
	{
		/* Max 1K chunk */
		ssize_t write_size = (fsize > WRITE_SIZE) ? WRITE_SIZE : fsize;
		fsize -= write_size;

		fread(buffer, 1, write_size, read_file);
		fwrite(buffer, 1, write_size, dest);
	}

	free(buffer);
	fclose(read_file);

	return rsize;
}

ssize_t output_zeros(FILE * dest, ssize_t amount)
{
	if(amount < 0)
	{
		/* Can't backward seek */
		return -1;
	}

	int i;
	while (amount > 0) {
		int sz = amount;
		if (sz > sizeof(zero))
			sz = sizeof(zero);
		fwrite(zero, 1, sz, dest);
		amount -= sz;
	}

	return 0;
}

ssize_t parse_bytes(const char * arg)
{
	size_t arg_len = strlen(arg);

	if(arg_len < 2)
	{
		/* Need at least 2 characters to parse */
		return -1;
	}

	char * suffix;
	size_t size = strtol(arg, &suffix, 10);

	/* Multiply by the suffix magnitude */
	switch(suffix[0])
	{
		case 'm':
		case 'M':
			size *= 1024;
		case 'k':
		case 'K':
			size *= 1024;
		case 'b':
		case 'B':
		default:
			return size;
	}
}

void remove_tmp_file(void)
{
	if(tmp_output)
		remove(tmp_output);
}

int main(int argc, char *argv[])
{
	FILE * write_file = NULL;
	const char * header = NULL;
	const char * output = NULL;
	size_t declared_size = 0;
	size_t total_bytes_written = 0;
	char title[TITLE_SIZE + 1] = { 0, };

	if(argc <= 1)
	{
		/* No way we can have just one argument or less */
		return print_usage(argv[0]);
	}

	int i = 1;
	const char * arg;

	while(i < argc)
	{
		arg = argv[i++];
		if(check_flag(arg, "-b", "--byteswap"))
		{
			/* Invalid usage */
			fprintf(stderr, "ERROR: The byteswap option is no longer supported. ");
			fprintf(stderr, "Use another tool to convert the output of this program.\n");
			fprintf(stderr, "       For example: dd conv=swab if=rom.z64 of=rom.v64\n\n");
			return print_usage(argv[0]);
		}
		if(check_flag(arg, "-h", "--header"))
		{
			if(header)
			{
				/* Invalid usage */
				fprintf(stderr, "ERROR: The header can only be set once\n\n");
				return print_usage(argv[0]);
			}

			if(i >= argc)
			{
				/* Invalid usage */
				fprintf(stderr, "ERROR: Expected an argument to header flag\n\n");
				return print_usage(argv[0]);
			}

			header = argv[i++];
			continue;
		}
		if(check_flag(arg, "-o", "--output"))
		{
			if(output)
			{
				/* Invalid usage */
				fprintf(stderr, "ERROR: The output can only be set once\n\n");
				return print_usage(argv[0]);
			}

			if(i >= argc)
			{
				/* Invalid usage */
				fprintf(stderr, "ERROR: Expected an argument to output flag\n\n");
				return print_usage(argv[0]);
			}

			output = argv[i++];

			size_t output_len = strlen(output);
			if(output_len < 5 || strcmp(output + output_len - 4, ".z64"))
			{
				fprintf(stderr, "WARNING: The output should have a '.z64' file extension\n");
			}

			asprintf(&tmp_output, "%s.tmp", output);
			continue;
		}
		if(check_flag(arg, "-l", "--size"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to size flag\n\n");
				return print_usage(argv[0]);
			}

			ssize_t size = parse_bytes(argv[i++]);

			if(size < MIN_SIZE)
			{
				/* Invalid size */
				fprintf(stderr, "ERROR: Invalid size argument; must be at least %d bytes\n\n", MIN_SIZE);
				return print_usage(argv[0]);
			}
			if (size % 4 != 0)
			{
				/* Invalid size */
				fprintf(stderr, "ERROR: Invalid size argument; must be a multiple of 4 bytes\n\n");
				return print_usage(argv[0]);				
			}
			if (size % 512 != 0)
			{
				fprintf(stderr, "WARNING: Sizes which are not multiple of 512 bytes might have problems being loaded with a 64drive\n\n");
			}				

			declared_size = size;
			continue;
		}
		if(check_flag(arg, "-s", "--offset"))
		{
			if(!header || !output)
			{
				fprintf(stderr, "ERROR: Need header and output flags before offset\n\n");
				return print_usage(argv[0]);
			}

			if(!total_bytes_written)
			{
				fprintf(stderr, "ERROR: The first file cannot have an offset\n\n");
				return print_usage(argv[0]);
			}

			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to offset flag\n\n");
				return print_usage(argv[0]);
			}

			ssize_t offset = parse_bytes(argv[i++]);

			if(offset <= 0)
			{
				/* Invalid offset */
				fprintf(stderr, "ERROR: Invalid offset argument\n\n");
				return print_usage(argv[0]);
			}
			if((offset % 4) != 0)
			{
				fprintf(stderr, "ERROR: Invalid offset argument (must be multiple of 4)\n\n");
				return print_usage(argv[0]);				
			}

			/* Write out needed number of zeros */
			ssize_t num_zeros = offset - total_bytes_written;

			if(output_zeros(write_file, num_zeros))
			{
				fprintf(stderr, "ERROR: Invalid offset %zd to seek to in %s!\n", offset, output);
				return STATUS_ERROR;
			}

			/* Same as total_bytes_written = offset */
			total_bytes_written += num_zeros;
			continue;
		}
		if(check_flag(arg, "-t", "--title"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to title flag\n\n");
				return print_usage(argv[0]);
			}

			const char * title_arg = argv[i++];
			size_t title_len = strlen(title_arg);

			if(title_len > TITLE_SIZE)
			{
				fprintf(stderr, "WARNING: Title will be truncated to %d characters\n", TITLE_SIZE);
				title_len = TITLE_SIZE;
			}

			memcpy(title, title_arg, title_len);
			continue;
		}

		/* Argument is not a flag; treat it as an input file */

		/* Can't copy input file unless header and output set */
		if(!header || !output)
		{
			fprintf(stderr, "ERROR: Need header and output before first file\n\n");
			return print_usage(argv[0]);
		}

		/* If this is the first input file, open the output file and write the header */
		if(!write_file)
		{
			/* Create or completely overwrite the output file */
			write_file = fopen(tmp_output, "wb");

			if(!write_file)
			{
				fprintf(stderr, "ERROR: Cannot open '%s' for writing\n", output);
				return STATUS_ERROR;
			}

			/* Try to clean up the temporary file if we exit */
			atexit(remove_tmp_file);

			/* Copy over the ROM header */
			ssize_t bytes_copied = copy_file(write_file, header);

			if(bytes_copied != HEADER_SIZE)
			{
				fprintf(stderr, "ERROR: Unable to copy ROM header from '%s' to '%s'\n", header, output);
				return STATUS_ERROR;
			}
		}

		/* Copy the input file into the output file */
		ssize_t bytes_copied = copy_file(write_file, arg);

		if(bytes_copied < 0)
		{
			fprintf(stderr, "ERROR: Unable to copy file from '%s' to '%s'\n", arg, output);
			return STATUS_ERROR;
		}

		/* Keep track to be sure we align properly when they request a memory alignment */
		total_bytes_written += bytes_copied;
	}

	if(!total_bytes_written)
	{
		/* Didn't write anything! */
		printf("ERROR: No input files, nothing written\n\n");
		return print_usage(argv[0]);
	}

	/* If the declared size is too small, error out */
	if(declared_size && declared_size < total_bytes_written)
	{
		fprintf(stderr, "ERROR: Couldn't fit ROM in %zu bytes as requested.\n", declared_size);
		return print_usage(argv[0]);
	}

	/* Pad the output file to the declared size (not including the IPL3 header) */
	if(!declared_size) {		
		/* If the user didn't specify a size, initialize this to the minimum
		   size that is padded to the correct alignment. Notice that this variable
		   declares the size WITHOUT header, but the padding refers to the final
		   ROM and so it must be calculated with the header. */
		declared_size = ROUND_UP(MIN_SIZE+HEADER_SIZE, PAD_ALIGN)-HEADER_SIZE;
	}
	if(declared_size > total_bytes_written)
	{
		ssize_t num_zeros = declared_size - total_bytes_written;

		if(output_zeros(write_file, num_zeros))
		{
			fprintf(stderr, "ERROR: Couldn't pad %zu bytes to %zu bytes.\n", total_bytes_written, declared_size);
			return print_usage(argv[0]);
		}
	}
	else if(((total_bytes_written+HEADER_SIZE) % PAD_ALIGN) != 0)
	{
		/* Pad size as required. */
		ssize_t num_zeros = PAD_ALIGN - ((total_bytes_written+HEADER_SIZE) % PAD_ALIGN);
		if(output_zeros(write_file, num_zeros))
		{
			fprintf(stderr, "ERROR: Couldn't pad %zu bytes to %zu bytes.\n", total_bytes_written, total_bytes_written+num_zeros);
			return print_usage(argv[0]);
		}
	}

	/* Set title in header */
	fseek(write_file, TITLE_OFFSET, SEEK_SET);
	fwrite(title, 1, TITLE_SIZE, write_file);

	/* Sync and close the output file */
	fclose(write_file);

	/* Rename to the final name */
	if(rename(tmp_output, output) != 0) {
		fprintf(stderr, "Couldn't rename temporary output file '%s' to '%s': %s", tmp_output, output, strerror(errno));
		return STATUS_ERROR;
	}

	return STATUS_OK;
}
