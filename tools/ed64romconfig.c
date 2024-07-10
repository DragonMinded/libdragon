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

#define CONTROLLERTYPE_INVALID 0xFE
#define CONTROLLERTYPE_N64 0x00
#define CONTROLLERTYPE_N64_WITH_RUMBLEPAK 0x01
#define CONTROLLERTYPE_N64_WITH_CONTROLLERPAK 0x02
#define CONTROLLERTYPE_N64_WITH_TRANSFERPAK 0x03
#define CONTROLLERTYPE_NONE 0xFF
#define CONTROLLERTYPE_N64_MOUSE 0x80
#define CONTROLLERTYPE_VRU 0x81
#define CONTROLLERTYPE_GAMECUBE 0x82
#define CONTROLLERTYPE_RANDNET_KEYBOARD 0x83
#define CONTROLLERTYPE_GAMECUBE_KEYBOARD 0x84

#define CART_ID_OFFSET 0x3C
#define CART_ID_SIZE   2
#define VERSION_OFFSET 0x3F
#define VERSION_SIZE   1
#define CONTROLLERTYPE1_OFFSET 0x34
#define CONTROLLERTYPE2_OFFSET 0x35
#define CONTROLLERTYPE3_OFFSET 0x36
#define CONTROLLERTYPE4_OFFSET 0x37
#define CONTROLLERTYPE_SIZE 1

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
	fprintf(stderr, "\t-w, --savetype <type>           Declare cartridge save type.\n");
	fprintf(stderr, "\t-c, --rtc                       Declare real-time clock support.\n");
	fprintf(stderr, "\t-r, --regionfree                Declare region-free ROM.\n");
	fprintf(stderr, "\t-1, --controller1 <type>        Define controller 1 hardware type. <type> should be one of:\n");
	fprintf(stderr, "\t    n64                         N64 controller without attachments\n");
	fprintf(stderr, "\t    n64,pak=rumble              N64 controller with Rumble Pak\n");
	fprintf(stderr, "\t    n64,pak=controller          N64 controller with Controller Pak\n");
	fprintf(stderr, "\t    n64,pak=transfer            N64 controller with Transfer Pak\n");
	fprintf(stderr, "\t    none                        Nothing attached to this port\n");
	fprintf(stderr, "\t    mouse                       N64 mouse\n");
	fprintf(stderr, "\t    vru                         VRU\n");
	fprintf(stderr, "\t    gamecube                    GameCube controller\n");
	fprintf(stderr, "\t    randnetkeyboard             Randnet keyboard\n");
	fprintf(stderr, "\t    gamecubekeyboard            GameCube keyboard\n");
	fprintf(stderr, "\t-2, --controller2 <type>        Define controller 2 hardware type. For <type>, see --controller1.\n");
	fprintf(stderr, "\t-3, --controller3 <type>        Define controller 3 hardware type. For <type>, see --controller1.\n");
	fprintf(stderr, "\t-4, --controller4 <type>        Define controller 4 hardware type. For <type>, see --controller1.\n");
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

/**
 * Corresponds to the Advanced Homebrew ROM Header values:
 * @see https://n64brew.dev/wiki/ROM_Header#Advanced_Homebrew_ROM_Header (offset 0x34)
 */
uint8_t parse_controller_type(const char* arg)
{
	const uint8_t n64_pak_prefix_string_length = 8;

	if(!strncmp(arg, "n64,pak=", n64_pak_prefix_string_length))
	{
		const char* pak_type_substring = arg + n64_pak_prefix_string_length;
		if(!strcmp(pak_type_substring, "rumble")) return CONTROLLERTYPE_N64_WITH_RUMBLEPAK;
		if(!strcmp(pak_type_substring, "controller")) return CONTROLLERTYPE_N64_WITH_CONTROLLERPAK;
		if(!strcmp(pak_type_substring, "transfer")) return CONTROLLERTYPE_N64_WITH_TRANSFERPAK;
	}
	if(!strcmp(arg, "n64")) return CONTROLLERTYPE_N64;
	if(!strcmp(arg, "none")) return CONTROLLERTYPE_NONE;
	if(!strcmp(arg, "mouse")) return CONTROLLERTYPE_N64_MOUSE;
	if(!strcmp(arg, "vru")) return CONTROLLERTYPE_VRU;
	if(!strcmp(arg, "gamecube")) return CONTROLLERTYPE_GAMECUBE;
	if(!strcmp(arg, "randnetkeyboard")) return CONTROLLERTYPE_RANDNET_KEYBOARD;
	if(!strcmp(arg, "gamecubekeyboard")) return CONTROLLERTYPE_GAMECUBE_KEYBOARD;

	return CONTROLLERTYPE_INVALID;
}

