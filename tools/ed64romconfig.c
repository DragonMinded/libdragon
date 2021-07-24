/*
	ed64romconfig V1.0, a program to set EverDrive64 ROM Header Configuration.
	Copyright (C) 2021  Christopher Bonhage (me@christopherbonhage.com)

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

#define SAVETYPE_NONE      0x00
#define SAVETYPE_EEPROM4K  0x10
#define SAVETYPE_EEPROM16K 0x20
#define SAVETYPE_SRAM256K  0x30
#define SAVETYPE_SRAM768K  0x40
#define SAVETYPE_FLASHRAM  0x50
#define SAVETYPE_SRAM1M    0x60
#define SAVETYPE_INVALID   0xFF
#define ROMCONFIG_NOT_SET  0x00

#define CART_ID_OFFSET 0x3C
#define CART_ID_SIZE   2
#define VERSION_OFFSET 0x3F
#define VERSION_SIZE   1

#define STATUS_OK       0
#define STATUS_ERROR    1
#define STATUS_BADUSAGE 2

int print_usage(const char * prog_name)
{
	fprintf(stderr, "Usage: %s [-r] [-c] [-w <savetype>] <file>\n\n", prog_name);
	fprintf(stderr, "This program takes a big-endian N64 ROM and sets the header so that\n");
	fprintf(stderr, "EverDrive64 will respect the declared save type, RTC, and region-free\n");
	fprintf(stderr, "settings without needing to create a save_db.txt entry for it.\n");
	fprintf(stderr, "See: https://github.com/krikzz/ED64/blob/master/docs/rom_config_database.md#developer-override\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Command-line flags:\n");
	fprintf(stderr, "\t-w, --savetype <type>  Declare cartridge save type.\n");
	fprintf(stderr, "\t-c, --rtc              Declare real-time clock support.\n");
	fprintf(stderr, "\t-r, --regionfree       Declare region-free ROM.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Supported cartridge save types:\n");
	fprintf(stderr, "\tnone        Game does not save or uses Controller Pak.\n");
	fprintf(stderr, "\teeprom4k    Game saves to 4 kilobit EEPROM.\n");
	fprintf(stderr, "\teeprom16k   Game saves to 16 kilobit EEPROM.\n");
	fprintf(stderr, "\tsram256k    Game saves to 256 kilobit SRAM\n");
	fprintf(stderr, "\tsram768k    Game saves to 768 kilobit SRAM\n");
	fprintf(stderr, "\tsram1m      Game saves to 1 megabit SRAM\n");
	fprintf(stderr, "\tflashram    Game saves to 1 megabit FlashRAM\n");
	return STATUS_BADUSAGE;
}

bool check_flag(const char * arg, const char * shortFlag, const char * longFlag)
{
	return !strcmp(arg, shortFlag) || !strcmp(arg, longFlag);
}

/**
 * Corresponds to ED64 ROM Configuration Database values:
 * @see https://github.com/krikzz/ED64/blob/master/docs/rom_config_database.md
 */
uint8_t parse_save_type(const char * arg)
{
	if(!strcmp(arg, "none"))      return SAVETYPE_NONE;
	if(!strcmp(arg, "eeprom4k"))  return SAVETYPE_EEPROM4K;
	if(!strcmp(arg, "eeprom16k")) return SAVETYPE_EEPROM16K;
	if(!strcmp(arg, "sram256k"))  return SAVETYPE_SRAM256K;
	if(!strcmp(arg, "sram768k"))  return SAVETYPE_SRAM768K;
	if(!strcmp(arg, "flashram"))  return SAVETYPE_FLASHRAM;
	if(!strcmp(arg, "sram1m"))    return SAVETYPE_SRAM1M;
	return SAVETYPE_INVALID;
}

int main(int argc, char *argv[])
{
	FILE * write_file = NULL;
	bool force_rtc = false;
	bool region_free = false;
	uint8_t save_type = SAVETYPE_NONE;
	int i = 1;
	const char * arg;

	if(argc <= 1)
	{
		/* No way we can have just one argument or less */
		return print_usage(argv[0]);
	}
	while(i < argc)
	{
		arg = argv[i++];
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
				return print_usage(argv[0]);
			}

			save_type = parse_save_type(argv[i++]);

			if(save_type == SAVETYPE_INVALID)
			{
				/* Invalid save type */
				fprintf(stderr, "ERROR: Invalid savetype argument\n\n");
				return print_usage(argv[0]);
			}

			continue;
		}
		/* The ROM file should be the last argument */
		if(i == argc)
		{
			write_file = fopen(arg, "r+b");
			if(!write_file)
			{
				fprintf(stderr, "ERROR: Cannot open '%s' for writing!\n", arg);
				return STATUS_ERROR;
			}
			break;
		}
		else
		{
			fprintf(stderr, "ERROR: Unexpected extra arguments\n\n");
			return print_usage(argv[0]);
		}
	}

	if(!write_file)
	{
		fprintf(stderr, "ERROR: Expected file argument\n\n");
		return print_usage(argv[0]);
	}

	if(force_rtc && (save_type == SAVETYPE_EEPROM4K || save_type == SAVETYPE_EEPROM16K))
	{
		fprintf(stderr, "WARNING: The combination of EEPROM + RTC does not work on EverDrive!\n");
	}

	uint8_t config = save_type | (force_rtc ? 1 : 0) + (region_free ? 2 : 0);

	const char cart_id[CART_ID_SIZE] = {'E', 'D'};
	fseek(write_file, CART_ID_OFFSET, SEEK_SET);
	fwrite(cart_id, 1, CART_ID_SIZE, write_file);
	fseek(write_file, VERSION_OFFSET, SEEK_SET);
	fwrite(&config, 1, VERSION_SIZE, write_file);

	fclose(write_file);

	return STATUS_OK;
}
