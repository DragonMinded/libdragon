
#include <stdbool.h>

/*********************************************************************
 * FAT backend: Everdrive64
 *********************************************************************/
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

// Everdrive registers
#define ED64_BASE_ADDRESS             0xBF808000
#define ED64_SD_IO_BUFFER                 0x0200
#define ED64_REGISTER_SD_BASE             0x0020
#define ED64_REGISTER_SD_STATUS           0x0030

// Everdrive config bits
#define ED64_SD_CFG_BITLEN                0x000F
#define ED64_SD_CFG_SPEED                 0x0010

// Everdrive status bits
#define ED64_SD_STATUS_BUSY               0x0080

#define ED64_SD_ACMD41_TOUT_MS          1000
#define ED64_SD_ACMD41_WAIT_MS            10

// Everdrive SD mode commands
#define ED64_SD_CMD0                    0x40 // Go idle state
#define ED64_SD_CMD2                    0x42 // All send CID
#define ED64_SD_CMD3                    0x43 // Send relative addr
#define ED64_SD_CMD6                    0x46
#define ED64_SD_CMD7                    0x47 // Select/deselect card
#define ED64_SD_CMD8                    0x48 // Send interface condition
#define ED64_SD_CMD12                   0x4C // Stop transmission on multiple block read
#define ED64_SD_CMD18                   0x52 // Read multiple block
#define ED64_SD_CMD25                   0x59 // Write multiple block
#define ED64_SD_CMD55                   0x77 // Application specific cmd
#define ED64_SD_CMD41                   0x69

// Everdrive SD implementation state bits
#define ED64_SD_MODE_ACCESS             0x03
#define ED64_SD_MODE_COMM               0x0C
// We only support verion > 2.0, so this is just on/off
#define ED64_SD_MODE_IS_HC              0x40

// Everdrive SD data access mode
#define ED64_SD_MODE_NONE               0x00
#define ED64_SD_MODE_BLOCK_READ         0x01
#define ED64_SD_MODE_BLOCK_WRITE        0x02

// Everdrive communication mode - see everdrive_sd_set_mode
#define ED64_SD_MODE_CMD_READ           0x00
#define ED64_SD_MODE_CMD_WRITE          0x04
#define ED64_SD_MODE_DATA_READ          0x08
#define ED64_SD_MODE_DATA_WRITE         0x0C

static uint32_t everdrive_sd_active_mode;

static uint32_t __attribute__((aligned(16))) everdrive_sd_config;
// Sets how many bits are read/written at a time, per lane. This is equal to the
// times the clock line is toggled per read/write. The actual data manipulated
// depends on the mode (see everdrive_sd_set_mode)
static void set_everdrive_sd_bitlen(uint8_t val) {
	if((everdrive_sd_config & ED64_SD_CFG_BITLEN) == val) return;
	everdrive_sd_config &= ~ED64_SD_CFG_BITLEN;
	everdrive_sd_config |= (val & ED64_SD_CFG_BITLEN);
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_STATUS, everdrive_sd_config);
}

// Set the mode to talk to the SD card. In ED64_SD_MODE_CMD_X modes, the
// bytes are read from/written to the cmd line. WithED64_SD_MODE_DATA_X,
// the provided bytes are written in SD wide bus format. e.g for [abcd efgh],
// the output on the 4 data lanes will look like;
// dat3: ae
// dat2: bf
// dat1: cg
// dat0: dh
// Effectively, the clock line will be toggled by the amount set by
// set_everdrive_sd_bitlen for each read/write. For example on the data mode,
// setting the bit len to 2 will output a single byte for every
// everdrive_sd_write
static void everdrive_sd_set_mode(uint8_t mode) {
	if ((everdrive_sd_active_mode & ED64_SD_MODE_COMM) == mode) return;
	everdrive_sd_active_mode &= ~ED64_SD_MODE_COMM;
	everdrive_sd_active_mode |= mode;
	uint32_t old_cfg = everdrive_sd_config;
	set_everdrive_sd_bitlen(0);
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_BASE + mode, 0xffff);
	everdrive_sd_config = old_cfg;
	// This seems necessary for everdrive somehow. If we don't try to set the
	// bit length and restore, is not necessary.
	wait_ticks(75);
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_STATUS, everdrive_sd_config);
}

