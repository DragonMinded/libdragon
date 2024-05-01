#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include "binout.h"

#define STBDS_NO_SHORT_NAMES
#define STB_DS_IMPLEMENTATION //Hack to get tools to compile
#include "stb_ds.h"

struct placeholder_data {
	int offset;
	int *pending_offsets;
};

struct {
	char *key;
	struct placeholder_data value;
} *placeholder_hash = NULL;

void w8(FILE *f, uint8_t v) 
{
    fputc(v, f);
}

void w16(FILE *f, uint16_t v)
{
    w8(f, v >> 8);
    w8(f, v & 0xff);
}

void w32(FILE *f, uint32_t v)
{
    w16(f, v >> 16);
    w16(f, v & 0xffff);
}

int w32_placeholder(FILE *f)
{
    int pos = ftell(f);
    w32(f, 0);
    return pos;
}

void w32_at(FILE *f, int pos, uint32_t v)
{
    int cur = ftell(f);
    assert(cur >= 0);  // fail on pipes
    fseek(f, pos, SEEK_SET);
    w32(f, v);
    fseek(f, cur, SEEK_SET);
}

void walign(FILE *f, int align)
{ 
    int pos = ftell(f);
    assert(pos >= 0);  // fail on pipes
    while (pos++ % align) w8(f, 0);
}

void wpad(FILE *f, int size)
{
    while (size--) {
        w8(f, 0);
    }
}

struct placeholder_data *__placeholder_get_data(const char *name)
{
	ptrdiff_t index = stbds_shgeti(placeholder_hash, name);
	if(index == -1) {
		struct placeholder_data default_value = {-1, NULL};
		index = stbds_shlen(placeholder_hash);
		stbds_shput(placeholder_hash, name, default_value);
	}
	return &placeholder_hash[index].value;
}

void __placeholder_make(FILE *file, int offset, const char *name)
{
	if(placeholder_hash == NULL) {
		stbds_sh_new_arena(placeholder_hash);
	}
	struct placeholder_data *data = __placeholder_get_data(name);
	data->offset = offset;
	for(size_t i=0; i<stbds_arrlenu(data->pending_offsets); i++) {
		w32_at(file, data->pending_offsets[i], data->offset);
	}
	stbds_arrfree(data->pending_offsets);
}

void placeholder_setv(FILE *file, const char *format, va_list arg)
{
	char *name = NULL;
	vasprintf(&name, format, arg);
	__placeholder_make(file, ftell(file), name);
	free(name);
}

void placeholder_set(FILE *file, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	placeholder_setv(file, format, args);
	va_end(args);
}

void placeholder_setv_offset(FILE *file, int offset, const char *format, va_list arg)
{
	char *name = NULL;
	vasprintf(&name, format, arg);
	__placeholder_make(file, offset, name);
	free(name);
}

void placeholder_set_offset(FILE *file, int offset, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	placeholder_setv_offset(file, offset, format, args);
	va_end(args);
}


void __w32_placeholder_named(FILE *file, const char *name)
{
	if(placeholder_hash == NULL) {
		stbds_sh_new_arena(placeholder_hash);
	}
	struct placeholder_data *data = __placeholder_get_data(name);
	if(data->offset == -1) {
		stbds_arrpush(data->pending_offsets, ftell(file));
		w32(file, 0);
	} else {
		w32(file, data->offset);
	}
}

void w32_placeholdervf(FILE *file, const char *format, va_list arg)
{
	char *name = NULL;
	vasprintf(&name, format, arg);
	__w32_placeholder_named(file, name);
	free(name);
}

void w32_placeholderf(FILE *file, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	w32_placeholdervf(file, format, args);
	va_end(args);
}

void placeholder_clear()
{
	for(size_t i=0; i<stbds_shlenu(placeholder_hash); i++) {
		stbds_arrfree(placeholder_hash[i].value.pending_offsets);
	}
	stbds_shfree(placeholder_hash);
}
