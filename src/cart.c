/**
 * @file cart.c
 * @brief Cartridge
 * @ingroup cart
 */

#include <string.h>
#include "libdragon.h"

/**
 * @defgroup cart Cartridge interface
 * @ingroup libdragon
 * @brief Routines for interacting with the cartridge and hardware attached to it.
 *
 * The cartridge contains the ROM (up to 64 megabytes), as well as optional writable
 * memory in the form of SRAM or FlashRAM. The cartridge may also contain other hardware
 * such as an EEPROM or Real-Time Clock, which are handled by the Joybus interface.
 *
 * In general, the best way to access ROM or RAM is through DMA transfers. The functions
 * here are mostly convenience helpers on top of the Peripheral Interface, which manages
 * DMA transfers.
 * 
 * If your ROM does not need to store more than 16 kilobits (2 kilobytes), you could use
 * the EEPROM save type. In the age of emulators and flash carts, EEPROM offers no real
 * advantage over SRAM. EEPROM is lower-capacity, slower to write, must be accessed in
 * 8-byte blocks, and (on real hardware) should use parity bits or checksums to ensure
 * data consistency. The strongest reason why you might consider using EEPROM is if you
 * wanted to make your own reproduction cartridge, since the boards often support EEPROM
 * in 4Kbit and 16Kbit capacities without needing to scavenge for "donor chips".
 *
 * SRAM is the simplest save type, allowing direct access using DMA reads/writes. If your
 * storage needs are greater than 16 kilobits (2 kilobytes) and less than 256 kilobits
 * (32 kilobytes), you should probably use SRAM. If you still need more capacity, there
 * are a few options, but each comes with its own trade-offs.
 * 
 * Controller Paks can also be used for extended storage; A single Controller Pak can
 * store up to 256 kilobits (32 kilobytes); up to four paks can be connected at a time,
 * and even more capacity is possible if the paks are swapped out between reads/writes.
 * 
 * For ROMs that need to store up to 1 megabit (128 kilobytes), your best choice for
 * save type is FlashRAM. Unfortunately, it is significantly more complicated to write
 * data to FlashRAM. At this time, libdragon does not offer convenience functions to
 * abstract the complexities of the various FlashRAM chips that could be on the cartridge.
 * 
 * The 768 kilobit SRAM configuration was only ever used by one cartridge (Dezaemon 3D),
 * and is implemented as a logic chip that selects between three 256 kilobit SRAM banks.
 * Many flash carts do have support for Dezaemon 3D, but 64drive does not display the
 * 768K save type in the menu and some EverDrive64 OS versions label it as "96K SRAM".
 * Emulator support for the 768 kilobit SRAM configuration is not widespread, and there
 * are no reproduction cartridge boards that support it. libdragon does not recommend
 * using the 768 kilobit SRAM save type if you want your ROM to be widely-compatible:
 * Prefer the 1 megabit FlashRAM save type instead.
 * 
 * EverDrive64 offers a 1 megabit SRAM save type, which many emulators and flash carts
 * have chosen not to support because it is not an authentic save type that was ever
 * used by any retail cartridge. libdragon does not recommend using the 1 megabit SRAM
 * save type if you want your ROM to be widely-compatible: Prefer FlashRAM instead.
 * 
 * Some flash carts and emulators may offer up to 1 megabit of SRAM storage using a
 * contiguous address space up to 0x1FFFF. libdragon does not support or recommend
 * taking advantage of this implementation detail: it is an emulation accuracy bug.
 * If your ROM relies on inaccurate behavior, it may not behave as expected in many
 * emulators or on real hardware. Please do not fragment the ecosystem or cause
 * unnecessary headaches for emulator maintainers and your end-users.
 *
 * @{
 */

#define CART_DOM1_ADDR2_START     0x10000000
#define CART_DOM1_ADDR2_END       0x1FBFFFFF
#define CART_DOM1_ADDR2_MASK      0x0FFFFFFF
#define CART_DOM1_ADDR2_SIZE      0x0FC00000

#define CART_DOM2_ADDR2_START     0x08000000
#define CART_DOM2_ADDR2_END       0x0FFFFFFF
#define CART_DOM2_ADDR2_MASK      0x07FFFFFF
#define CART_DOM2_ADDR2_SIZE      0x08000000

