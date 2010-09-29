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
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#define WRITE_SIZE	1024

#define TITLE_LOC	32
#define TITLE_SIZE	20
#define DEF_TITLE	"N64 Demo"

#define STATE_NONE	0
#define STATE_L     1
#define STATE_H		2
#define STATE_O		3
#define STATE_S		4
#define STATE_T		5

/* Easier to write from here */
static int title[TITLE_SIZE];
static int wrote_title = 0;

void print_usage(char *prog_name)
{
	fprintf(stderr, "Usage: %s [-b] -l <size>B/K/M -h <file> -o <file> -t <title> <file> [[-s <offset>B/K/M] <file>]*\n\n", prog_name);
	fprintf(stderr, "This program appends a header to an arbitrary number of binaries,\n");
	fprintf(stderr, "the first being an Nintendo64 binary and the rest arbitrary data.\n\n");
	fprintf(stderr, "\t-b\t\tByteswap the resulting output.\n");
	fprintf(stderr, "\t-l <size>\tForce output to <size> bytes.\n");
	fprintf(stderr, "\t-h <file>\tUse <file> as header.\n");
	fprintf(stderr, "\t-o <file>\tOutput is saved to <file>.\n");
	fprintf(stderr, "\t-t <title>\tTitle of ROM.\n");
	fprintf(stderr, "\t-s <offset>\tNext file starts at <offset> from top of memory.  Offset must be 32bit aligned.\n");
	fprintf(stderr, "\t\t\tB for byte offset.\n");
	fprintf(stderr, "\t\t\tK for kilobyte offset.\n");
	fprintf(stderr, "\t\t\tM for megabyte offset.\n");
}

uint32_t get_file_size(FILE *fp)
{
	int x = ftell(fp);
	uint32_t ret = 0;
	
	fseek(fp, 0, SEEK_END);
	ret = ftell(fp);
	fseek(fp, x, SEEK_SET);
	
	return ret;
}

