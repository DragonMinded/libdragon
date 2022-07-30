#include "yuv.h"
#include "yuv_internal.h"
#include "rsp.h"
#include "rspq.h"
#include "n64sys.h"
#include "debug.h"

static uint32_t ovl_id;

static void yuv_assert_handler(rsp_snapshot_t *state, uint16_t code) {
	switch (code) {
	case ASSERT_INVALID_INPUT_Y:
		printf("Input buffer for Y plane was not configured\n");
		break;
	case ASSERT_INVALID_INPUT_CB:
		printf("Input buffer for CB plane was not configured\n");
		break;
	case ASSERT_INVALID_INPUT_CR:
		printf("Input buffer for CR plane was not configured\n");
		break;
	case ASSERT_INVALID_OUTPUT:
		printf("Output buffer was not configured\n");
		break;
	}
}

DEFINE_RSP_UCODE(rsp_yuv,
	.assert_handler = yuv_assert_handler);

#define CMD_YUV_SET_INPUT         0x0
#define CMD_YUV_SET_OUTPUT        0x1
#define CMD_YUV_INTERLEAVE_32X16  0x2

void yuv_init(void)
{
	static bool init = false;
	if (!init) {
		init = true;

		rspq_init();
		ovl_id = rspq_overlay_register(&rsp_yuv);
	}
}

void yuv_set_input_buffer(uint8_t *y, uint8_t *cb, uint8_t *cr, int y_pitch)
{
	rspq_write(ovl_id, CMD_YUV_SET_INPUT, 
		PhysicalAddr(y), PhysicalAddr(cb), PhysicalAddr(cr), y_pitch);
}

void yuv_set_output_buffer(uint8_t *out, int out_pitch)
{
	rspq_write(ovl_id, CMD_YUV_SET_OUTPUT,
		PhysicalAddr(out), out_pitch);
}

void yuv_interleave_block_32x16(int x0, int y0)
{
	rspq_write(ovl_id, CMD_YUV_INTERLEAVE_32X16,
		(x0<<12) | y0);
}