void everdrive_sd_busy() {
	while ((io_read(ED64_BASE_ADDRESS + ED64_REGISTER_SD_STATUS)
		& ED64_SD_STATUS_BUSY) != 0);
}

void everdrive_sd_write_command(uint8_t val) {
	everdrive_sd_set_mode(ED64_SD_MODE_CMD_WRITE);
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_BASE + ED64_SD_MODE_CMD_WRITE, val);
	everdrive_sd_busy();
}

uint8_t everdrive_sd_read_command() {
	everdrive_sd_set_mode(ED64_SD_MODE_CMD_READ);
	// Even though this is exactly the same command as everdrive_sd_set_mode, it is
	// required to actually read from the register.
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_BASE + ED64_SD_MODE_CMD_READ, 0xffff);
	everdrive_sd_busy();
	return io_read(ED64_BASE_ADDRESS + ED64_REGISTER_SD_BASE + ED64_SD_MODE_CMD_READ);
}

void everdrive_sd_write_data(uint8_t val) {
	everdrive_sd_set_mode(ED64_SD_MODE_DATA_WRITE);
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_BASE + ED64_SD_MODE_DATA_WRITE,  0x00ff | (val << 8));
}

uint8_t everdrive_sd_read_data() {
	everdrive_sd_set_mode(ED64_SD_MODE_DATA_READ);
	// Even though this is exactly the same command as everdrive_sd_set_mode, it is
	// required to actually write to the register.
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_BASE + ED64_SD_MODE_DATA_READ,  0xffff);
	return io_read(ED64_BASE_ADDRESS + ED64_REGISTER_SD_BASE + ED64_SD_MODE_DATA_READ);
}

// Wait for and read the first byte
static bool everdrive_sd_read_first(uint8_t res_buff[5]) {
	uint32_t timeout = 2048;

	set_everdrive_sd_bitlen(8);
	uint8_t res = everdrive_sd_read_command();

	// Effectively we are bitshifting the command buffer until we find a zero
	// (start bit), followed by another zero (transmission bit)
	// We should be able to find it in 8 bytes
	set_everdrive_sd_bitlen(1);
	while ((res & 0xC0) != 0) {
		if (!timeout--) return false;
		res = everdrive_sd_read_command();
	}
	if (res_buff != NULL) {
		res_buff[0] = res;
	}
	return true;
}

// Wait for and read an RX like response
static bool everdrive_sd_read_response(uint8_t res_buff[5]) {
	uint8_t timeout = 16;
	if (!everdrive_sd_read_first(res_buff)) {
		return false;
	}

	set_everdrive_sd_bitlen(8);

	if (res_buff != NULL) {
		res_buff[1] = everdrive_sd_read_command();
		res_buff[2] = everdrive_sd_read_command();
		res_buff[3] = everdrive_sd_read_command();
		res_buff[4] = everdrive_sd_read_command();
	}

	// Make sure everything is consumed, we just don't use them
	while(everdrive_sd_read_command() != 0xFF) {
		if (!timeout--) {
			return false;
		}
	};
	return true;
}

static bool everdrive_sd_execute_command(uint8_t resp_buff[5], uint8_t cmd, uint32_t arg) {
	uint64_t crc7 = 0x00; // Most significant byte will be the result

	crc7 = (uint64_t)cmd << 56;
	crc7 |= (uint64_t)arg << 24;
	for (int i = 0; i < 40; i++) {;
		uint64_t hibit = crc7 >> 63;
		crc7 <<= 1;
		if (hibit) crc7 = (crc7 ^ ((uint64_t)0x12 << 56));
	}

	set_everdrive_sd_bitlen(8);

	everdrive_sd_write_command(0xff);
	everdrive_sd_write_command(cmd);
	everdrive_sd_write_command(arg >> 24);
	everdrive_sd_write_command(arg >> 16);
	everdrive_sd_write_command(arg >> 8);
	everdrive_sd_write_command(arg);

	// LSB must always be 1
	everdrive_sd_write_command((crc7 >> 56) | 1);

	// CMD0 does not have a response
	if (cmd == ED64_SD_CMD0) {
		return true;
	}

	if (!everdrive_sd_read_response(resp_buff)) {
		debugf("CMD%u timed out\n", cmd & ~0x40);
		return false;
	};

	return true;
}

