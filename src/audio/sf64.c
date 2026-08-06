/**
 * @file sf64.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief SF64 sound bank loader
 * @ingroup mixer
 */
#include "sf64.h"
#include "sf64_internal.h"
#include "wav64.h"
#include "wav64_internal.h"
#include "debug.h"
#include "asset_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

sf64_bank_t *sf64_load(const char *fn)
{
	int fd = must_open(fn);

	sf64_header_t head;
	read(fd, &head, sizeof(head));
	if (memcmp(head.magic, SF64_ID, 4) != 0) {
		assertf(memcmp(head.magic, "RIFF", 4) != 0,
			"cannot load SF2 file: %s\nPlease convert to SF64 with audioconv64", fn);
		assertf(0, "cannot load SF64 file: %s\nFile corrupted", fn);
	}
	assertf(head.version == 1,
		"cannot load SF64 file: %s\nVersion %d not supported, please convert again with audioconv64",
		fn, head.version);

	lseek(fd, head.metadata_offset, SEEK_SET);
	int meta_sz = head.metadata_size;
	void *meta = asset_loadfd(fd, &meta_sz);

	int tables = (int)head.num_presets * (int)sizeof(sf64_preset_t)
		+ (int)head.num_regions * (int)sizeof(sf64_region_t)
		+ (int)head.num_samples * (int)sizeof(sf64_sample_t);
	assertf(meta_sz >= tables, "cannot load SF64 file: %s\nTruncated metadata", fn);

	sf64_bank_t *bank = malloc(sizeof(*bank) + head.num_samples * sizeof(wav64_t *));
	assert(bank);
	bank->fd = fd;
	bank->num_presets = head.num_presets;
	bank->num_regions = head.num_regions;
	bank->num_samples = head.num_samples;
	bank->meta = meta;
	bank->presets = (sf64_preset_t *)meta;
	bank->regions = (sf64_region_t *)(bank->presets + head.num_presets);
	bank->samples = (sf64_sample_t *)(bank->regions + head.num_regions);

	for (int i = 0; i < head.num_samples; i++) {
		char name[128];
		snprintf(name, sizeof(name), "%s[%d]", fn, i);
		lseek(fd, bank->samples[i].wav64_offset, SEEK_SET);
		bank->waves[i] = wav64_loadfd(fd, name, NULL);
	}

	return bank;
}

void sf64_close(sf64_bank_t *bank)
{
	assert(bank);
	for (int i = 0; i < bank->num_samples; i++)
		wav64_close(bank->waves[i]);
	close(bank->fd);
	free(bank->meta);
	free(bank);
}

int sf64_find_preset(sf64_bank_t *bank, int midi_bank, int program)
{
	assert(bank);
	for (int i = 0; i < bank->num_presets; i++) {
		if (bank->presets[i].bank == midi_bank && bank->presets[i].program == program)
			return i;
	}
	return -1;
}
