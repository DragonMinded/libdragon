/**
 * @file joybus_rtc.c
 * @brief Joybus Real-Time Clock Utilities
 * @ingroup rtc
 */

#include "debug.h"
#include "joybus_commands.h"
#include "joybus_internal.h"
#include "joybus_rtc.h"
#include "libcart/cart.h"
#include "n64sys.h"
#include "rtc_utils.h"

/**
 * @addtogroup joybus_rtc
 * @{
 */

// MARK: Types

/** @brief Joybus RTC Block Types */
typedef enum
{
    /** @brief Control block */
    JOYBUS_RTC_BLOCK_CONTROL = 0,
    /** @brief Unused block */
    JOYBUS_RTC_BLOCK_UNUSED = 1,
    /** @brief Time block */
    JOYBUS_RTC_BLOCK_TIME = 2,
} joybus_rtc_block_t;

/** @brief Joybus RTC Status Byte */
typedef union
{
    /** @brief raw byte value */
    uint8_t byte;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        /** @brief "RTC is stopped" bit */
        unsigned stopped : 1;
        unsigned         : 7;
    /// @cond
    };
    /// @endcond
} joybus_rtc_status_t;

/** @brief Joybus RTC Data Chunk */
typedef union
{
    /** @brief double-word access of data */
    uint64_t dword;
    /** @brief byte-wise access of data */
    uint8_t bytes[8];
} joybus_rtc_data_t;

/** @brief Joybus RTC Control Data */
typedef union
{
    /** @brief Raw data chunk */
    joybus_rtc_data_t data;
    /// @cond
    struct __attribute__((packed))
    {
    /// @endcond
        unsigned             : 6;
        /** @brief Lock Block 1 bit */
        unsigned lock_block1 : 1;
        /** @brief Lock Block 2 bit */
        unsigned lock_block2 : 1;
        unsigned             : 5;
        /** @brief Stop bit */
        unsigned stop        : 1;
        unsigned             : 2;
    /// @cond
    };
    /// @endcond
} joybus_rtc_control_t;

// MARK: Constants

/** @brief Joybus RTC port number */
#define JOYBUS_RTC_PORT 4

/**
 * @brief Duration (in milliseconds) to wait after writing a Joybus RTC block.
 *
 * The software should wait for the previous RTC write to finish before issuing
 * another Joybus RTC command. Ideally, you could read the RTC status byte to
 * determine when to proceed, but some RTC reproductions do not correctly
 * implement the RTC status response, so a delay is used for compatibility.
 */
#define JOYBUS_RTC_WRITE_BLOCK_DELAY 20

// MARK: Static functions

/** @brief Decode the Joybus RTC date/time block data into a struct tm */
static time_t joybus_rtc_decode_time( const joybus_rtc_data_t * data )
{
    if (data->dword == 0) return 0;
    struct tm rtc_time = (struct tm){
        .tm_sec   = bcd_decode( data->bytes[0] ),
        .tm_min   = bcd_decode( data->bytes[1] ),
        .tm_hour  = bcd_decode( data->bytes[2] - 0x80 ),
        .tm_mday  = bcd_decode( data->bytes[3] ),
        .tm_wday  = bcd_decode( data->bytes[4] ),
        .tm_mon   = bcd_decode( data->bytes[5] ) - 1,
        .tm_year  = bcd_decode( data->bytes[6] ) + (bcd_decode( data->bytes[7] ) * 100),
    };
    return mktime( &rtc_time );
}

static void joybus_rtc_read_async(
    joybus_rtc_block_t block,
    joybus_callback_t callback,
    void *ctx
)
{
    joybus_cmd_rtc_read_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_READ_BLOCK,
        .block = block,
    } };
    // Allocate the Joybus operation block input and output buffers
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    // Skip commands on ports before the desired port offset
    size_t i = JOYBUS_RTC_PORT;
    // Set the command metadata
    input[i++] = sizeof(cmd.send);
    input[i++] = sizeof(cmd.recv);
    // Copy the send_data into the input buffer
    memcpy(&input[i], (void *)&cmd.send, sizeof(cmd.send));
    i += sizeof(cmd.send) + sizeof(cmd.recv);
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // Execute the Joybus operation asynchronously
    joybus_exec_async(input, callback, ctx);
}