static uint8_t everdrive_sd_send_app_command(uint8_t resp_buff[5], uint8_t cmd, uint32_t rca, uint32_t arg) {
	// Next command will be an application specific cmd
	if (!everdrive_sd_execute_command(NULL, ED64_SD_CMD55, rca)) {
		debugf("ACMD%u CMD55 err\n", cmd & ~0x40);
		return false;
	};

	return everdrive_sd_execute_command(resp_buff, cmd, arg);
}

// Interleaves lower 32 bits of two uint64s into a uint64.
// t =   **** **** **** **** abcd efgh ijkl mnop
// x =   **** **** **** **** rstu wxyz ABCD EFGH
// into: arbs ctdu ewfx gyhz iAjB kClD mEnF oGpH
static uint64_t everdrive_sd_interleave_bits(uint64_t t, uint64_t x) {
	t = (t | (t << 16)) & 0x0000FFFF0000FFFF;
	t = (t | (t << 8)) & 0x00FF00FF00FF00FF;
	t = (t | (t << 4)) & 0x0F0F0F0F0F0F0F0F;
	t = (t | (t << 2)) & 0x3333333333333333;
	t = (t | (t << 1)) & 0x5555555555555555;

	x = (x | (x << 16)) & 0x0000FFFF0000FFFF;
	x = (x | (x << 8)) & 0x00FF00FF00FF00FF;
	x = (x | (x << 4)) & 0x0F0F0F0F0F0F0F0F;
	x = (x | (x << 2)) & 0x3333333333333333;
	x = (x | (x << 1)) & 0x5555555555555555;
	return (t << 1) | x;
}

static void everdrive_sd_crc16(const uint8_t* data_p, uint16_t *crc_out){
	uint64_t t, x, y;
	uint8_t tx;
	uint16_t dat_crc[4] = {0x0000, 0x0000, 0x0000, 0x0000};

	for (int k = 0; k < 64; k++){
		// Convert 8 bytes of data into a uint64 representing data on 4 parallel
		// lanes (dat0-3) of wide bus SD data format such that we can compute
		// individual lane's CRCs

		// Pack into 64bits
		//      0       1       2       3       4       5       6      7
		// x <- [63..56][55..48][47..40][39..32][31..24][23..16][15..8][7..0]
		x = ((uint64_t)data_p[0]<<56) | ((uint64_t)data_p[1]<<48) |
			((uint64_t)data_p[2]<<40) | ((uint64_t)data_p[3]<<32) |
			((uint64_t)data_p[4]<<24) | ((uint64_t)data_p[5]<<16) |
			((uint64_t)data_p[6]<<8) | (uint64_t)data_p[7];

		// Transpose every 2x2 bit block in the 8x8 matrix
		// abcd efgh     aick emgo
		// ijkl mnop     bjdl fnhp
		// qrst uvwx     qys0 u2w4
		// yz01 2345  \  rzt1 v3x5
		// 6789 ABCD  /  6E8G AICK
		// EFGH IJKL     7F9H BJDL
		// MNOP QRST     MUOW QYS?
		// UVWX YZ?!     NVPX RZT!
		t = ((x ^ (x >> 7)) & 0x00AA00AA00AA00AA);
		x = (x ^ t ^ (t << 7));

		// Transpose 2x2 blocks inside their 4x4 blocks in the 8x8 matrix
		// aick emgo     aiqy emu2
		// bjdl fnhp     bjrz fnv3
		// qys0 u2w4     cks0 gow4
		// rzt1 v3x5  \  dlt1 hpx5
		// 6E8G AICK  /  6EMU AIQY
		// 7F9H BJDL     7FNV BJRZ
		// MUOW QYS?     8GOW CKS?
		// NVPX RZT!     9HPX DLT!
		t = ((x ^ (x >> 14)) & 0x0000CCCC0000CCCC);
		x = (x ^ t ^ (t << 14));

		// collect successive 4bits to be interleaved with their pair
		// t <- 0000 0000 0000 0000 0000 0000 0000 0000 aiqy 6EMU bjrz 7FNV cks0 8GOW dlt1 9HPX
		// x <- 0000 0000 0000 0000 0000 0000 0000 0000 emu2 AIQY fnv3 BJRZ gow4 CKS? hpx5 DLT!
		t = ((x & 0xF0F0F0F000000000) >> 32) | ((x & 0x00000000F0F0F0F0) >> 4);
		x = (((x & 0x0F0F0F0F00000000) >> 28) | (x & 0x000000000F0F0F0F));

		// interleave 4 bits to form the real bytes
		x = everdrive_sd_interleave_bits(t, x);

		// At this point x is properly interleaved.
		//      0       1       2       3       4       5       6      7
		// x <- [63..56][55..48][47..40][39..32][31..24][23..16][15..8][7..0]
		//      |----------dat3||----------dat2||----------dat1||-------dat0|

		// For every dat line
		for (int i = 3; i >= 0; i--) {
			tx = dat_crc[i] >> 8 ^ (x >> (i * 16 + 8));
			tx ^= tx >> 4;
			dat_crc[i] = (dat_crc[i] << 8) ^ (uint16_t)(tx << 12) ^(uint16_t)(tx << 5) ^ tx;

			tx = dat_crc[i] >> 8 ^ (x >> (i * 16));
			tx ^= tx >> 4;
			dat_crc[i] = (dat_crc[i] << 8) ^ (uint16_t)(tx << 12) ^ (uint16_t)(tx << 5) ^ tx;
		}

		data_p += 8;
	}

	// The hardware interface will write any given data to the lanes in packed
	// format so we need to interleave the crc to take a bit from each CRC per
	// line
	t = (uint64_t)dat_crc[3] << 32 | dat_crc[2];
	y = (uint64_t)dat_crc[1] << 32 | dat_crc[0];

	t = everdrive_sd_interleave_bits(t, y);;

	y = t & 0x00000000FFFFFFFF;
	t = t >> 32;
	
	t = everdrive_sd_interleave_bits(t, y);;

	crc_out[0] = t >> 48;
	crc_out[1] = t >> 32;
	crc_out[2] = t >> 16;
	crc_out[3] = t;
}

