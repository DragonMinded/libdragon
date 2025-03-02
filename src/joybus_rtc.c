/**
 * @file joybus_rtc.c
 * @brief Joybus Real-Time Clock Utilities
 * @ingroup rtc
 */

#include "debug.h"
#include "ed64.h"
#include "ed64x.h"
#include "joybus_commands.h"
#include "joybus_internal.h"
#include "joybus_rtc.h"
#include "libcart/cart.h"
#include "n64sys.h"
#include "rtc_internal.h"

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
    /** @brief Empty block */
    JOYBUS_RTC_BLOCK_EMPTY = 3,
} joybus_rtc_block_t;

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
    // Super quick sanity check
    if (data->dword == 0) return RTC_EBADTIME;

    int sec = bcd_decode( data->bytes[0] );
    int min = bcd_decode( data->bytes[1] );
    int hour = bcd_decode( data->bytes[2] - 0x80 );
    int day = bcd_decode( data->bytes[3] );
    int dow = bcd_decode( data->bytes[4] );
    int month = bcd_decode( data->bytes[5] );
    int year = bcd_decode( data->bytes[6] );
    int century = bcd_decode( data->bytes[7] );

    // Extremely basic sanity-check on the date and time
    if(
        century > 1 ||
        month == 0 || month > 12 ||
        day == 0 || day > 31 || dow > 6 ||
        hour >= 24 || min >= 60 || sec >= 60
    )
    {
        return RTC_EBADTIME;
    }

    struct tm rtc_time = (struct tm){
        .tm_sec   = sec,
        .tm_min   = min,
        .tm_hour  = hour,
        .tm_mday  = day,
        .tm_mon   = month - 1,
        .tm_year  = year + (century * 100),
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
static void joybus_rtc_read( joybus_rtc_block_t block, joybus_rtc_data_t * data )
{
    joybus_cmd_rtc_read_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_READ_BLOCK,
        .block = block,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    data->dword = cmd.recv.dword;
    // DO NOT RELY ON THE STATUS BYTE: Some Joybus RTC implementations don't set it!
}

/**
 * @brief Write a Joybus RTC block.
 *
 * @param block the RTC block to write
 * @param[out] data source buffer
 *
 * @return Joybus RTC status byte
 */
static void joybus_rtc_write( joybus_rtc_block_t block, const joybus_rtc_data_t * data )
{
    joybus_cmd_rtc_write_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_WRITE_BLOCK,
        .block = block,
        .dword = data->dword
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    // DO NOT RELY ON THE STATUS BYTE: Some Joybus RTC implementations don't set it!
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
static volatile joybus_rtc_status_t joybus_rtc_detect_status = {0};

static void joybus_rtc_detect_callback( uint64_t *out_dwords, void *ctx )
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joybus_rtc_detect_callback_t callback = ctx;
    joybus_cmd_identify_port_t *cmd = (void *)&out_bytes[JOYBUS_RTC_PORT + JOYBUS_COMMAND_METADATA_SIZE];
    bool detected = cmd->recv.identifier == JOYBUS_IDENTIFIER_CART_RTC;
    joybus_rtc_status_t status = { .byte = cmd->recv.status };
    joybus_rtc_detect_state = detected ? JOYBUS_RTC_DETECTED : JOYBUS_RTC_NOT_DETECTED;
    joybus_rtc_detect_status.byte = status.byte;

    debugf("joybus_rtc_detect_async: %s %s %s %s\n",
        detected ? "detected" : "not detected",
        status.stopped ? "stopped" : "running",
        status.crystal_bad ? "crystal:BAD" : "crystal:OK",
        status.battery_bad ? "battery:BAD" : "battery:OK"
    );

    callback( detected, status );
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
            return !joybus_rtc_detect_status.crystal_bad;
        case JOYBUS_RTC_NOT_DETECTED:
            return false;
        case JOYBUS_RTC_DETECT_INIT:
            joybus_rtc_detect_async(NULL);
            break;
        case JOYBUS_RTC_DETECT_PENDING:
            break;
    }
    while( joybus_rtc_detect_state == JOYBUS_RTC_DETECT_PENDING ){ /* Spinlock! */}
    return joybus_rtc_detect_state == JOYBUS_RTC_DETECTED && !joybus_rtc_detect_status.crystal_bad;
}

// MARK: Control functions

static void joybus_rtc_set_stopped_write_callback( uint64_t *out_dwords, void *ctx )
{
    debugf("joybus_rtc_set_stopped_async: done\n");
    joybus_rtc_set_stopped_callback_t callback = ctx;
    if( callback ) callback();
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

void joybus_rtc_set_stopped_async( bool stop, joybus_rtc_set_stopped_callback_t callback )
{
    // Pack the stop bit and the callback pointer into the context value
    void *ctx = (void *)((uint32_t)callback | (stop & 0x1));
    debugf("joybus_rtc_set_stopped_async: reading control block\n");
    joybus_rtc_read_async( JOYBUS_RTC_BLOCK_CONTROL, joybus_rtc_set_stopped_read_callback, ctx );
}

void joybus_rtc_set_stopped( bool stop )
{
    joybus_rtc_control_t control;
    debugf("joybus_rtc_set_stopped: reading control block\n");
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    debugf("joybus_rtc_set_stopped: control state (0x%llx)\n", control.data.dword);

    control.stop = stop;
    control.lock_block1 = !stop;
    control.lock_block2 = !stop;
    debugf("joybus_rtc_set_stopped: writing control block (0x%llx)\n", control.data.dword);
    joybus_rtc_write( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
}

bool joybus_rtc_is_stopped( void )
{
    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_RTC_IDENTIFY,
    } };
    joybus_exec_cmd_struct(JOYBUS_RTC_PORT, cmd);
    joybus_rtc_status_t status = { .byte = cmd.recv.status };
    debugf("joybus_rtc_is_stopped: status (0x%02x)\n", status.byte);
    joybus_rtc_detect_status.byte = status.byte;
    return status.stopped;
}

