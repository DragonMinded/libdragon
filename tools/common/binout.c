#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

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
	ptrdiff_t index = shgeti(placeholder_hash, name);
	if(index == -1) {
		struct placeholder_data default_value = {-1, NULL};
		index = shlen(placeholder_hash);
		shput(placeholder_hash, name, default_value);
	}
	return &placeholder_hash[index].value;
}

void placeholder_register(FILE *file, const char *name)
{
	if(placeholder_hash == NULL) {
		sh_new_arena(placeholder_hash);
	}
	struct placeholder_data *data = __placeholder_get_data(name);
	data->offset = ftell(file);
	for(size_t i=0; i<arrlenu(data->pending_offsets); i++) {
		w32_at(file, data->pending_offsets[i], data->offset);
	}
	arrfree(data->pending_offsets);
}

void placeholder_registervf(FILE *file, const char *format, va_list arg)
{
	char *name = NULL;
	vasprintf(&name, format, arg);
	placeholder_register(file, name);
	free(name);
}

void placeholder_registerf(FILE *file, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	placeholder_registervf(file, format, args);
	va_end(args);
}

void placeholder_add(FILE *file, const char *name)
{
	if(placeholder_hash == NULL) {
		sh_new_arena(placeholder_hash);
	}
	struct placeholder_data *data = __placeholder_get_data(name);
	if(data->offset == -1) {
		arrpush(data->pending_offsets, ftell(file));
		w32(file, 0);
	} else {
		w32(file, data->offset);
	}
}

void placeholder_addvf(FILE *file, const char *format, va_list arg)
{
	char *name = NULL;
	vasprintf(&name, format, arg);
	placeholder_add(file, name);
	free(name);
}

void placeholder_addf(FILE *file, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	placeholder_addvf(file, format, args);
	va_end(args);
}


void placeholder_clear()
{
	for(size_t i=0; i<shlenu(placeholder_hash); i++) {
		arrfree(placeholder_hash[i].value.pending_offsets);
	}
	shfree(placeholder_hash);
}