static bool everdrive_sd_stop_transmission() {
	uint16_t timeout = -1;

	if (!everdrive_sd_execute_command(NULL, ED64_SD_CMD12, 0)) return false;

	// Wait until the buffer is cleaned
	set_everdrive_sd_bitlen(8);
	while (everdrive_sd_read_data() != 0xff) {
		if (!timeout--) {
			debugf("Buffer timed out\n");
			return false;
		}
	}

	return true;
}

static LBA_t everdrive_sd_address = 0;
static bool everdrive_sd_change_mode(uint32_t mode, LBA_t addr) {
	if (
		(everdrive_sd_active_mode & ED64_SD_MODE_ACCESS) == mode &&
		everdrive_sd_address == addr
	) return true;

	// If the SD card is already in multiblock read/write mode, exit it.
	if ((everdrive_sd_active_mode & ED64_SD_MODE_ACCESS) != ED64_SD_MODE_NONE) {
		if (!everdrive_sd_stop_transmission()) return false;
	}

	if (!everdrive_sd_execute_command(
		NULL,
		mode == ED64_SD_MODE_BLOCK_WRITE ? ED64_SD_CMD25 : ED64_SD_CMD18,
		(everdrive_sd_active_mode & ED64_SD_MODE_IS_HC) ? addr : (addr * 512)
	)) return false;

	everdrive_sd_active_mode &= ~ED64_SD_MODE_ACCESS;
	everdrive_sd_active_mode |= mode;
	everdrive_sd_address = addr;
	return true;
}