static void joybus_rtc_write_async(
    joybus_rtc_block_t block,
    const joybus_rtc_data_t * data,
    joybus_callback_t callback,
    void *ctx
)
{
    joybus_cmd_rtc_write_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_WRITE_BLOCK,
        .block = block,
        .dword = data->dword
    } };
    // Allocate the Joybus operation block input and output buffers
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    // Skip commands on ports before the desired port offset
    size_t i = JOYBUS_RTC_PORT;
    // Set the command metadata
    input[i++] = sizeof(cmd.send);
    input[i++] = sizeof(cmd.recv);
    // Copy the send_data into the input buffer
    memcpy(&input[i], (void *)&cmd.send, sizeof(cmd.send));
    i += sizeof(cmd.send) + sizeof(cmd.recv);
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // Execute the Joybus operation asynchronously
    joybus_exec_async(input, callback, ctx);
}

/**
 * @brief Read a Joybus RTC block.
 *
 * @param block the RTC block to read
 * @param[out] data destination buffer
 *
 * @return Joybus RTC status byte
 */
static joybus_rtc_status_t joybus_rtc_read( joybus_rtc_block_t block, joybus_rtc_data_t * data )
{
    joybus_cmd_rtc_read_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_READ_BLOCK,
        .block = block,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    data->dword = cmd.recv.dword;
    return (joybus_rtc_status_t) { .byte = cmd.recv.status };
}

/**
 * @brief Write a Joybus RTC block.
 *
 * @param block the RTC block to write
 * @param[out] data source buffer
 *
 * @return Joybus RTC status byte
 */
static joybus_rtc_status_t joybus_rtc_write( joybus_rtc_block_t block, const joybus_rtc_data_t * data )
{
    joybus_cmd_rtc_write_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_WRITE_BLOCK,
        .block = block,
        .dword = data->dword
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    return (joybus_rtc_status_t) { .byte = cmd.recv.status };
}

// MARK: Detect functions

/** @brief Joybus RTC detect state enumeration */
typedef enum {
    JOYBUS_RTC_DETECT_INIT = 0, ///< Initial state
    JOYBUS_RTC_DETECT_PENDING,  ///< Detection is pending
    JOYBUS_RTC_DETECTED,        ///< RTC detected
    JOYBUS_RTC_NOT_DETECTED,    ///< RTC not detected
} joybus_rtc_detect_state_t;

static volatile joybus_rtc_detect_state_t joybus_rtc_detect_state = JOYBUS_RTC_DETECT_INIT;

static void joybus_rtc_detect_callback( uint64_t *out_dwords, void *ctx )
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joybus_rtc_detect_callback_t callback = ctx;
    joybus_cmd_identify_port_t *cmd = (void *)&out_bytes[JOYBUS_RTC_PORT + JOYBUS_COMMAND_METADATA_SIZE];
    bool detected = cmd->recv.identifier == JOYBUS_IDENTIFIER_CART_RTC;
    joybus_rtc_detect_state = detected ? JOYBUS_RTC_DETECTED : JOYBUS_RTC_NOT_DETECTED;
    debugf("joybus_rtc_detect_async: %s status 0x%02x\n", detected ? "detected" : "not detected", cmd->recv.status);
    callback( detected );
}

void joybus_rtc_detect_async( joybus_rtc_detect_callback_t callback )
{
    joybus_rtc_detect_state = JOYBUS_RTC_DETECT_PENDING;
    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_IDENTIFY,
    } };
    // Allocate the Joybus operation block input and output buffers
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    // Skip commands on ports before the desired port offset
    size_t i = JOYBUS_RTC_PORT;
    // Set the command metadata
    input[i++] = sizeof(cmd.send);
    input[i++] = sizeof(cmd.recv);
    // Copy the send_data into the input buffer
    memcpy( &input[i], (void *)&cmd.send, sizeof(cmd.send) );
    i += sizeof(cmd.send) + sizeof(cmd.recv);
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    // Execute the Joybus operation asynchronously
    debugf("joybus_rtc_detect_async: probing\n");
    joybus_exec_async( input, joybus_rtc_detect_callback, callback );
}

