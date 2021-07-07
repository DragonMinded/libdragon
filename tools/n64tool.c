/*
    n64tool V1.0, a program used to append an n64 header to an arbitrary
	sized Nintendo64 binary.
    Copyright (C) 2009  Shaun Taylor (dragonminded@dragonminded.com)

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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WRITE_SIZE (1024*1024)
#define SAVETYPE_NOT_SET 0xFF

#define TITLE_OFFSET   0x20
#define TITLE_SIZE     20
#define CART_ID_OFFSET 0x3C
#define CART_ID_SIZE   2
#define VERSION_OFFSET 0x3F
#define VERSION_SIZE   1

static const unsigned char zero[1024] = {0};

void print_usage(const char * prog_name)
{
	fprintf(stderr, "Usage: %s [-b] [-r] [-c] [-w <savetype>] -t <title> -l <size>B/K/M -h <file> -o <file> <file> [[-s <offset>B/K/M] <file>]*\n\n", prog_name);
	fprintf(stderr, "This program creates an N64 ROM from a header and a list of files,\n");
	fprintf(stderr, "the first being an Nintendo64 binary and the rest arbitrary data.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Command-line flags:\n");
	fprintf(stderr, "\t-b, --byteswap\t\tByteswap the resulting output (v64 format).\n");
	fprintf(stderr, "\t-t, --title <title>\tTitle of ROM (max %d characters).\n", TITLE_SIZE);
	fprintf(stderr, "\t-w, --savetype <type>\tDeclare cartridge save type.\n");
	fprintf(stderr, "\t-c, --rtc\t\tDeclare real-time clock support.\n");
	fprintf(stderr, "\t-r, --regionfree\tDeclare region-free ROM.\n");
	fprintf(stderr, "\t-l, --size <size>\tForce output to <size>.\n");
	fprintf(stderr, "\t-h, --header <file>\tUse <file> as header.\n");
	fprintf(stderr, "\t-o, --output <file>\tOutput is saved to <file>.\n");
	fprintf(stderr, "\t-s, --offset <offset>\tNext file starts at <offset> from top of memory. Offset must be 32-bit aligned.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Size/offset suffix notation:\n");
	fprintf(stderr, "\tB for bytes.\n");
	fprintf(stderr, "\tK for kilobytes.\n");
	fprintf(stderr, "\tM for megabytes.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Supported cartridge save types:\n");
	fprintf(stderr, "\tnone\t\tGame does not save or uses Controller Pak.\n");
	fprintf(stderr, "\teeprom4k\tGame saves to 4 kilobit EEPROM.\n");
	fprintf(stderr, "\teeprom16k\tGame saves to 16 kilobit EEPROM.\n");
	fprintf(stderr, "\tsram128k\tGame saves to 128 kilobit SRAM\n");
	fprintf(stderr, "\tsram256k\tGame saves to 256 kilobit SRAM\n");
	fprintf(stderr, "\tsram768k\tGame saves to 768 kilobit SRAM\n");
	fprintf(stderr, "\tflashram\tGame saves to 1 megabit FlashRAM\n");
}

bool check_flag(const char * arg, const char * shortFlag, const char * longFlag)
{
	return strcmp(arg, shortFlag) == 0 || strcmp(arg, longFlag) == 0;
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

ssize_t swap_bytes(uint8_t * buffer, size_t size)
{
	if((size & 1) != 0)
	{
		/* Invalid, can only byteswap multiples of 2 */
		return -1;
	}

	int i;

	for(i = 0; i < (size >> 1); i++)
	{
		int loc1 = i << 1;
		int loc2 = loc1 + 1;

		/* Easy peasy */
		uint8_t temp = buffer[loc1];
		buffer[loc1] = buffer[loc2];
		buffer[loc2] = temp;
	}

	return 0;
}

ssize_t copy_file(FILE * dest, const char * file, bool byte_swap)
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

	/* Should probably make this read in incriments, but whatever */
	buffer = malloc(WRITE_SIZE);

	if(!buffer)
	{
		fprintf(stderr, "ERROR: Out of memory!\n");

		fclose(read_file);

		return -1;
	}

	/* Weird windows bug fix, plus this is the right way to do things */
	while(fsize > 0)
	{
		/* Max 1K chunk */
		ssize_t write_size = (fsize > WRITE_SIZE) ? WRITE_SIZE : fsize;
		fsize -= write_size;

		fread(buffer, 1, write_size, read_file);

		if(byte_swap)
		{
			if(swap_bytes(buffer, write_size))
			{
				fprintf(stderr, "ERROR: Invalid file size on %s.  Should be multiple of 32bits!\n", file);

				free(buffer);
				fclose(read_file);

				return -1;
			}
		}

		fwrite(buffer, 1, write_size, dest);
	}

	free(buffer);
	fclose(read_file);

	return rsize;
}