// Everdrive OS already does this but this is still necessary to find out if
// the card is HC or not. Might be a simpler way in practice to just read OCR
// CMD58 does not seem to work on its own.
static DSTATUS fat_disk_initialize_everdrive(void) {
	uint32_t sd_rca;

	// Set lo speed for initialization and initialize everdrive_sd_config
	everdrive_sd_config = 1;
	set_everdrive_sd_bitlen(0);
	// Initialize active mode
	everdrive_sd_active_mode = ED64_SD_MODE_NONE | ED64_SD_MODE_CMD_READ;
	everdrive_sd_set_mode(ED64_SD_MODE_CMD_WRITE);


	// Put in idle
	everdrive_sd_execute_command(NULL, ED64_SD_CMD0, 0);

	uint8_t resp_buff[5];

	// IF cond with 4 bits voltage range 2.7-3.6V (1) and AA as the check pattern
	if (!everdrive_sd_execute_command(resp_buff, ED64_SD_CMD8, 0x1AA)) return RES_ERROR;

	if (resp_buff[4] != 0xAA) {
		debugf("SD card did not echo AA: %02X\n", resp_buff[4]);
		return RES_ERROR;
	};

	if (resp_buff[3] != 1) {
		debugf("SD card - voltage mismatch\n");
		return RES_ERROR;
	}

	int num_retries = ED64_SD_ACMD41_TOUT_MS / ED64_SD_ACMD41_WAIT_MS;
	while (1) {
		if (!num_retries--) {
			debugf("SD card did not respond\n");
			return RES_ERROR;
			break;
		}

		// Query with HCS and 3.2-3.4V
		if (everdrive_sd_send_app_command(resp_buff, ED64_SD_CMD41, 0, 0x40300000)) {
			// Check ready bit on OCR
			if ((resp_buff[1] & 0x80) != 0) break;
		}

		wait_ms(ED64_SD_ACMD41_WAIT_MS);
	};

	// Check CCS and set HC mode
	everdrive_sd_active_mode |= resp_buff[1] & 0x40;

	if (!everdrive_sd_execute_command(NULL, ED64_SD_CMD2, 0)) return RES_ERROR;

	if (!everdrive_sd_execute_command(resp_buff, ED64_SD_CMD3, 0)) return RES_ERROR;

	sd_rca = 	(resp_buff[1] << 24) |
				(resp_buff[2] << 16) |
				(resp_buff[3] << 8) |
				resp_buff[4];

	if (!everdrive_sd_execute_command(NULL, ED64_SD_CMD7, sd_rca)) return RES_ERROR;

	// Set bus width to 4
	if (!everdrive_sd_send_app_command(NULL, ED64_SD_CMD6, sd_rca, 0x2)) {
		debugf("ACMD6 err\n");
		return RES_ERROR;
	};

	// Set hi speed
	everdrive_sd_config |= ED64_SD_CFG_SPEED;
	io_write(ED64_BASE_ADDRESS + ED64_REGISTER_SD_STATUS, everdrive_sd_config);

	return RES_OK;
}

static DRESULT fat_disk_read_everdrive(BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

	uint8_t crc[8];
	DRESULT ret_val = RES_OK;

	// Overclock the PI
	uint32_t old_pw = PI_regs->dom1_pulse_width;
	io_write((uint32_t)&PI_regs->dom1_pulse_width, 0x09);

	if (!everdrive_sd_change_mode(ED64_SD_MODE_BLOCK_READ, sector)) {
		ret_val = RES_ERROR; goto cleanup;
	};

	for (int i=0;i<count;i++)
	{
		uint16_t timeout = -1;
		uint8_t result;
		// Each 1 bit everdrive_sd_read_data shifts 4 bit of data from the four
		// data lanes. To find the start marker, wait for all lanes to go low to
		// start the transfer.
		set_everdrive_sd_bitlen(1);
		do {
			if (!timeout--) {
				debugf("Data token timeout\n");
				ret_val = RES_ERROR; goto cleanup;
			}
			result = everdrive_sd_read_data();
		} while (result != 0xf0);

		// It is also possible to read the data one byte at a time but this is
		// inefficient - left here for documentation only.
		// set_everdrive_sd_bitlen(2); // Read a byte from wide bus each time
		// for(int j = 0; j < 512; j++) {
		// 	buff[j] = everdrive_sd_read_data();
		// }

		set_everdrive_sd_bitlen(4);

		data_cache_hit_writeback_invalidate(buff, 512);
		dma_read(buff,  ED64_BASE_ADDRESS + ED64_SD_IO_BUFFER, 512);

		// TODO: actually check the CRC?
		data_cache_hit_writeback_invalidate(crc, 8);
		dma_read(crc, ED64_BASE_ADDRESS + ED64_SD_IO_BUFFER, 8);
		buff += 512;
	}

	everdrive_sd_address = sector + count;

cleanup:
	io_write((uint32_t)&PI_regs->dom1_pulse_width, old_pw);
	if (ret_val == RES_OK) return RES_OK;

	// Do error cleanup - At this point it is difficult to know which sector we
	// were at, so this will stop multi block transmission and will require a
	// mode change upon a new fat_disk_read_everdrive thus everdrive_sd_address
	// is not relevant anymore.
	everdrive_sd_change_mode(ED64_SD_MODE_NONE, 0);

	return ret_val;
}

