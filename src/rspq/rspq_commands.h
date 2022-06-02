#ifndef __LIBDRAGON_RSPQ_COMMANDS_H
#define __LIBDRAGON_RSPQ_COMMANDS_H

/**
 * RSPQ internal commands (overlay 0)
 */
enum {
    /**
     * @brief RSPQ command: Invalid
     * 
     * Reserved ID for invalid command. This is used as a marker so that RSP knows
     * when it has caught up with CPU and reached an empty portion of the buffer.
     */
    RSPQ_CMD_INVALID           = 0x00,

    /**
     * @brief RSPQ command: No-op
     * 
     * This commands does nothing. It can be useful for debugging purposes.
     */
    RSPQ_CMD_NOOP              = 0x01,

    /**
     * @brief RSPQ command: Jump to another buffer
     * 
     * This commands tells the RSP to start fetching commands from a new address.
     * It is mainly used internally to implement the queue as a ring buffer (jumping
     * at the start when we reach the end of the buffer).
     */
    RSPQ_CMD_JUMP              = 0x02,

    /**
     * @brief RSPQ command: Call a block
     * 
     * This command is used by the block functions to implement the execution of
     * a block. It tells RSP to starts fetching commands from the block address,
     * saving the current address in an internal save slot in DMEM, from which
     * it will be recovered by CMD_RET. Using multiple slots allow for nested
     * calls.
     */    
    RSPQ_CMD_CALL              = 0x03,

    /**
     * @brief RSPQ command: Return from a block
     * 
     * This command tells the RSP to recover the buffer address from a save slot
     * (from which it was currently saved by a CALL command) and begin fetching
     * commands from there. It is used to finish the execution of a block.
     */
    RSPQ_CMD_RET               = 0x04,

    /**
     * @brief RSPQ command: DMA transfer
     * 
     * This commands runs a DMA transfer (either DRAM to DMEM, or DMEM to DRAM).
     * It is used by #rspq_overlay_register to register a new overlay table into
     * DMEM while the RSP is already running (to allow for overlays to be
     * registered even after boot), and can be used by the users to perform
     * manual DMA transfers to and from DMEM without risking a conflict with the
     * RSP itself.
     */
    RSPQ_CMD_DMA               = 0x05,

    /**
     * @brief RSPQ Command: write SP_STATUS register
     * 
     * This command asks the RSP to write to the SP_STATUS register. It is normally
     * used to set/clear signals or to raise RSP interrupts.
     */
    RSPQ_CMD_WRITE_STATUS      = 0x06,

    /**
     * @brief RSPQ Command: Swap lowpri/highpri buffers
     * 
     * This command is used as part of the highpri feature. It allows to switch
     * between lowpri and highpri queue, by saving the current buffer pointer
     * in a special save slot, and restoring the buffer pointer of the other
     * queue from another slot. In addition, it also writes to SP_STATUS, to
     * be able to adjust signals: entering highpri mode requires clearing
     * SIG_HIGHPRI_REQUESTED and setting SIG_HIGHPRI_RUNNING; exiting highpri
     * mode requires clearing SIG_HIGHPRI_RUNNING.
     * 
     * The command is called internally by RSP to switch to highpri when the
     * SIG_HIGHPRI_REQUESTED is found set; then it is explicitly enqueued by the
     * CPU when the highpri queue is finished to switch back to lowpri
     * (see #rspq_highpri_end).
     */
    RSPQ_CMD_SWAP_BUFFERS      = 0x07,

    /**
     * @brief RSPQ Command: Test and write SP_STATUS register
     * 
     * This commands does a test-and-write sequence on the SP_STATUS register: first,
     * it waits for a certain mask of bits to become zero, looping on it. Then
     * it writes a mask to the register. It is used as part of the syncpoint
     * feature to raise RSP interrupts, while waiting for the previous
     * interrupt to be processed (coalescing interrupts would cause syncpoints
     * to be missed).
     */
    RSPQ_CMD_TEST_WRITE_STATUS = 0x08,

    /**
     * @brief RSPQ command: Push commands to RDP
     * 
     * This command will send a buffer of RDP commands in RDRAM to the RDP.
     * Additionally, it will perform a write to SP_STATUS when the buffer is 
     * not contiguous with the previous one. This is used for synchronization
     * with the CPU.
     */
    RSPQ_CMD_RDP               = 0x09,

    /**
     * @brief RSPQ command: Wait for RDP to be idle.
     * 
     * This command will let the RSP spin-wait until the RDP is idle (that is,
     * the DP_STATUS_BUSY bit in COP0_DP_STATUS goes to 0). Notice that the
     * RDP is fully asynchronous, and reading DP_STATUS_BUSY basically makes
     * sense only after a RDP SYNC_FULL command (#rdpq_sync_full()), when it
     * really does make sure that all previous commands have finished
     * running.
     */
    RSPQ_CMD_RDP_WAIT_IDLE     = 0x0A
};

/** @brief Write an internal command to the RSP queue */
#define rspq_int_write(cmd_id, ...) rspq_write(0, cmd_id, ##__VA_ARGS__)

typedef struct rdpq_block_s rdpq_block_t;

/** @brief A pre-built block of commands */
typedef struct rspq_block_s {
    uint32_t nesting_level;     ///< Nesting level of the block
    rdpq_block_t *rdp_block;
    uint32_t cmds[];            ///< Block contents (commands)
} rspq_block_t;

#endif