#define CART_ROM_MASK             0x03FFFFFF
#define CART_ROM_SIZE             0x04000000
#define FLASHRAM_MASK             0x0001FFFF
#define FLASHRAM_SIZE             0x00020000
#define SRAM_256KBIT_MASK         0x00007FFF
#define SRAM_256KBIT_SIZE         0x00008000
#define SRAM_256KBIT_BANKS        1
#define SRAM_768KBIT_BANKS        3
#define SRAM_1MBIT_BANKS          4

#define FLASHRAM_IDENTIFIER                0x11118001
#define FLASHRAM_OFFSET_COMMAND            0x00010000
#define FLASHRAM_OFFSET_MASK               0x0000FFFF
#define FLASHRAM_COMMAND_SET_ERASE_OFFSET  0x4B000000
#define FLASHRAM_COMMAND_SET_ERASE_MODE    0x78000000
#define FLASHRAM_COMMAND_SET_WRITE_OFFSET  0xA5000000
#define FLASHRAM_COMMAND_SET_WRITE_MODE    0xB4000000
#define FLASHRAM_COMMAND_COMMIT            0xD2000000
#define FLASHRAM_COMMAND_SET_IDENTIFY_MODE 0xE1000000
#define FLASHRAM_COMMAND_SET_READ_MODE     0xF0000000

static uint8_t cart_detect_sram( void );
static bool cart_sram_verify( uint8_t bank );

/**
 * @brief Clamp length from a start address so that it does not go past an end address.
 *
 * Used by DMA functions to ensure the read/writes stay in defined ranges.
 *
 * @param[in] len
 *            Length to clamp (in bytes).
 * 
 * @param[in] start
 *            Starting address to add length to.
 * 
 * @param[in] end
 *            Ending address that start+len cannot exceed.
 *
 * @return Clamped length.
 */
static uint32_t clamp(uint32_t len, uint32_t start, uint32_t end)
{
    int32_t overage = (int32_t)(start + len) - (int32_t)(end + 1);
    return ( overage > 0 ) ? len - overage : len;
}

/**
 * @brief Determine which save type is available on the cartridge.
 *
 * This function checks for EEPROM, then FlashRAM, then SRAM.
 *
 * It is not possible for SRAM and FlashRAM to co-exist.
 * 
 * There were no N64 retail releases that included more than one save type,
 * but it is possible for EEPROM and either SRAM or FlashRAM to be installed
 * simultaneously.
 * 
 * Your code should check the bitfield for the desired save type instead of
 * testing for equality with a single save type (except for SAVE_TYPE_NONE).
 *
 * @return a bitfield of save types detected on the cartridge.
 */
uint8_t cart_detect_save_type( void )
{
    uint8_t detected = SAVE_TYPE_NONE;

    eeprom_type_t eeprom = eeprom_present();
    if( eeprom == EEPROM_4K ) detected |= SAVE_TYPE_EEPROM_4KBIT;
    if( eeprom == EEPROM_16K ) detected |= SAVE_TYPE_EEPROM_16KBIT;

    if( cart_detect_flashram() ) detected |= SAVE_TYPE_FLASHRAM_1MBIT;
    else detected |= cart_detect_sram();

    return detected;
}

/**
 * @brief Determine which FlashRAM chip is installed on the cartridge.
 *
 * The various FlashRAM chips all have slightly different behaviors, so it is helpful
 * to know which one is installed.
 *
 * @return the detected FlashRAM type on the cartridge.
 */
flashram_type_t cart_detect_flashram( void )
{
    /* Tell the FlashRAM to identify itself */
    io_write(CART_DOM2_ADDR2_START | FLASHRAM_OFFSET_COMMAND, FLASHRAM_COMMAND_SET_IDENTIFY_MODE) ;

    /* Read the identifiers */
    uint32_t __attribute__((aligned(16))) silicon_id[2];
    data_cache_hit_writeback_invalidate(silicon_id, sizeof(silicon_id));
    cart_dom2_addr2_read(silicon_id, 0, sizeof(silicon_id));

    /* Check for the magic "this is FlashRAM" value, followed by which chip it is. */
    if( silicon_id[0] == FLASHRAM_IDENTIFIER ) return silicon_id[1];
    return FLASHRAM_TYPE_NONE;
}