ssize_t output_zeros(FILE * dest, ssize_t amount)
{
	if((amount & 3) != 0)
	{
		/* Don't support odd word alignments */
		return -1;
	}
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

	/* Blank last character to do an atoi */
	char * temp = strdup(arg);
	temp[arg_len-1] = '\0';

	/* Convert to base number */
	ssize_t size = atoi(temp);
	free(temp);

	/* Multiply by the suffix magnitude */
	switch(arg[arg_len-1])
	{
		case 'm':
		case 'M':
			size *= 1024;
		case 'k':
		case 'K':
			size *= 1024;
		case 'b':
		case 'B':
			return size;
		default:
			/* Invalid! */
			return -1;
	}
}

uint8_t parse_save_type(const char * arg)
{
	/**
	 * Corresponds to ED64 ROM Configuration Database values:
	 * @see https://github.com/N64-tools/ED64/blob/develop/docs/rom_config_database.md
	 */
	if (strcmp(arg, "none") == 0) return 0x00;
	if (strcmp(arg, "eeprom4k") == 0) return 0x10;
	if (strcmp(arg, "eeprom16k") == 0) return 0x20;
	if (strcmp(arg, "sram256k") == 0) return 0x30;
	if (strcmp(arg, "sram768k") == 0) return 0x40;
	if (strcmp(arg, "flashram") == 0) return 0x50;
	if (strcmp(arg, "sram128k") == 0) return 0x60;
	return SAVETYPE_NOT_SET;
}

/**
 * Combines save_type, force_rtc, region_free values for use in the header version field:
 * @see https://github.com/N64-tools/ED64/blob/develop/docs/rom_config_database.md#developer-override
 */
uint8_t rom_configuration(uint8_t save_type, bool force_rtc, bool region_free)
{
	if(!force_rtc && !region_free && save_type == SAVETYPE_NOT_SET)
	{
		/* Disable ROM configuration */
		return SAVETYPE_NOT_SET;
	}
	if(save_type == SAVETYPE_NOT_SET)
	{
		fprintf(stderr, "WARNING: RTC/Region-Free declared without save type; defaulting to 'none'\n");
		save_type = parse_save_type("none");
	}
	if(force_rtc && (save_type == 0x10 || save_type == 0x20))
	{
		fprintf(stderr, "WARNING: The combination of EEPROM + RTC does not work on real N64 hardware!\n");
	}
	uint8_t config = (force_rtc ? 1 : 0) + (region_free ? 2 : 0);
	return save_type | config;
}

