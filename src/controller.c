#include <string.h>
#include "libdragon.h"
#include "regsinternal.h"

#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
#define SI_STATUS_IO_BUSY   ( 1 << 1 )

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

int read_mempak_sector( int controller, int sector, uint8_t *sector_data )
{
    if( sector < 0 || sector >= 128 ) { return -1; }
    if( sector_data == 0 ) { return -1; }

    /* Sectors are 256 bytes, a mempak reads 32 bytes at a time */
    for( int i = 0; i < 8; i++ )
    {
        if( read_mempak_address( controller, (sector * 256) + (i * 32), sector_data + (i * 32) ) )
        {
            /* Failed to read a block */
            return -2;
        }
    }

    return 0;
}

int write_mempak_sector( int controller, int sector, uint8_t *sector_data )
{
    if( sector < 0 || sector >= 128 ) { return -1; }
    if( sector_data == 0 ) { return -1; }

    /* Sectors are 256 bytes, a mempak reads 32 bytes at a time */
    for( int i = 0; i < 8; i++ )
    {
        if( write_mempak_address( controller, (sector * 256) + (i * 32), sector_data + (i * 32) ) )
        {
            /* Failed to read a block */
            return -2;
        }
    }

    return 0;
}

static int __validate_header( uint8_t *sector )
{
    if( !sector ) { return -1; }

    /* Header is first sector */
    if( memcmp( &sector[0x20], &sector[0x60], 16 ) != 0 ) { return -1; }
    if( memcmp( &sector[0x80], &sector[0xC0], 16 ) != 0 ) { return -1; }
    if( memcmp( &sector[0x20], &sector[0x80], 16 ) != 0 ) { return -1; }

    return 0;
}

static int __validate_toc( uint8_t *sector )
{
    uint32_t sum = 0;

    /* Rudimentary checksum */
    for( int i = 5; i < 128; i++ )
    {
        sum += sector[(i << 1) + 1];
    }

    /* True checksum is sum % 256 */
    if( (sum & 0xFF) == sector[1] )
    {
        return 0;
    }
    else
    {
        return -1;
    }

    return -1;
}

static char __n64_to_ascii( char c )
{
    /* Miscelaneous chart */
    switch( c )
    {
        case 0x00:
            return 0;
        case 0x0F:
            return ' ';
        case 0x34:
            return '!';
        case 0x35:
            return '\"';
        case 0x36:
            return '#';
        case 0x37:
            return '`';
        case 0x38:
            return '*';
        case 0x39:
            return '+';
        case 0x3A:
            return ',';
        case 0x3B:
            return '-';
        case 0x3C:
            return '.';
        case 0x3D:
            return '/';
        case 0x3E:
            return ':';
        case 0x3F:
            return '=';
        case 0x40:
            return '?';
        case 0x41:
            return '@';
    }

    /* Numbers */
    if( c >= 0x10 && c <= 0x19 )
    {
        return '0' + (c - 0x10);
    }

    /* Uppercase ASCII */
    if( c >= 0x1A && c <= 0x33 )
    {
        return 'A' + (c - 0x1A);
    }

    /* Default to space for unprintables */
    return ' ';
}

static int __read_note( uint8_t *sector, int index, entry_structure_t *note )
{
    if( !sector || !note ) { return -1; }
    if( index < 0 || index > 16 ) { return -1; }

    uint8_t *tnote = &sector[index << 5];

    /* Easy stuff */
    note->vendor = (tnote[0] << 16) | (tnote[1] << 8) | (tnote[2]);
    note->region = tnote[3];

    /* Important stuff */
    note->game_id = (tnote[4] << 8) | (tnote[5]);
    note->inode = (tnote[6] << 8) | (tnote[7]);

    /* Don't know number of blocks */
    note->blocks = 0;
    note->valid = 0;

    /* Translate n64 to ascii */
    memset( note->name, 0, sizeof( note->name ) );

    for( int i = 0; i < 16; i++ )
    {
        note->name[i] = __n64_to_ascii( tnote[0x10 + i] );
    }

    /* Find the last position */
    for( int i = 0; i < 17; i++ )
    {
        if( note->name[i] == 0 )
        {
            /* Here it is! */
            note->name[i]   = '.';
            note->name[i+1] = __n64_to_ascii( tnote[0xC] );
            break;
        }
    }

    /* Validate entries */
    if( note->inode < 5 || note->inode >= 128 )
    {
        /* Invalid inode */
        return -2;
    }

    switch( note->region )
    {
        case 0x00:
        case 0x37:
        case 0x41:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x49:
        case 0x4A:
        case 0x50:
        case 0x53:
        case 0x55:
        case 0x58:
        case 0x59:
            break;
        default:
            /* Invalid region */
            return -3;

    }

    /* Checks out */
    note->valid = 1;
    return 0;
}

