#include <string.h>
#include "libdragon.h"
#include "regsinternal.h"

#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
#define SI_STATUS_IO_BUSY   ( 1 << 1 )

#define ERROR_NONE          0x0
#define ERROR_BAD_COMMAND   0x1
#define ERROR_NOT_PRESENT   0x2

static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;
static void * const PIF_RAM = (void *)0x1fc007c0;

static struct controller_data current;
static struct controller_data last;

void controller_init()
{
    memset(&current, 0, sizeof(current));
    memset(&last, 0, sizeof(last));
}

static void __SI_DMA_wait(void) 
{
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) ;
}

static void __controller_exec_PIF( void *inblock, void *outblock ) 
{
    volatile uint64_t inblock_temp[8];
    volatile uint64_t outblock_temp[8];

    data_cache_writeback_invalidate(inblock_temp, 64);
    memcpy(UncachedAddr(inblock_temp), inblock, 64);

    __SI_DMA_wait();

    SI_regs->DRAM_addr = inblock_temp; // only cares about 23:0
    SI_regs->PIF_addr_write = PIF_RAM; // is it really ever anything else?

    __SI_DMA_wait();

    data_cache_writeback_invalidate(outblock_temp, 64);

    SI_regs->DRAM_addr = outblock_temp;
    SI_regs->PIF_addr_read = PIF_RAM;

    __SI_DMA_wait();

    memcpy(outblock, UncachedAddr(outblock_temp), 64);
}


void controller_read(struct controller_data * output) 
{
    static unsigned long long SI_read_con_block[8] = 
    {
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    __controller_exec_PIF(SI_read_con_block,output);
}

void controller_scan()
{
    /* Remember last */
    memcpy(&last, &current, sizeof(current));

    /* Grab current */
    memset(&current, 0, sizeof(current));
    controller_read(&current);
}

struct controller_data get_keys_down()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = (current.c[i].data) & ~(last.c[i].data);
    }

    return ret;
}

struct controller_data get_keys_up()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which was pressed last time and isn't now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = ~(current.c[i].data) & (last.c[i].data);
    }

    return ret;
}

struct controller_data get_keys_held()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which was pressed last time and now as well */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].data = (current.c[i].data) & (last.c[i].data);
    }

    return ret;
}

struct controller_data get_keys_pressed()
{
    return current;
}

int get_dpad_direction( int controller )
{
    /* Diagonals first because it could only be right angles otherwise */
    if( current.c[controller & 0x3].up && current.c[controller & 0x3].left ) 
    {
        return 3;
    }
    
    if( current.c[controller & 0x3].up && current.c[controller & 0x3].right ) 
    {
        return 1;
    }

    if( current.c[controller & 0x3].down && current.c[controller & 0x3].left ) 
    {
        return 5;
    }

    if( current.c[controller & 0x3].down && current.c[controller & 0x3].right ) 
    {
        return 7;
    }

    if( current.c[controller & 0x3].right )
    {
        return 0;
    }

    if( current.c[controller & 0x3].up )
    {
        return 2;
    }

    if( current.c[controller & 0x3].left )
    {
        return 4;
    }

    if( current.c[controller & 0x3].down )
    {
        return 6;
    }

    return -1;
}