int main(int argc, char *argv[])
{
	FILE * write_file = NULL;
	const char * header = NULL;
	const char * output = NULL;
	size_t total_size = 0;
	size_t total_bytes = 0;
	bool byte_swap = false;
	bool force_rtc = false;
	bool region_free = false;
	uint8_t save_type = SAVETYPE_NOT_SET;
	char title[TITLE_SIZE];

	/* Set title to all spaces */
	memset(title, 0x20, TITLE_SIZE);

	if(argc <= 1)
	{
		/* No way we can have just one argument or less */
		print_usage(argv[0]);
		return -1;
	}

	int i = 1;
	const char * arg;

	while(i < argc)
	{
		arg = argv[i++];
		if(check_flag(arg, "-b", "--byteswap"))
		{
			byte_swap = true;
			continue;
		}
		if(check_flag(arg, "-c", "--rtc"))
		{
			force_rtc = true;
			continue;
		}
		if(check_flag(arg, "-r", "--regionfree"))
		{
			region_free = true;
			continue;
		}
		if(check_flag(arg, "-w", "--savetype"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to savetype flag\n\n");
				print_usage(argv[0]);
				return -1;
			}

			save_type = parse_save_type(argv[i++]);

			if(save_type == SAVETYPE_NOT_SET)
			{
				/* Invalid save type */
				fprintf(stderr, "ERROR: Invalid savetype argument\n\n");
				print_usage(argv[0]);
				return -1;
			}

			continue;
		}
		if(check_flag(arg, "-h", "--header"))
		{
			if(header)
			{
				/* Invalid usage */
				fprintf(stderr, "ERROR: The header can only be set once\n\n");
				print_usage(argv[0]);
				return -1;
			}

			if(i >= argc)
			{
				/* Invalid usage */
				fprintf(stderr, "ERROR: Expected an argument to header flag\n\n");
				print_usage(argv[0]);
				return -1;
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
				print_usage(argv[0]);
				return -1;
			}

			if(i >= argc)
			{
				/* Invalid usage */
				fprintf(stderr, "ERROR: Expected an argument to output flag\n\n");
				print_usage(argv[0]);
				return -1;
			}

			output = argv[i++];
			continue;
		}
		if(check_flag(arg, "-l", "--size"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to size flag\n\n");
				print_usage(argv[0]);
				return -1;
			}

			total_size = parse_bytes(argv[i++]);

			if(total_size <= 0)
			{
				/* Invalid size */
				fprintf(stderr, "ERROR: Invalid size argument\n\n");
				print_usage(argv[0]);
				return -1;
			}

			continue;
		}
		if(check_flag(arg, "-s", "--offset"))
		{
			if(!header || !output)
			{
				fprintf(stderr, "ERROR: Need header and output flags before offset\n\n");
				print_usage(argv[0]);
				return -1;
			}

			if(!total_bytes || !total_size)
			{
				fprintf(stderr, "ERROR: The first file cannot have an offset\n\n");
				print_usage(argv[0]);
				return -1;
			}

			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to offset flag\n\n");
				print_usage(argv[0]);
				return -1;
			}

			ssize_t offset = parse_bytes(argv[i++]);

			if(offset <= 0)
			{
				/* Invalid offset */
				fprintf(stderr, "ERROR: Invalid offset argument\n\n");
				print_usage(argv[0]);
				return -1;
			}

			/* Write out needed number of zeros */
			size_t num_zeros = offset - total_bytes;

			if(output_zeros(write_file, num_zeros))
			{
				fprintf(stderr, "ERROR: Invalid offset %d to seek to in %s!\n", offset, output);
				return -1;
			}

			/* Same as total_bytes = offset */
			total_bytes += num_zeros;
			continue;
		}
		if(check_flag(arg, "-t", "--title"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to title flag\n\n");
				print_usage(argv[0]);
				return -1;
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
		if(!header || !output || !total_size)
		{
			fprintf(stderr, "ERROR: Need size, header, and output before first file\n\n");
			print_usage(argv[0]);
			return -1;
		}

		/* Is our output file open? */
		if(!write_file)
		{
			write_file = fopen(output, "wb");

			if(!write_file)
			{
				fprintf(stderr, "ERROR: Cannot open %s for writing!\n", output);
				return -1;
			}

			ssize_t wrote = copy_file(write_file, header, byte_swap);

			/* Since write file wasn't open, we haven't written the header */
			if(wrote < 0)
			{
				return -1;
			}
			else
			{
				/* Adjust final output size to reflect */
				total_size -= wrote;
			}
		}

		/* Copy over file */
		ssize_t copied = copy_file(write_file, arg, byte_swap);

		if(copied < 0)
		{
			/* Error, exit */
			return -1;
		}

		/* Keep track to be sure we align properly when they request a memory alignment */
		total_bytes += copied;
	}

	if(!total_bytes)
	{
		/* Didn't write anything! */
		printf("ERROR: No input files, nothing written!\n");
		return -1;
	}

	/* Pad to correct length */
	ssize_t num_zeros = total_size - total_bytes;

	if(output_zeros(write_file, num_zeros))
	{
		fprintf(stderr, "ERROR: Couldn't pad image in %s!\n", output);
		return -1;
	}

	/* Set title in header */
	fseek(write_file, TITLE_OFFSET, SEEK_SET);
	fwrite(title, 1, TITLE_SIZE, write_file);

	/* Set ROM configuration in header */
	uint8_t config = rom_configuration(save_type, force_rtc, region_free);
	if(config != SAVETYPE_NOT_SET)
	{
		/**
		 * Enable the developer override and store the ROM configuration in the header
		 * @see https://github.com/N64-tools/ED64/blob/develop/docs/rom_config_database.md#developer-override
		 */
		const char cart_id[CART_ID_SIZE] = {'E', 'D'};
		fseek(write_file, CART_ID_OFFSET, SEEK_SET);
		fwrite(cart_id, 1, CART_ID_SIZE, write_file);
		fseek(write_file, VERSION_OFFSET, SEEK_SET);
		fwrite(&config, 1, VERSION_SIZE, write_file);
	}

	fflush(write_file);
	fclose(write_file);

	return 0;
}
