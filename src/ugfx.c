#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ugfx.h"
#include "rsp.h"
#include "n64sys.h"

#define UGFX_PARAMS_ADDRESS 0x0

DEFINE_RSP_UCODE(rsp_ugfx);

static void *ugfx_rdp_buffer;
static uint32_t ugfx_rdp_buffer_size;

extern void *__safe_buffer[];

typedef struct ugfx_ucode_params_t {
    uint32_t input;
    uint32_t input_size;
    uint32_t output_buf;
    uint32_t output_buf_size;
} __attribute__ ((aligned (16))) ugfx_ucode_params_t;

void ugfx_matrix_from_column_major(ugfx_matrix_t *dest, const float *source)
{
    for (size_t i = 0; i < 16; ++i)
    {
        size_t c = i / 4;
        size_t r = i % 4;
        int32_t fixed = float_to_fixed(source[i], 16);
        dest->integer[c][r] = (uint16_t)((fixed & 0xFFFF0000) >> 16);
        dest->fraction[c][r] = (uint16_t)(fixed & 0x0000FFFF);
    }
}

void ugfx_matrix_from_row_major(ugfx_matrix_t *dest, const float *source)
{
    for (size_t i = 0; i < 16; ++i)
    {
        size_t c = i % 4;
        size_t r = i / 4;
        int32_t fixed = float_to_fixed(source[i], 16);
        dest->integer[c][r] = (uint16_t)((fixed & 0xFFFF0000) >> 16);
        dest->fraction[c][r] = (uint16_t)(fixed & 0x0000FFFF);
    }
}

ugfx_buffer_t* ugfx_buffer_new(uint32_t capacity)
{
    ugfx_buffer_t *buffer = malloc(sizeof(ugfx_buffer_t));

    buffer->data = malloc(capacity * sizeof(ugfx_command_t));
    buffer->capacity = capacity;
    buffer->length = 0;

    return buffer;
}

void ugfx_buffer_free(ugfx_buffer_t *buffer)
{
    free(buffer->data);
    free(buffer);
}

void ugfx_buffer_clear(ugfx_buffer_t *buffer)
{
    buffer->length = 0;
}

void ugfx_buffer_check_size(ugfx_buffer_t *buffer, uint32_t new_size)
{
    if (new_size > buffer->capacity)
    {
        do 
        {
            // Round to next power of 2?
            buffer->capacity = buffer->capacity << 1;
        } while (new_size > buffer->capacity);

        buffer->data = realloc(buffer->data, buffer->capacity * sizeof(ugfx_command_t));
    }
}

void ugfx_buffer_push(ugfx_buffer_t *buffer, ugfx_command_t command)
{
    ugfx_buffer_check_size(buffer, buffer->length + 1);
    buffer->data[buffer->length++] = command;
}

void ugfx_buffer_insert(ugfx_buffer_t *buffer, const ugfx_command_t *commands, uint32_t count)
{
    ugfx_buffer_check_size(buffer, buffer->length + count);
    memcpy(buffer->data + buffer->length, commands, count * sizeof(ugfx_command_t));
    buffer->length += count;
}

void ugfx_init(uint32_t rdp_buffer_size)
{
    ugfx_rdp_buffer_size = rdp_buffer_size;
    ugfx_rdp_buffer = malloc(ugfx_rdp_buffer_size);
    rsp_init();
}

void ugfx_load(const ugfx_command_t *commands, uint32_t commands_length)
{
    ugfx_ucode_params_t params =
    {
        .input = (uint32_t)PhysicalAddr(commands),
        .input_size = commands_length * sizeof(ugfx_command_t),
        .output_buf = (uint32_t)PhysicalAddr(ugfx_rdp_buffer),
        .output_buf_size = ugfx_rdp_buffer_size,
    };

    data_cache_hit_writeback(&params, sizeof(params));

    rsp_load(&rsp_ugfx);
    rsp_load_data(&params, sizeof(params), UGFX_PARAMS_ADDRESS);
}

void ugfx_close()
{
    free(ugfx_rdp_buffer);
    ugfx_rdp_buffer = NULL;
    ugfx_rdp_buffer_size = 0;
}

static inline int32_t ugfx_pixel_size_from_bitdepth(bitdepth_t bitdepth)
{
    switch (bitdepth)
    {
        case DEPTH_16_BPP:
            return UGFX_PIXEL_SIZE_16B;
        case DEPTH_32_BPP:
            return UGFX_PIXEL_SIZE_32B;
        default:
            return -1;
    }
}

ugfx_command_t ugfx_set_display(display_context_t disp)
{
    if (disp == 0)
    {
        return 0;
    }

    int32_t pixel_size = ugfx_pixel_size_from_bitdepth(display_get_bitdepth());
    if (pixel_size < 0) 
    {
        return 0;
    }

    return ugfx_set_color_image(__safe_buffer[disp - 1], UGFX_FORMAT_RGBA, pixel_size, display_get_width() - 1);
}

void ugfx_viewport_init(ugfx_viewport_t *viewport, int16_t left, int16_t top, int16_t right, int16_t bottom)
{
    left = float_to_fixed(left, 2);
    top = float_to_fixed(top, 2);
    right = float_to_fixed(right, 2);
    bottom = float_to_fixed(bottom, 2);

    int16_t half_width = (right - left) / 2;
    int16_t half_height = (bottom - top) / 2;

    *viewport = (ugfx_viewport_t) {
        .scale = { 
            half_width,
            -half_height,
            Z_MAX / 2,
            0
        },
        .offset = { 
            left + half_width,
            top + half_height,
            Z_MAX / 2,
            0 
        }
    };
}