/**
 * @brief Read from Cartridge Domain 1 Address 2
 *
 * This is a low-level function this is used by #cart_rom_read.
 *
 * @param[out] dest
 *             Pointer to a buffer to place read data
 * 
 * @param[in]  offset
 *             Offset in bytes from the start of Cartridge Domain 1 Address 2 to read from
 * 
 * @param[in]  len
 *             Length in bytes to read into dest
 */
void cart_dom1_addr2_read(void * dest, uint32_t offset, uint32_t len)
{
    assert(dest != NULL);
    assert(offset < CART_DOM1_ADDR2_SIZE);
    assert(len > 1);

    uint32_t cart_address = ((offset & CART_DOM1_ADDR2_MASK) | CART_DOM1_ADDR2_START);
    len = clamp( len, cart_address, CART_DOM1_ADDR2_END );
    pi_dma_read( dest, cart_address, len );
}

/**
 * @brief Write to Cartridge Domain 1 Address 2
 *
 * This is a low-level function this is used by #cart_rom_write.
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 * 
 * @param[in] offset
 *            Offset in bytes from the start of Cartridge Domain 1 Address 2 to write to
 * 
 * @param[in] len
 *            Length in bytes to write from src
 */
void cart_dom1_addr2_write(const void * src, uint32_t offset, uint32_t len)
{
    assert(src != NULL);
    assert(offset < CART_DOM1_ADDR2_SIZE);
    assert(len > 1);

    uint32_t cart_address = ((offset & CART_DOM1_ADDR2_MASK) | CART_DOM1_ADDR2_START);
    len = clamp( len, cart_address, CART_DOM1_ADDR2_END );
    pi_dma_write( src, cart_address, len );
}

/**
 * @brief Read from Cartridge Domain 2 Address 2
 *
 * This function should be used when reading from SRAM or FlashRAM.
 *
 * @param[out] dest
 *             Pointer to a buffer to place read data
 * 
 * @param[in]  offset
 *             Offset in bytes from the start of Cartridge Domain 1 Address 2 to read from
 * 
 * @param[in]  len
 *             Length in bytes to read into dest
 */
void cart_dom2_addr2_read(void * dest, uint32_t offset, uint32_t len)
{
    assert(dest != NULL);
    assert(offset < CART_DOM2_ADDR2_SIZE);
    assert(len > 1);

    uint32_t cart_address = ((offset & CART_DOM2_ADDR2_MASK) | CART_DOM2_ADDR2_START);
    len = clamp( len, cart_address, CART_DOM2_ADDR2_END );
    pi_dma_read( dest, cart_address, len );
}

/**
 * @brief Write to Cartridge Domain 2 Address 2
 *
 * This function should be used when writing to the SRAM or FlashRAM.
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 * 
 * @param[in] offset
 *            Offset in bytes from the start of Cartridge Domain 2 Address 2 to write to
 * 
 * @param[in] len
 *            Length in bytes to write from src
 */
void cart_dom2_addr2_write(const void * src, uint32_t offset, uint32_t len)
{
    assert(src != NULL);
    assert(offset < CART_DOM2_ADDR2_SIZE);
    assert(len > 1);

    uint32_t cart_address = ((offset & CART_DOM2_ADDR2_MASK) | CART_DOM2_ADDR2_START);
    len = clamp( len, cart_address, CART_DOM2_ADDR2_END );
    pi_dma_write( src, cart_address, len );
}

/**
 * @brief Read from Cartridge ROM.
 *
 * @param[out] dest
 *             Pointer to a buffer to place read data
 * 
 * @param[in]  offset
 *             Offset in bytes from the start of Cartridge ROM to read from
 * 
 * @param[in]  len
 *             Length in bytes to read into dest
 */
void cart_rom_read(void * dest, uint32_t offset, uint32_t len)
{
    assert(dest != NULL);
    assert(offset < CART_ROM_SIZE);
    assert(len > 1);
    assert(offset + len <= CART_ROM_SIZE);

    offset &= CART_ROM_MASK;
    len = clamp( len, offset, CART_ROM_SIZE - 1 );
    cart_dom1_addr2_read( dest, offset, len );
}