int main(int argc, char *argv[])
{
	FILE * write_file = NULL;
	bool force_rtc = false;
	bool region_free = false;
	uint8_t save_type = SAVETYPE_NONE;
	uint8_t controller_type1 = CONTROLLERTYPE_N64;
	uint8_t controller_type2 = CONTROLLERTYPE_N64;
	uint8_t controller_type3 = CONTROLLERTYPE_N64;
	uint8_t controller_type4 = CONTROLLERTYPE_N64;
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
		if(check_flag(arg, "-1", "--controller1"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to controller1 flag\n\n");
				return print_usage(argv[0]);
			}

			controller_type1 = parse_controller_type(argv[i++]);

			if(controller_type1 == CONTROLLERTYPE_INVALID)
			{
				/* Invalid controller type*/
				fprintf(stderr, "ERROR: Invalid controller type argument\n\n");
				return print_usage(argv[0]);
			}

			continue;
		}
		if(check_flag(arg, "-2", "--controller2"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to controller2 flag\n\n");
				return print_usage(argv[0]);
			}

			controller_type2 = parse_controller_type(argv[i++]);

			if(controller_type2 == CONTROLLERTYPE_INVALID)
			{
				/* Invalid controller type*/
				fprintf(stderr, "ERROR: Invalid controller type argument\n\n");
				return print_usage(argv[0]);
			}
			
			continue;
		}
		if(check_flag(arg, "-3", "--controller3"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to controller3 flag\n\n");
				return print_usage(argv[0]);
			}

			controller_type3 = parse_controller_type(argv[i++]);

			if(controller_type3 == CONTROLLERTYPE_INVALID)
			{
				/* Invalid controller type*/
				fprintf(stderr, "ERROR: Invalid controller type argument\n\n");
				return print_usage(argv[0]);
			}
			
			continue;
		}
		if(check_flag(arg, "-4", "--controller4"))
		{
			if(i >= argc)
			{
				/* Expected another argument */
				fprintf(stderr, "ERROR: Expected an argument to controller4 flag\n\n");
				return print_usage(argv[0]);
			}

			controller_type4 = parse_controller_type(argv[i++]);

			if(controller_type4 == CONTROLLERTYPE_INVALID)
			{
				/* Invalid controller type*/
				fprintf(stderr, "ERROR: Invalid controller type argument\n\n");
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

	uint8_t config = save_type | (force_rtc ? 1 : 0) | (region_free ? 2 : 0);

	const char cart_id[CART_ID_SIZE] = {'E', 'D'};
	fseek(write_file, CART_ID_OFFSET, SEEK_SET);
	fwrite(cart_id, 1, CART_ID_SIZE, write_file);
	fseek(write_file, VERSION_OFFSET, SEEK_SET);
	fwrite(&config, 1, VERSION_SIZE, write_file);
	fseek(write_file, CONTROLLERTYPE1_OFFSET, SEEK_SET);
	fwrite(&controller_type1, 1, CONTROLLERTYPE_SIZE, write_file);
	fseek(write_file, CONTROLLERTYPE2_OFFSET, SEEK_SET);
	fwrite(&controller_type2, 1, CONTROLLERTYPE_SIZE, write_file);
	fseek(write_file, CONTROLLERTYPE3_OFFSET, SEEK_SET);
	fwrite(&controller_type3, 1, CONTROLLERTYPE_SIZE, write_file);
	fseek(write_file, CONTROLLERTYPE4_OFFSET, SEEK_SET);
	fwrite(&controller_type4, 1, CONTROLLERTYPE_SIZE, write_file);

	fclose(write_file);

	return STATUS_OK;
}