static int __get_num_pages( uint8_t *sector, int inode )
{
    if( inode < 5 || inode >= 128 ) { return -1; }

    int tally = 0;
    int last = inode;
    int rcount = 0;

    /* If we go over this, something is wrong */
    while( rcount < 123 )
    {
        switch( sector[(last << 1) + 1] )
        {
            case 0x01:
                /* Last block */
                return tally + 1;
            case 0x03:
                /* Error, can't have free blocks! */
                return -2;
            default:
                last = sector[(last << 1) + 1];
                tally++;

                /* Failed to point to valid next block */
                if( last < 5 || last >= 128 ) { return -3; }
                break;
        }

        rcount++;
    }

    /* Invalid filesystem */
    return -3;
}

static int __get_free_space( uint8_t *sector )
{
    int space = 0;

    for( int i = 5; i < 128; i++ )
    {
        if( sector[(i << 1) + 1] == 0x03 )
        {
            space++;
        }
    }

    return space;
}

static int __get_note_block( uint8_t *sector, int inode, int block )
{
    if( inode < 5 || inode >= 128 ) { return -1; }
    if( block < 0 || block > 123 ) { return -1; }

    int tally = block + 1;
    int last = inode;

    /* Only going through until we hit the requested node */
    while( tally > 0 )
    {
        /* Count down to zero, when zero is hit, this is the node we want */
        tally--;

        if( !tally )
        {
            return last;
        }
        else
        {
            switch( sector[(last << 1) + 1] )
            {
                case 0x01:
                    /* Last block, couldn't find block number */
                    return -2;
                case 0x03:
                    /* Error, can't have free blocks! */
                    return -2;
                default:
                    last = sector[(last << 1) + 1];

                    /* Failed to point to valid next block */
                    if( last < 5 || last >= 128 ) { return -3; }
                    break;
            }
        }
    }

    /* Invalid filesystem */
    return -3;
}

int __get_valid_toc( int controller )
{
    /* We will need only one sector at a time */
    uint8_t data[256];

    /* First check to see that the header block is valid */
    if( read_mempak_sector( controller, 0, data ) )
    {
        /* Couldn't read header */
        return -2;
    }

    if( __validate_header( data ) )
    {
        /* Header is invalid or unformatted */
        return -3;
    }

    /* Try to read the first TOC */
    if( read_mempak_sector( controller, 1, data ) )
    {
        /* Couldn't read header */
        return -2;
    }

    if( __validate_toc( data ) )
    {
        /* First TOC is bad.  Maybe the second works? */
        if( read_mempak_sector( controller, 2, data ) )
        {
            /* Couldn't read header */
            return -2;
        }

        if( __validate_toc( data ) )
        {
            /* Second TOC is bad, nothing good on this memcard */
            return -3;
        }
        else
        {
            /* Found a good TOC! */
            return 2;
        }
    }
    else
    {
        /* Found a good TOC! */
        return 1;
    }
}

int validate_mempak( int controller )
{
    int toc = __get_valid_toc( controller );

    if( toc == 1 || toc == 2 )
    {
        /* Found a valid TOC */
        return 0;
    }
    else
    {
        /* Pass on return code */
        return toc;
    }
}

int get_mempak_entry( int controller, int entry, entry_structure_t *entry_data )
{
    uint8_t data[256];
    int toc;

    if( entry < 0 || entry > 15 ) { return -1; }
    if( entry_data == 0 ) { return -1; }

    /* Make sure mempak is valid */
    if( (toc = __get_valid_toc( controller )) <= 0 )
    {
        /* Bad mempak or was removed, return */
        return -2;
    }

    /* Entries are spread across two sectors */
    if( read_mempak_sector( controller, (entry >= 8) ? 4 : 3, data ) )
    {
        /* Couldn't read note database */
        return -2;
    }

    /* Only eight notes per sector */
    if( __read_note( data, entry & 0x7, entry_data ) )
    {
        /* Note is most likely empty, don't bother getting length */
        return 0;
    }

    /* Grab the TOC sector */
    if( read_mempak_sector( controller, toc, data ) )
    {
        /* Couldn't read TOC */
        return -2;
    }

    /* Get the length of the entry */
    int blocks = __get_num_pages( data, entry_data->inode );

    if( blocks > 0 )
    {
        /* Valid entry */
        entry_data->blocks = blocks;
        return 0;
    }
    else
    {
        /* Invalid TOC */
        entry_data->valid = 0;
        return 0;
    }
}

int get_mempak_free_space( int controller )
{
    uint8_t data[256];
    int toc;

    /* Make sure mempak is valid */
    if( (toc = __get_valid_toc( controller )) <= 0 )
    {
        /* Bad mempak or was removed, return */
        return -2;
    }

    /* Grab the valid TOC to get free space */
    if( read_mempak_sector( controller, toc, data ) )
    {
        /* Couldn't read TOC */
        return -2;
    }
    
    return __get_free_space( data );
}

int test()
{
    __get_note_block( 0, 0, 0 );

    return 0;
}