int get_controllers_present()
{
    int ret = 0;
    struct controller_data output;
    static unsigned long long SI_read_controllers_block[8] =
    {
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xff010401ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    __controller_exec_PIF(SI_read_controllers_block,&output);

    if( output.c[0].err == ERROR_NONE ) { ret |= CONTROLLER_1_INSERTED; }
    if( output.c[1].err == ERROR_NONE ) { ret |= CONTROLLER_2_INSERTED; }
    if( output.c[2].err == ERROR_NONE ) { ret |= CONTROLLER_3_INSERTED; }
    if( output.c[3].err == ERROR_NONE ) { ret |= CONTROLLER_4_INSERTED; }
   
    return ret;
}

int get_accessories_present()
{
    int ret = 0;
    struct controller_data output;
    static unsigned long long SI_read_status_block[8] = 
    {
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    __controller_exec_PIF(SI_read_status_block,&output);

    /* The only significant value is the third byte */
    if( (output.c[0].err == ERROR_NONE) && (((output.c[0].data >> 8) & 0xFF) == 0x01) ) { ret |= CONTROLLER_1_INSERTED; }
    if( (output.c[1].err == ERROR_NONE) && (((output.c[1].data >> 8) & 0xFF) == 0x01) ) { ret |= CONTROLLER_2_INSERTED; }
    if( (output.c[2].err == ERROR_NONE) && (((output.c[2].data >> 8) & 0xFF) == 0x01) ) { ret |= CONTROLLER_3_INSERTED; }
    if( (output.c[3].err == ERROR_NONE) && (((output.c[3].data >> 8) & 0xFF) == 0x01) ) { ret |= CONTROLLER_4_INSERTED; }

    return ret;
}

static uint16_t __calc_address_crc( uint16_t address )
{
    /* CRC table */
    uint16_t xor_table[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x15, 0x1F, 0x0B, 0x16, 0x19, 0x07, 0x0E, 0x1C, 0x0D, 0x1A, 0x01 };
    uint16_t crc = 0;

    /* Make sure we have a valid address */
    address &= ~0x1F;

    /* Go through each bit in the address, and if set, xor the right value into the output */
    for( int i = 15; i >= 5; i-- )
    {
        /* Is this bit set? */
        if( ((address >> i) & 0x1) )
        {
           crc ^= xor_table[i]; 
        }
    }

    /* Just in case */
    crc &= 0x1F;

    /* Create a new address with the CRC appended */
    return address | crc;
}

static uint8_t __calc_data_crc( uint8_t *data )
{
    uint8_t ret = 0;

    for( int i = 0; i <= 32; i++ )
    {
        for( int j = 7; j >= 0; j-- )
        {
            int tmp = 0;

            if( ret & 0x80 )
            {
                tmp = 0x85;
            }

            ret <<= 1;

            if( i < 32 )
            {
                if( data[i] & (0x01 << j) )
                {
                    ret |= 0x1;
                }
            }

            ret ^= tmp;
        }
    }

    return ret;
}

int read_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    uint8_t output[64];
    uint8_t SI_read_mempak_block[64];
    int ret;

    /* Controller must be in range */
    if( controller < 0 || controller > 3 ) { return -1; }

    /* Last byte must be 0x01 to signal to the SI to process data */
    memset( SI_read_mempak_block, 0, 64 );
    SI_read_mempak_block[56] = 0xfe;
    SI_read_mempak_block[63] = 0x01;

    /* Start command at the correct channel to read from the right mempak */
    SI_read_mempak_block[controller]     = 0x03;
    SI_read_mempak_block[controller + 1] = 0x21;
    SI_read_mempak_block[controller + 2] = 0x02;

    /* Calculate CRC on address */
    uint16_t read_address = __calc_address_crc( address );
    SI_read_mempak_block[controller + 3] = (read_address >> 8) & 0xFF;
    SI_read_mempak_block[controller + 4] = read_address & 0xFF;

    /* Leave room for 33 bytes (32 bytes + CRC) to come back */
    memset( &SI_read_mempak_block[controller + 5], 0xFF, 33 ); 

    __controller_exec_PIF(SI_read_mempak_block,&output);

    /* Copy data correctly out of command */
    memcpy( data, &output[controller + 5], 32 );

    /* Validate CRC */
    uint8_t crc = __calc_data_crc( &output[controller + 5] );

    if( crc == output[controller + 5 + 32] )
    {
        /* Data was read successfully */
        ret = 0;
    }
    else
    {
        if( crc == (output[controller + 5 + 32] ^ 0xFF) )
        {
            /* Pak not present! */
            ret = -2;
        }
        else
        {
            /* Pak returned bad data */
            ret = -3;
        }
    }

    return ret;
}

int write_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    uint8_t output[64];
    uint8_t SI_write_mempak_block[64];
    int ret;

    /* Controller must be in range */
    if( controller < 0 || controller > 3 ) { return -1; }

    /* Last byte must be 0x01 to signal to the SI to process data */
    memset( SI_write_mempak_block, 0, 64 );
    SI_write_mempak_block[56] = 0xfe;
    SI_write_mempak_block[63] = 0x01;

    /* Start command at the correct channel to write from the right mempak */
    SI_write_mempak_block[controller]     = 0x23;
    SI_write_mempak_block[controller + 1] = 0x01;
    SI_write_mempak_block[controller + 2] = 0x03;

    /* Calculate CRC on address */
    uint16_t write_address = __calc_address_crc( address );
    SI_write_mempak_block[controller + 3] = (write_address >> 8) & 0xFF;
    SI_write_mempak_block[controller + 4] = write_address & 0xFF;

    /* Place the data to be written */
    memcpy( &SI_write_mempak_block[controller + 5], data, 32 ); 

    /* Leave room for CRC to come back */
    SI_write_mempak_block[controller + 5 + 32] = 0xFF;

    __controller_exec_PIF(SI_write_mempak_block,&output);

    /* Calculate CRC on output */
    uint8_t crc = __calc_data_crc( &output[controller + 5] );

    if( crc == output[controller + 5 + 32] )
    {
        /* Data was written successfully */
        ret = 0;
    }
    else
    {
        if( crc == (output[controller + 5 + 32] ^ 0xFF) )
        {
            /* Pak not present! */
            ret = -2;
        }
        else
        {
            /* Pak returned bad data */
            ret = -3;
        }
    }

    return ret;
}

int identify_accessory( int controller )
{
    uint8_t data[32];

    /* Init string one */
    memset( data, 0xfe, 32 );
    write_mempak_address( controller, 0x8000, data );
    
    /* Init string two */
    memset( data, 0x80, 32 );
    write_mempak_address( controller, 0x8000, data );

    /* Get register contents */
    if( read_mempak_address( controller, 0x8000, data ) == 0 )
    {
        /* Should really check all bytes, but this should suffice */
        if( data[0] == 0x80 )
        {
            return ACCESSORY_RUMBLEPAK;
        }
        else
        {
            return ACCESSORY_MEMPAK;
        }
    }
    else
    {
        return ACCESSORY_NONE;
    }
}

void rumble_start( int controller )
{
    uint8_t data[32];

    /* Unsure of why we have to do this multiple times */
    memset( data, 0x01, 32 );    
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
}

void rumble_stop( int controller )
{
    uint8_t data[32];

    /* Unsure of why we have to do this multiple times */
    memset( data, 0x00, 32 );
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
    write_mempak_address( controller, 0xC000, data );
}