/**
 * @brief Write to Cartridge ROM.
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 * 
 * @param[in] offset
 *            Offset in bytes from the start of Cartridge ROM to write to
 * 
 * @param[in] len
 *            Length in bytes to write from src
 */
void cart_rom_write(const void * src, uint32_t offset, uint32_t len)
{
    assert(src != NULL);
    assert(offset < CART_ROM_SIZE);
    assert(len > 1);
    assert(offset + len <= CART_ROM_SIZE);

    offset &= CART_ROM_MASK;
    len = clamp( len, offset, CART_ROM_SIZE - 1 );
    cart_dom1_addr2_write( src, offset, len );
}

/**
 * @brief Read from an SRAM bank.
 *
 * @param[in] dest
 *            Pointer to a buffer to place read data
 * 
 * @param[in] bank
 *            Which 256Kbit SRAM bank to select (0-3)
 * 
 * @param[in] offset
 *            Offset in bytes from the start of the 256Kbit SRAM bank to write to
 * 
 * @param[in] len
 *            Length in bytes to read into dest
 */
void cart_sram_read(void * dest, uint8_t bank, uint32_t offset, uint32_t len)
{
    assert(dest != NULL);
    assert(bank < SRAM_1MBIT_BANKS);
    assert(offset < SRAM_256KBIT_SIZE);
    assert(len > 1);
    assert(offset + len <= SRAM_256KBIT_SIZE);

    offset &= SRAM_256KBIT_MASK;
    len = clamp( len, offset, SRAM_256KBIT_SIZE - 1 );
    uint32_t sram_address = ((uint32_t)bank << 18) | offset;
    cart_dom2_addr2_read( dest, sram_address, len );
}

/**
 * @brief Write to an SRAM bank.
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 * 
 * @param[in] bank
 *            Which 256Kbit SRAM bank to select (0-3)
 * 
 * @param[in] offset
 *            Offset in bytes from the start of the 256Kbit SRAM bank to write to
 * 
 * @param[in] len
 *            Length in bytes to write from src
 */
void cart_sram_write(const void * src, uint8_t bank, uint32_t offset, uint32_t len)
{
    assert(src != NULL);
    assert(bank < SRAM_1MBIT_BANKS);
    assert(offset < SRAM_256KBIT_SIZE);
    assert(len > 1);
    assert(offset + len <= SRAM_256KBIT_SIZE);

    offset &= SRAM_256KBIT_MASK;
    len = clamp( len, offset, SRAM_256KBIT_SIZE - 1 );
    uint32_t sram_address = ((uint32_t)bank << 18) | offset;
    cart_dom2_addr2_write( src, sram_address, len );
}

/**
 * @brief Probe the characteristics of the SRAM banks on the cartridge.
 *
 * 768Kbit SRAM is implemented as three separate 256Kbit SRAM chips with a logic circuit
 * to determine which chip to access. 1Mbit SRAM can be implemented as a fourth SRAM chip
 * in the same bank-selection arrangement.
 * 
 * Unfortunately, the only way to check this is to actually perform DMA writes/reads,
 * which is a destructive operation. This routine attempts to preserve the data before
 * clobbering it during the tests, and will restore the original data before returning.
 * 
 * @return a bitfield of SRAM save type configurations that were detected.
 */
