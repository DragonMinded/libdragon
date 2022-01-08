#include "yuv.h"
#include "yuv_internal.h"
#include "rsp.h"
#include "rspq.h"
#include "n64sys.h"
#include "debug.h"

DEFINE_RSP_UCODE(rsp_yuv);

#define CMD_YUV_SET_INPUT         0x40
#define CMD_YUV_SET_OUTPUT        0x41
#define CMD_YUV_INTERLEAVE_32X16  0x42

void yuv_init(void)
{
	static bool init = false;
	if (!init) {
		init = true;

		rsp_ucode_register_assert(&rsp_yuv, ASSERT_INVALID_INPUT_Y,
			"Input buffer for Y plane was not configured", NULL);
		rsp_ucode_register_assert(&rsp_yuv, ASSERT_INVALID_INPUT_CB,
			"Input buffer for CB plane was not configured", NULL);
		rsp_ucode_register_assert(&rsp_yuv, ASSERT_INVALID_INPUT_CR,
			"Input buffer for CR plane was not configured", NULL);
		rsp_ucode_register_assert(&rsp_yuv, ASSERT_INVALID_OUTPUT,
			"Output buffer was not configured", NULL);

		rspq_init();
		rspq_overlay_register(&rsp_yuv, 0x4);
	}
}

void yuv_set_input_buffer(uint8_t *y, uint8_t *cb, uint8_t *cr, int y_pitch)
{
	rspq_write(CMD_YUV_SET_INPUT, 
		PhysicalAddr(y), PhysicalAddr(cb), PhysicalAddr(cr), y_pitch);
}

void yuv_set_output_buffer(uint8_t *out, int out_pitch)
{
	rspq_write(CMD_YUV_SET_OUTPUT,
		PhysicalAddr(out), out_pitch);
}

void yuv_interleave_block_32x16(int x0, int y0)
{
	rspq_write(CMD_YUV_INTERLEAVE_32X16,
		(x0<<12) | y0);
}