static DRESULT fat_disk_write_everdrive(const BYTE* buff, LBA_t sector, UINT count) {
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

	uint8_t result;
	DRESULT ret_val = RES_OK;

	// Overclock the PI
	uint32_t old_pw = PI_regs->dom1_pulse_width;
	io_write((uint32_t)&PI_regs->dom1_pulse_width, 0x09);

	if(!everdrive_sd_change_mode(ED64_SD_MODE_BLOCK_WRITE, sector)) {
		ret_val = RES_ERROR; goto cleanup;
	};

	uint16_t crc[4], timeout;

	for (int i=0;i<count;i++)
	{
		set_everdrive_sd_bitlen(2);
		everdrive_sd_write_data(0xff);
		everdrive_sd_write_data(0xf0); // Pull all lines low to start transfer

		set_everdrive_sd_bitlen(4);

		if (((uint32_t)buff & 7) == 0)
		{
			data_cache_hit_writeback(buff, 512);
			dma_write_raw_async(buff, ED64_BASE_ADDRESS + ED64_SD_IO_BUFFER, 512);
		}
		else
		{
			typedef uint32_t u_uint32_t __attribute__((aligned(1)));

			uint32_t* dst = (uint32_t*)(ED64_BASE_ADDRESS + ED64_SD_IO_BUFFER);
			u_uint32_t* src = (u_uint32_t*)buff;
			for (int i = 0; i < 512/16; i++)
			{
				uint32_t a = *src++; uint32_t b = *src++; uint32_t c = *src++; uint32_t d = *src++;
				*dst++ = a;          *dst++ = b;          *dst++ = c;          *dst++ = d;
			}
		}

		everdrive_sd_crc16((void*)buff, crc);

		data_cache_hit_writeback(crc, 8);
		dma_write(crc, ED64_BASE_ADDRESS + ED64_SD_IO_BUFFER, 8);

		// Each read will shift 4 bit of parallel data. dat0 will go low when we
		// have the data response token's status. Read it from the same line
		// once found. Swiching to command mode does not work here although they
		// should be using the same line for the response?
		set_everdrive_sd_bitlen(1);
		timeout = 1024;
		do {
			if (!timeout--) {
				debugf("Write resp timeout\n");
				ret_val = RES_ERROR; goto cleanup;
			}
			result = everdrive_sd_read_data();
		} while (result != 0xFE);

		result = (everdrive_sd_read_data() & 1) << 2;
		result |= (everdrive_sd_read_data() & 1) << 1;
		result |= (everdrive_sd_read_data() & 1);

		if (result == 0b101) {
			debugf("Write CRC mismatch\n");
			ret_val = RES_ERROR; goto cleanup;
		}

		if (result != 0b010) {
			debugf("Write Error\n");
			ret_val = RES_ERROR; goto cleanup;
		}

		// Consume all remaining data
		timeout = -1;
		while(everdrive_sd_read_data() != 0xFF) {
			if(!timeout--) {
				debugf("Flush data timeout\n");
				ret_val = RES_ERROR; goto cleanup;
			}
		};
		buff += 512;
	}

everdrive_sd_address = sector + count;

cleanup:
	io_write((uint32_t)&PI_regs->dom1_pulse_width, old_pw);

	if (ret_val == RES_OK) return RES_OK;

	// Do error cleanup - At this point it is difficult to know which sector we
	// were at, so this will stop multi block transmission and will require a
	// mode change upon a new fat_disk_write_everdrive thus everdrive_sd_address
	// is not relevant anymore.
	everdrive_sd_change_mode(ED64_SD_MODE_NONE, 0);

	return ret_val;
 }