// MARK: Time functions

static void joybus_rtc_get_time_callback( uint64_t *out_dwords, void *ctx )
{
    const uint8_t *out_bytes = (void *)out_dwords;
    joybus_rtc_get_time_callback_t callback = ctx;
    joybus_cmd_rtc_read_block_t *cmd = (void *)&out_bytes[JOYBUS_RTC_PORT + JOYBUS_COMMAND_METADATA_SIZE];
    joybus_rtc_data_t data = { .dword = cmd->recv.dword };
    debugf("joybus_rtc_get_time_async: raw time (0x%llx)\n", data.dword);
    time_t decoded_time = joybus_rtc_decode_time( &data );

    struct tm * parsed_tm = gmtime( &decoded_time );
    debugf("joybus_rtc_get_time_async: parsed time (%04d-%02d-%02d %02d:%02d:%02d)\n",
        parsed_tm->tm_year + 1900, parsed_tm->tm_mon + 1, parsed_tm->tm_mday,
        parsed_tm->tm_hour, parsed_tm->tm_min, parsed_tm->tm_sec
    );

    callback( RTC_ESUCCESS, decoded_time );
}

void joybus_rtc_get_time_async( joybus_rtc_get_time_callback_t callback )
{
    if( joybus_rtc_detect_state != JOYBUS_RTC_DETECTED )
    {
        debugf("joybus_rtc_get_time_async: RTC not detected; aborting!\n");
        callback( RTC_ENOCLOCK, 0 );
        return;
    }

    if( joybus_rtc_detect_status.crystal_bad )
    {
        debugf("joybus_rtc_get_time_async: crystal is bad; aborting!\n");
        callback( RTC_EBADCLOCK, 0 );
        return;
    }

    debugf("joybus_rtc_get_time_async: reading time block\n");
    joybus_rtc_read_async( JOYBUS_RTC_BLOCK_TIME, joybus_rtc_get_time_callback, callback );
}

int joybus_rtc_get_time( time_t *out )
{
    if( joybus_rtc_detect_state != JOYBUS_RTC_DETECTED )
    {
        debugf("joybus_rtc_get_time: RTC not detected; aborting!\n");
        return RTC_ENOCLOCK;
    }

    if( joybus_rtc_detect_status.crystal_bad )
    {
        debugf("joybus_rtc_get_time: crystal is bad; aborting!\n");
        return RTC_EBADCLOCK;
    }

    joybus_rtc_data_t data = {0};
    debugf("joybus_rtc_get_time: reading time block\n");
    joybus_rtc_read( JOYBUS_RTC_BLOCK_TIME, &data );
    debugf("joybus_rtc_get_time: raw time (0x%llx)\n", data.dword);
    *out = joybus_rtc_decode_time( &data );

    struct tm * parsed_tm = gmtime( out );
    debugf("joybus_rtc_get_time: parsed time (%04d-%02d-%02d %02d:%02d:%02d)\n",
        parsed_tm->tm_year + 1900, parsed_tm->tm_mon + 1, parsed_tm->tm_mday,
        parsed_tm->tm_hour, parsed_tm->tm_min, parsed_tm->tm_sec
    );

    return RTC_ESUCCESS;
}

int joybus_rtc_set_time( time_t new_time )
{
    if( new_time < JOYBUS_RTC_TIMESTAMP_MIN || new_time > JOYBUS_RTC_TIMESTAMP_MAX )
    {
        debugf("joybus_rtc_set_time: time out of range\n");
        return RTC_EBADTIME;
    }

    if( joybus_rtc_detect_status.crystal_bad )
    {
        debugf("joybus_rtc_set_time: crystal is bad; aborting!\n");
        return RTC_EBADCLOCK;
    }

    if( joybus_rtc_detect_status.battery_bad )
    {
        debugf("joybus_rtc_set_time: battery is bad; aborting!\n");
        return RTC_EBADCLOCK;
    }

    if (cart_type == CART_ED || cart_type == CART_EDX)
    {
        uint8_t buf[7];
        ed64_rtc_encode( new_time, buf );

        // Joybus RTC write is not supported on EverDrive 64.
        // Attempt to use the ED64 i2c interface to set the time instead.
        switch( cart_type )
        {
        case CART_ED:
            if( ed64_rtc_write( buf ) != 0 )
            {
                debugf("joybus_rtc_set_time: EverDrive 64 i2c write failed!\n");
                return RTC_EBADCLOCK;
            }
            return RTC_ESUCCESS;
        case CART_EDX:
            if( ed64x_rtc_write( buf ) != 0 )
            {
                debugf("joybus_rtc_set_time: EverDrive 64 X7 i2c write failed!\n");
                return RTC_EBADCLOCK;
            }
            return RTC_ESUCCESS;
        }
    }

    debugf("joybus_rtc_set_time: reading control block\n");
    joybus_rtc_control_t control;
    joybus_rtc_read( JOYBUS_RTC_BLOCK_CONTROL, &control.data );
    debugf("joybus_rtc_get_time: control state (0x%llx)\n", control.data.dword);

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
        return RTC_EBADCLOCK;
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
        return RTC_EBADCLOCK;
    }

    return RTC_ESUCCESS;
}

/** @} */ /* joybus_rtc */
