#include <rsp_queue.inc>

    .set noreorder
    .set at

    .data

    RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand Test2Cmd_test, 8           # 0x00
	RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
TEST2_STATE:    .quad 0
    RSPQ_EndSavedState

    .text

Test2Cmd_test:
    sw a0, %lo(TEST2_STATE) + 0x0
    jr ra
    sw a1, %lo(TEST2_STATE) + 0x4
    