int swap_bytes(uint8_t *buffer, int size)
{
	if(size & 1 == 0)
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

int copy_file(FILE *dest, char *file, int byte_swap)
{
	FILE *read_file = fopen(file, "rb");
	uint8_t *buffer;
	
	if(!read_file)
	{
		fprintf(stderr, "Cannot open %s for reading!\n", file);
		return -1;
	}
	
	int fsize = get_file_size(read_file);
	int rsize = fsize;
	
	/* Should probably make this read in incriments, but whatever */
	buffer = malloc(WRITE_SIZE);
	
	if(!buffer)
	{
		fprintf(stderr, "Out of memory!\n");
		
		fclose(read_file);
		
		return -1;
	}
	
	/* Weird windows bug fix, plus this is the right way to do things */
	while(fsize > 0)
	{
		/* Max 1K chunk */
		int write_size = (fsize > WRITE_SIZE) ? WRITE_SIZE : fsize;
		fsize -= write_size;
		
		fread(buffer, 1, write_size, read_file);
		
		if(!wrote_title)
		{
			/* Pop title into header */
			memcpy(buffer + TITLE_LOC, title, TITLE_SIZE);
			
			wrote_title = 1;
		}
	
		if(byte_swap)
		{
			if(swap_bytes(buffer, write_size))
			{
				fprintf(stderr, "Invalid file size on %s.  Should be multiple of 32bits!\n", file);
				
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

int output_zeros(FILE *dest, int amount)
{
	if(amount & 3 != 0)
	{
		/* Don't support odd word alignments */
		return -1;
	}
	
	if(amount <= 0)
	{
		/* We are done */
		return 0;
	}
	
	int i;
	
	for(i = 0; i < (amount >> 2); i++)
	{
		/* Write out a word at a time */
		uint32_t byte = 0;
		
		fwrite(&byte, 1, 4, dest);
	}
	
	return 0;
}

int get_bytes(char *cur_arg)
{
	int size = 0;
	
	/* Figure out if they mean bytes, kilobytes, megabytes */
	char c = cur_arg[strlen(cur_arg)-1];
	
	/* Blank last character to do an atoi */						
	char *temp = strdup(cur_arg);
	temp[strlen(temp)-1] = '\0';
	
	/* Grab number */
	size = atoi(temp);						
	free(temp);
	
	/* Multiply out by byte amount */
	switch(c)
	{
		case 'm':
		case 'M':
			size *= 1024;
		case 'k':
		case 'K':
			size *= 1024;
		case 'b':
		case 'B':
			break;
		default:
			/* Invalid! */
			return -1;
	}
	
	return size;
}

int main(int argc, char *argv[])
{
	FILE *write_file = 0;
	char *header = 0;
	char *output = 0;
	int total_size = 0;
	int total_bytes = 0;
	int state = STATE_NONE;
	int byte_swap = 0;
	int i;
	
	/* Set default title */
	memset(title, 0x20, TITLE_SIZE);
	memcpy(title, DEF_TITLE, (strlen(DEF_TITLE) > TITLE_SIZE) ? TITLE_SIZE : strlen(DEF_TITLE));
	
	if(argc <= 1)
	{
		/* No way we can have just one argument or less */
		print_usage(argv[0]);
		return -1;
	}
	
	for(i = 1; i < argc; i++)
	{
		char *cur_arg = argv[i];
		
		switch(cur_arg[0])
		{
			case '-':
				/* Option flag */
				switch(cur_arg[1])
				{
					case 'b':
						/* Byteswap output */
						byte_swap = 1;
						break;
					case 'l':
						/* Size of resulting image */
						state = STATE_L;
						break;
					case 'h':
						/* Header file */
						if(!header)
						{
							state = STATE_H;
						}
						else
						{
							print_usage(argv[0]);
							return -1;
						}
						
						break;
					case 'o':
						/* Output file */
						if(!output)
						{
							state = STATE_O;
						}
						else
						{
							print_usage(argv[0]);
							return -1;
						}
						break;
					case 's':
						/* Offset */
						state = STATE_S;
						break;
					case 't':
						/* Title */
						state = STATE_T;
						break;
					default:
						print_usage(argv[0]);
						return -1;
				}
				
				break;
			case '\0':
				/* Shouldn't happen */
				fprintf(stderr, "Unexpected end of arguments!\n");
				return -1;
			default:
				/* Standard argument */
				switch(state)
				{
					case STATE_H:
						/* Get header file */
						header = cur_arg;
						break;
					case STATE_O:
						/* Get output file */
						output = cur_arg;
						break;
					case STATE_T:
						/* Grab title */						
						if(strlen(cur_arg) < 16)
						{
							/* Spaces for pretty printing */
							memset(title, 0x20, TITLE_SIZE);
							memcpy(title, cur_arg, strlen(cur_arg));
						}
						else
						{
							memcpy(title, cur_arg, TITLE_SIZE);
						}
						
						break;
					case STATE_S:
						/* Can't be here unless header and output set, and has to be at least 2 bytes.
						   Also, they have to have at least one file output before they can skip bytes. */
						if(!header || !output || strlen(cur_arg) < 2 || !total_bytes || !total_size)
						{
							print_usage(argv[0]);
							return -1;
						}
						
						int offset = get_bytes(cur_arg);
						
						if(offset < 0)
						{
							/* Invalid! */
							print_usage(argv[0]);
							return -1;
						}
						
						/* Write out needed number of zeros */
						int num_zeros = offset - total_bytes;
						
						if(output_zeros(write_file, num_zeros))
						{
							fprintf(stderr, "Invalid offset to seek to!\n", output);
							return -1;
						}
						
						/* Same as total_bytes = offset */
						total_bytes += num_zeros;
						
						break;
					case STATE_L:
						/* Just grab size */
						total_size = get_bytes(cur_arg);
						
						if(total_size < 0)
						{
							/* Invalid! */
							print_usage(argv[0]);
							return -1;
						}
						
						break;
					case STATE_NONE:
						/* Can't be here unless header and output set */
						if(!header || !output || !total_size)
						{
							print_usage(argv[0]);
							return -1;
						}
						
						/* Is our output file open? */
						if(!write_file)
						{
							write_file = fopen(output, "wb");
							
							if(!write_file)
							{
								fprintf(stderr, "Cannot open %s for writing!\n", output);
								return -1;
							}
							
							int wrote = copy_file(write_file, header, byte_swap);
							
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
						int copied = copy_file(write_file, cur_arg, byte_swap);
						
						if(copied < 0)
						{
							/* Error, exit */
							return -1;
						}
						
						/* Keep track to be sure we align properly when they request a memory alignment */
						total_bytes += copied;
						
						break;
				}
				
				/* Reset state */
				state = STATE_NONE;
				
				break;
		}
	}
	
	if(!total_bytes)
	{
		printf("No input files, nothing written!\n");
		/* Didn't write anything! */
	}
	else
	{
		/* Pad to correct length */
		int num_zeros = total_size - total_bytes;
		
		if(output_zeros(write_file, num_zeros))
		{
			fprintf(stderr, "Couldn't pad image!\n", output);
			return -1;
		}
		
		fflush(write_file);
		fclose(write_file);
	}
	
	return 0;
}