uint8_t cart_detect_sram( void )
{
    uint8_t __attribute__((aligned(16))) backup[SRAM_256KBIT_SIZE][SRAM_1MBIT_BANKS];
    uint8_t detected = SAVE_TYPE_NONE;
    uint8_t restore_banks = 0;

    /* Back up the SRAM data in the 1Mbit of bank-selected address spaces */
    for( int i = 0; i < SRAM_1MBIT_BANKS; ++i )
    {
        data_cache_hit_writeback_invalidate( &backup[i], SRAM_256KBIT_SIZE );
        cart_sram_read( &backup[i], i, 0, SRAM_256KBIT_SIZE );
    }
  
    /* Check for the standard 256Kbit SRAM capacity */
    if( cart_sram_verify( 0 ) )
    {
        detected |= SAVE_TYPE_SRAM_256KBIT;
        restore_banks = SRAM_256KBIT_BANKS;
        /* Check the bank-selected address spaces */
        if( cart_sram_verify( 1 ) &&
            cart_sram_verify( 2 ) )
        {
            detected |= SAVE_TYPE_SRAM_768KBIT_BANKED;
            restore_banks = SRAM_768KBIT_BANKS;
            if( cart_sram_verify( 3 ) )
            {
                detected |= SAVE_TYPE_SRAM_1MBIT_BANKED;
                restore_banks = SRAM_1MBIT_BANKS;
            }
        }
    }

    /* Restore SRAM data to bank-selected address spaces */
    if( restore_banks )
    {
        for( int i = 0; i < restore_banks; ++i )
        {
            data_cache_hit_writeback_invalidate( &backup[i], SRAM_256KBIT_SIZE );
            cart_sram_write( &backup[i], i, 0, SRAM_256KBIT_SIZE );
        }
    }
    
    return detected;
}

/**
 * @brief Verify that an SRAM bank is actually writable.
 * 
 * Some flash carts and emulators will wrap or mask SRAM addresses,
 * so this routine has to check that the SRAM reads and writes data
 * to the desired bank and does not also write to other SRAM banks.
 * 
 * This is a destructive operation across all SRAM banks, and this
 * routine makes no effort to preserve any existing data! All data
 * backup and restoration is handled by #cart_detect_sram.
 * 
 * @param[in]  bank
 *             Which 256Kbit SRAM bank to select (0-3)
 * 
 * @return whether the SRAM bank exists and supports DMA writes. 
 */
bool cart_sram_verify( uint8_t bank )
{
    assert(bank < SRAM_1MBIT_BANKS);

    uint8_t __attribute__((aligned(16))) write_buf[SRAM_256KBIT_SIZE];
    uint8_t __attribute__((aligned(16))) read_buf[SRAM_256KBIT_SIZE];

    /* Clear all previous SRAM banks to detect address wrapping */
    memset( write_buf, 0, SRAM_256KBIT_SIZE );
    for( int i = 0; i < bank; i++ )
    {
        /* No previous banks to erase if verifying the first one */
        if( bank == 0 ) break;
        /* Write all zeroes to the bank to read back later */
        data_cache_hit_writeback_invalidate( write_buf, SRAM_256KBIT_SIZE );
        cart_sram_write( write_buf, i, 0, SRAM_256KBIT_SIZE );
    }

    /* Generate test values based on the destination SRAM addresses */
    uint32_t * write_words = (uint32_t *)write_buf;
    for( int i = 0; i < SRAM_256KBIT_SIZE / sizeof(uint32_t); ++i )
    {
        write_words[i] = ((uint32_t)bank << 18) + i;
    }

    /* Write the test values into SRAM */
    data_cache_hit_writeback_invalidate( write_buf, SRAM_256KBIT_SIZE );
    cart_sram_write( write_buf, bank, 0, SRAM_256KBIT_SIZE );

    /* Read the test values back to see if they persisted */
    data_cache_hit_writeback_invalidate( read_buf, SRAM_256KBIT_SIZE );
    cart_sram_read( read_buf, bank, 0, SRAM_256KBIT_SIZE );

    /* Compare what was written to what was read back from SRAM */
    if( memcmp( write_buf, read_buf, SRAM_256KBIT_SIZE ) != 0 )
    {
        /* There was a mismatch between what was written and read */
        return false;
    }

    /* Check that no previous banks were modified by changing this one */
    memset( write_buf, 0, SRAM_256KBIT_SIZE );
    for( int i = 0; i < bank; i++ )
    {
        /* No previous banks to check if verifying the first one */
        if( bank == 0 ) break;
        /* Read back the bank to see if it's still all zeroes */
        data_cache_hit_writeback_invalidate( read_buf, SRAM_256KBIT_SIZE );
        cart_sram_read( read_buf, i, 0, SRAM_256KBIT_SIZE );
        if( memcmp( write_buf, read_buf, SRAM_256KBIT_SIZE ) != 0 )
        {
            /* The write test appears to have wrapped into another bank */
            return false;
        }
    }

    return true;
}

/** @} */ /* cart */