bool joybus_rtc_detect( void )
{
    switch( joybus_rtc_detect_state )
    {
        case JOYBUS_RTC_DETECTED:
            return true;
        case JOYBUS_RTC_NOT_DETECTED:
            return false;
        case JOYBUS_RTC_DETECT_INIT:
            joybus_rtc_detect_async(NULL);
            break;
        case JOYBUS_RTC_DETECT_PENDING:
            break;
    }
    while( joybus_rtc_detect_state == JOYBUS_RTC_DETECT_PENDING ){ /* Spinlock! */}
    return joybus_rtc_detect_state == JOYBUS_RTC_DETECTED;
}

// MARK: Control functions

static void joybus_rtc_set_stopped_write_callback( uint64_t *out_dwords, void *ctx )
{
    joybus_rtc_set_stopped_callback_t callback = ctx;
    if( callback ) callback();
    debugf("joybus_rtc_set_stopped_async: done\n");
}

static void joybus_rtc_set_stopped_read_callback( uint64_t *out_dwords, void *ctx )
{
    // Parse the Joybus command response
    const uint8_t *out_bytes = (void *)out_dwords;
    joybus_cmd_rtc_read_block_t *cmd = (void *)&out_bytes[JOYBUS_RTC_PORT + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_rtc_control_t control = { .data = { .dword = cmd->recv.dword } };
    debugf("joybus_rtc_set_stopped_async: control state (0x%llx)\n", control.data.dword);

    // Unpack the stop bit and the callback pointer from the context
    bool stop = ((uint32_t)ctx & 0x1);
    joybus_rtc_set_stopped_callback_t callback = (void *)((uint32_t)ctx & ~0x1);

    if( control.stop == stop )
    {
        debugf("joybus_rtc_set_stopped_async: already in desired state\n");
        if( callback ) callback();
    }
    else
    {
        control.stop = stop;
        control.lock_block1 = !stop;
        control.lock_block2 = !stop;
        debugf("joybus_rtc_set_stopped_async: writing control block (0x%llx)\n", control.data.dword);
        joybus_rtc_write_async(
            JOYBUS_RTC_BLOCK_CONTROL,
            &control.data,
            joybus_rtc_set_stopped_write_callback,
            callback
        );
    }
}

void joybus_rtc_set_stopped_async( bool stop, joybus_rtc_set_stopped_callback_t callback )
{
    // Pack the stop bit and the callback pointer into the context value
    void *ctx = (void *)((uint32_t)callback | (stop & 0x1));
    debugf("joybus_rtc_set_stopped_async: reading control block\n");
    joybus_rtc_read_async( JOYBUS_RTC_BLOCK_TIME, joybus_rtc_set_stopped_read_callback, ctx );
}

void joybus_rtc_set_stopped( bool stop )
{
    joybus_rtc_control_t control;
    debugf("joybus_rtc_set_stopped: reading control block\n");
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    debugf("joybus_rtc_set_stopped: control state (0x%llx)\n", control.data.dword);

    if ( control.stop != stop )
    {
        control.stop = stop;
        control.lock_block1 = !stop;
        control.lock_block2 = !stop;
        debugf("joybus_rtc_set_stopped: writing control block (0x%llx)\n", control.data.dword);
        joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    }
    else
    {
        debugf("joybus_rtc_set_stopped: already in desired state\n");
    }
}

bool joybus_rtc_is_stopped( void )
{
    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_IDENTIFY,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    joybus_rtc_status_t status = { .byte = cmd.recv.status };
    debugf("joybus_rtc_is_stopped: status (0x%02x)\n", status.byte);
    return status.stopped;
}

// MARK: Time functions

static void joybus_rtc_read_time_callback( uint64_t *out_dwords, void *ctx )
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joybus_rtc_read_time_callback_t callback = ctx;
    joybus_cmd_rtc_read_block_t *cmd = (void *)&out_bytes[JOYBUS_RTC_PORT + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_rtc_data_t data = { .dword = cmd->recv.dword };
    debugf("joybus_rtc_read_time_async: raw time (0x%llx)\n", data.dword);
    time_t decoded_time = joybus_rtc_decode_time( &data );

    struct tm * parsed_tm = gmtime( &decoded_time );
    debugf("joybus_rtc_read_time: parsed time (%04d-%02d-%02d %02d:%02d:%02d)\n",
        parsed_tm->tm_year + 1900, parsed_tm->tm_mon + 1, parsed_tm->tm_mday,
        parsed_tm->tm_hour, parsed_tm->tm_min, parsed_tm->tm_sec
    );

    callback( decoded_time );
}

void joybus_rtc_read_time_async( joybus_rtc_read_time_callback_t callback )
{
    debugf("joybus_rtc_read_time_async: reading time block\n");
    joybus_rtc_read_async( JOYBUS_RTC_BLOCK_TIME, joybus_rtc_read_time_callback, callback );
}

time_t joybus_rtc_read_time( void )
{
    joybus_rtc_data_t data = {0};
    debugf("joybus_rtc_read_time: reading time block\n");
    joybus_rtc_read( JOYBUS_RTC_BLOCK_TIME, &data );
    debugf("joybus_rtc_read_time: raw time (0x%llx)\n", data.dword);
    time_t decoded_time = joybus_rtc_decode_time( &data );

    struct tm * parsed_tm = gmtime( &decoded_time );
    debugf("joybus_rtc_read_time: parsed time (%04d-%02d-%02d %02d:%02d:%02d)\n",
        parsed_tm->tm_year + 1900, parsed_tm->tm_mon + 1, parsed_tm->tm_mday,
        parsed_tm->tm_hour, parsed_tm->tm_min, parsed_tm->tm_sec
    );

    return decoded_time;
}

bool joybus_rtc_set_time( time_t new_time )
{
    if (cart_type == CART_ED)
    {
        // Joybus RTC write is not supported on EverDrive 64 V3.
        // TODO: Research and implement ED64 V3-specific RTC write
        return false;
    }

    if (cart_type == CART_EDX)
    {
        // Joybus RTC write is not supported on EverDrive 64 X7.
        // TODO: Research and implement ED64 X7-specific RTC write
        return false;
    }

    debugf("joybus_rtc_set_time: reading control block\n");
    joybus_rtc_control_t control;
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    debugf("joybus_rtc_read_time: control state (0x%llx)\n", control.data.dword);

    /* Prepare the RTC to write the time */
    control.stop = true;
    control.lock_block1 = false;
    control.lock_block2 = false;
    debugf("joybus_rtc_set_time: writing control block (0x%llx)\n", control.data.dword);
    joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );

    /* Determine if the RTC implementation supports writes. */
    if( !joybus_rtc_is_stopped() )
    {
        debugf("joybus_rtc_set_time: rtc did not stop; aborting!\n");
        return false;
    }

    /* Write the time block to RTC */
    struct tm * rtc_time = gmtime( &new_time );
    debugf("joybus_rtc_set_time: parsed time (%02d:%02d:%02d %02d/%02d/%04d)\n",
        rtc_time->tm_hour, rtc_time->tm_min, rtc_time->tm_sec,
        rtc_time->tm_mon + 1, rtc_time->tm_mday, rtc_time->tm_year + 1900
    );
    joybus_rtc_data_t data = { .bytes = {
        bcd_encode( rtc_time->tm_sec ),
        bcd_encode( rtc_time->tm_min ),
        bcd_encode( rtc_time->tm_hour ) + 0x80,
        bcd_encode( rtc_time->tm_mday ),
        bcd_encode( rtc_time->tm_wday ),
        bcd_encode( rtc_time->tm_mon + 1 ),
        bcd_encode( rtc_time->tm_year ),
        bcd_encode( rtc_time->tm_year / 100 ),
    } };
    debugf("joybus_rtc_set_time: writing time block (0x%llx)\n", data.dword);
    joybus_rtc_write( JOYBUS_RTC_BLOCK_TIME, &data );
    wait_ms( JOYBUS_RTC_WRITE_BLOCK_DELAY );

    /* Put the RTC back into normal operating mode */
    control.stop = false;
    control.lock_block1 = true;
    control.lock_block2 = true;
    debugf("joybus_rtc_set_time: writing control block (0x%llx)\n", control.data.dword);
    joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );

    if( joybus_rtc_is_stopped() )
    {
        debugf("joybus_rtc_set_time: rtc did not restart?\n");
    }

    return true;
}

/** @} */ /* joybus_rtc */
