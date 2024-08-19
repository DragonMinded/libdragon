// Decoder for algorithm -lh5- of the LZH family.
// This code is adapted from https://github.com/jca02266/lha
// and has been turned into a single file with the -lh5- algo.

#include "lzh5_internal.h"
#include "ringbuf_internal.h"
#include "../asset_internal.h"

#ifdef N64
#include <malloc.h>
#include "debug.h"
#include "dma.h"
#include "utils.h"
#include "n64sys.h"
#include "dragonfs.h"
#include "asset.h"

#else
#include <stdlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <string.h>

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

typedef struct {

	// File pointer to read from.
	FILE *fp;
	uint32_t rom_addr;

	// Internal cache of bytes read from the input stream.
	uint8_t buf[2][128] __attribute__((aligned(16)));
	uint8_t *buf_ptr, *buf_end;
	int buf_size;
	int cur_buf;

	// Bits from the input stream that are waiting to be read.
	uint64_t bit_buffer;
	int bits;

} BitStreamReader;

// Initialize bit stream reader structure.
static void bit_stream_reader_init(BitStreamReader *reader, FILE *fp, uint32_t rom_addr)
{
	memset(reader, 0, sizeof(BitStreamReader));
	reader->fp = fp;
	reader->rom_addr = rom_addr;
	reader->cur_buf = 1;

	#ifdef N64
	if (reader->rom_addr) {
		data_cache_hit_invalidate(reader->buf[reader->cur_buf^1], sizeof(reader->buf[0]));
		dma_read_raw_async(reader->buf[reader->cur_buf^1], reader->rom_addr, sizeof(reader->buf[0]));
		reader->rom_addr += sizeof(reader->buf[0]);
	}
	#endif
}

static void refill_bits_fetch(BitStreamReader *reader)
{
	reader->cur_buf ^= 1;

	#ifdef N64
	if (reader->rom_addr) {
		data_cache_hit_invalidate(reader->buf[reader->cur_buf^1], sizeof(reader->buf[0]));
		dma_read_raw_async(reader->buf[reader->cur_buf^1], reader->rom_addr, sizeof(reader->buf[0]));
		reader->rom_addr += sizeof(reader->buf[0]);
		reader->buf_size = sizeof(reader->buf[0]);
	#else
	if (0) {
	#endif
	} else {
		reader->buf_size = fread(reader->buf[reader->cur_buf], 1, sizeof(reader->buf[0]), reader->fp);
	}
	reader->buf_ptr = reader->buf[reader->cur_buf];
	reader->buf_end = reader->buf[reader->cur_buf] + reader->buf_size;
}

__attribute__((noinline))
static int refill_bits(BitStreamReader *reader)
{
	if (__builtin_expect(reader->buf_ptr >= reader->buf_end, 0)) {
		refill_bits_fetch(reader);
	}

	uint64_t w = *(uint32_t*)reader->buf_ptr;
	if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
		w = __builtin_bswap32(w);
	reader->buf_ptr += 4;
	reader->bit_buffer |= w << (32 - reader->bits);
	reader->bits += 32;
	assert(reader->bits <= 64);
	return reader->buf_size > 0;
}

static inline void fill_bits(BitStreamReader *reader, int n)
{
	reader->bit_buffer <<= n;
	reader->bits -= n;
	assert(reader->bits >= 0);
	if (__builtin_expect(reader->bits <= 32, 0)) {
		refill_bits(reader);
	}
}

static int peek_bits(BitStreamReader *reader, int n)
{
	return reader->bit_buffer >> (64 - n);
}

static int read_bits(BitStreamReader *reader, int n)
{
	int result = peek_bits(reader, n);
	fill_bits(reader, n);
	return result;
}

static int end_bits(BitStreamReader *reader)
{
	return reader->buf_size == 0;
}


#define LZH5_DICBIT         13  /* 2^13 =  8KB sliding dictionary */
#define LZH5_MAXMATCH       256 /* formerly F (not more than UCHAR_MAX + 1) */
#define LZH5_THRESHOLD      3   /* choose optimal value */

#define LZH5_NP          (LZH5_DICBIT + 1)
#define LZH5_NT          (sizeof(uint16_t)*8 + 3)
#define LZH5_NC          (255 + LZH5_MAXMATCH + 2 - LZH5_THRESHOLD)

#define LZH5_PBIT        4       /* smallest integer such that (1 << PBIT) > * NP */
#define LZH5_TBIT        5       /* smallest integer such that (1 << TBIT) > * NT */
#define LZH5_CBIT        9       /* smallest integer such that (1 << CBIT) > * NC */

/*      #if NT > NP #define NPT NT #else #define NPT NP #endif  */
#define LZH5_NPT         0x80

#define C_TABLE_BITS    8
#define PT_TABLE_BITS   8

typedef struct {
	uint16_t left[2 * LZH5_NC - 1], right[2 * LZH5_NC - 1];
	uint16_t c_table[1<<C_TABLE_BITS];
	uint16_t pt_table[1<<PT_TABLE_BITS];
	uint8_t c_len[LZH5_NC];
	uint8_t pt_len[LZH5_NPT];
	int blocksize;

	BitStreamReader *reader;
} HuffDecoder;


static void make_table(HuffDecoder 	*hd, int nchar, uint8_t *bitlen, int tablebits, uint16_t* table)
{
    uint16_t count[17];  /* count of bitlen */
    uint16_t weight[17]; /* 0x10000ul >> bitlen */
    uint16_t start[17];  /* first code of bitlen */
    uint16_t total;
    unsigned int i, l;
    int j, k, m, n, avail;
    uint16_t *p;

    avail = nchar;

    /* initialize */
    for (i = 1; i <= 16; i++) {
        count[i] = 0;
        weight[i] = 1 << (16 - i);
    }

    /* count */
    for (i = 0; i < nchar; i++) {
        if (bitlen[i] > 16) {
            /* CVE-2006-4335 */
            assertf(0, "Bad table (case a)");
        }
        else
            count[bitlen[i]]++;
    }

    /* calculate first code */
    total = 0;
    for (i = 1; i <= 16; i++) {
        start[i] = total;
        total += weight[i] * count[i];
    }
    if ((total & 0xffff) != 0 || tablebits > 16) { /* 16 for weight below */
        assertf(0, "make_table(): Bad table (case b)");
    }

    /* shift data for make table. */
    m = 16 - tablebits;
    for (i = 1; i <= tablebits; i++) {
        start[i] >>= m;
        weight[i] >>= m;
    }

    /* initialize */
    j = start[tablebits + 1] >> m;
    k = MIN(1 << tablebits, 4096);
    if (j != 0)
        for (i = j; i < k; i++)
            table[i] = 0;

    /* create table and tree */
    for (j = 0; j < nchar; j++) {
        k = bitlen[j];
        if (k == 0)
            continue;
        l = start[k] + weight[k];
        if (k <= tablebits) {
            /* code in table */
            l = MIN(l, 4096);
            for (i = start[k]; i < l; i++)
                table[i] = j;
        }
        else {
            /* code not in table */
            i = start[k];
            if ((i >> m) > 4096) {
                /* CVE-2006-4337 */
                assertf(0, "Bad table (case c)");
            }
            p = &table[i >> m];
            i <<= tablebits;
            n = k - tablebits;
            /* make tree (n length) */
            while (--n >= 0) {
                if (*p == 0) {
                    hd->right[avail] = hd->left[avail] = 0;
                    *p = avail++;
                }
                if (i & 0x8000)
                    p = &hd->right[*p];
                else
                    p = &hd->left[*p];
                i <<= 1;
            }
            *p = j;
        }
        start[k] = l;
    }
}

static void read_pt_len(HuffDecoder *hd, short nn, short nbit, short i_special)
{
    int i, c, n;

    n = read_bits(hd->reader, nbit);
    if (n == 0) {
        c = read_bits(hd->reader, nbit);
        for (i = 0; i < nn; i++)
            hd->pt_len[i] = 0;
        for (i = 0; i < (1<<PT_TABLE_BITS); i++)
            hd->pt_table[i] = c;
    }
    else {
        i = 0;
        while (i < MIN(n, LZH5_NPT)) {
            c = peek_bits(hd->reader, 3);
            if (c != 7)
                fill_bits(hd->reader, 3);
            else {
                uint64_t mask = 1ull << (64 - 4);
                while (mask & hd->reader->bit_buffer) {
                    mask >>= 1;
                    c++;
                }
                fill_bits(hd->reader, c - 3);
            }

            hd->pt_len[i++] = c;
            if (i == i_special) {
                c = read_bits(hd->reader, 2);
                while (--c >= 0 && i < LZH5_NPT)
                    hd->pt_len[i++] = 0;
            }
        }
        while (i < nn)
            hd->pt_len[i++] = 0;
        make_table(hd, nn, hd->pt_len, PT_TABLE_BITS, hd->pt_table);
    }
}

static void read_c_len(HuffDecoder *hd)
{
    int i, c, n;

    n = read_bits(hd->reader, LZH5_CBIT);
    if (n == 0) {
        c = read_bits(hd->reader, LZH5_CBIT);
        for (i = 0; i < LZH5_NC; i++)
            hd->c_len[i] = 0;
        for (i = 0; i < (1<<C_TABLE_BITS); i++)
            hd->c_table[i] = c;
    } else {
        i = 0;
        while (i < MIN(n,LZH5_NC)) {
			c = hd->pt_table[peek_bits(hd->reader, PT_TABLE_BITS)];
            if (c >= LZH5_NT) {
                uint64_t mask = 1ull << (64 - PT_TABLE_BITS - 1);
                do {
                    if (hd->reader->bit_buffer & mask)
                        c = hd->right[c];
                    else
                        c = hd->left[c];
                    mask >>= 1;
					// assert(mask || c != left[c]);
                } while (c >= LZH5_NT);
            }
            fill_bits(hd->reader, hd->pt_len[c]);
            if (c <= 2) {
                if (c == 0)
                    c = 1;
                else if (c == 1)
                    c = read_bits(hd->reader, 4) + 3;
                else
                    c = read_bits(hd->reader, LZH5_CBIT) + 20;
                while (--c >= 0)
                    hd->c_len[i++] = 0;
            }
            else
                hd->c_len[i++] = c - 2;
        }
        while (i < LZH5_NC)
            hd->c_len[i++] = 0;
        make_table(hd, LZH5_NC, hd->c_len, C_TABLE_BITS, hd->c_table);
    }
}

static uint16_t decode_huff8(HuffDecoder *hd, uint16_t *table, uint8_t *len, int ne)
{
    uint16_t j = table[peek_bits(hd->reader, 8)];
    if (j >= ne) {
        uint64_t mask = 1ull << (64 - 8 - 1);
        do {
            if (hd->reader->bit_buffer & mask)
                j = hd->right[j];
            else
                j = hd->left[j];
            mask >>= 1;
			// assert(mask || j != left[j]);
        } while (j >= ne);
    }
    fill_bits(hd->reader, len[j]);
	return j;
}

static bool decode_new_block(HuffDecoder *hd)
{
	hd->blocksize = read_bits(hd->reader, 16);
	if (end_bits(hd->reader)) {
		hd->blocksize = 0;
		return false;
	}
	read_pt_len(hd, LZH5_NT, LZH5_TBIT, 3);
	read_c_len(hd);
	read_pt_len(hd, LZH5_NP, LZH5_PBIT, -1);
	return true;
}

static inline int huff_decode_code(HuffDecoder *hd)
{
    if (hd->blocksize == 0) {
		if (!decode_new_block(hd))
			return -1;
    }
    hd->blocksize--;
	return decode_huff8(hd, hd->c_table, hd->c_len, LZH5_NC);
}

static inline unsigned short huff_decode_offset(HuffDecoder *hd)
{
	uint16_t j = decode_huff8(hd, hd->pt_table, hd->pt_len, LZH5_NP);
    if (__builtin_expect(j > 1, 1))
        j = (1 << (j - 1)) + read_bits(hd->reader, j - 1);
    return j;
}

static void huff_decode_init(HuffDecoder *hd, BitStreamReader *reader)
{
    memset(hd, 0, sizeof(*hd));
	hd->reader = reader;
	refill_bits(hd->reader);
}


// 16 KiB history ring buffer:
#define HISTORY_BITS    14   /* 2^14 = 16384 */

typedef struct _LHANewDecoder {
	BitStreamReader bit_stream_reader;

	HuffDecoder huff;
} LHANewDecoder;

typedef struct _LHANewDecoderPartial {
	// Decoder
	LHANewDecoder decoder;

	// Ring buffer of past data.  Used for position-based copies.
	decompress_ringbuf_t ringbuf;
	int ringbuf_copy_offset;
	int ringbuf_copy_count;

	int decoded_bytes;

} LHANewDecoderPartial;


static int lha_lh_new_init(LHANewDecoder *decoder, FILE *fp, uint32_t rom_addr)
{
	// Initialize input stream reader.
	bit_stream_reader_init(&decoder->bit_stream_reader, fp, rom_addr);

	huff_decode_init(&decoder->huff, &decoder->bit_stream_reader);
	return 1;
}

static int lha_lh_new_init_partial(LHANewDecoderPartial *decoder, FILE *fp, void *window, int winsize)
{
	lha_lh_new_init(&decoder->decoder, fp, 0);

	// Initialize data structures.
	__ringbuf_init(&decoder->ringbuf, window, winsize);
	decoder->ringbuf_copy_offset = 0;
	decoder->ringbuf_copy_count = 0;

	decoder->decoded_bytes = 0;

	return 1;
}

static size_t lha_lh_new_read_partial(LHANewDecoderPartial *decoder, uint8_t *buf, int sz)
{
	uint8_t *buf_orig = buf;
	int code;

	while (sz > 0) {
		if (decoder->ringbuf_copy_count > 0) {
			int wn = MIN(sz, decoder->ringbuf_copy_count);
			__ringbuf_copy(&decoder->ringbuf, decoder->ringbuf_copy_offset, buf, wn);
			sz -= wn;
			if(buf) buf += wn;
			decoder->ringbuf_copy_count -= wn;
			continue;
		}

		code = huff_decode_code(&decoder->decoder.huff);
		if (code < 0) {
			break;
		}

		if (code < 256) {
			if (buf) *buf++ = code;
			__ringbuf_writebyte(&decoder->ringbuf, code);
			sz--;
		} else {
			int offset = huff_decode_offset(&decoder->decoder.huff);
			if (offset < 0) {
				assert(0);
				break;
			}

			decoder->ringbuf_copy_offset = offset + 1;
			decoder->ringbuf_copy_count = code - 256 + LZH5_THRESHOLD;
		}
	}

	decoder->decoded_bytes += buf - buf_orig;
	return buf - buf_orig;
}

static size_t lha_lh_new_read_full(LHANewDecoder *decoder, uint8_t *buf, int sz)
{
	uint8_t *buf_orig = buf;
	int code;
	
	if (buf == NULL) goto end;

	while (sz > 0) {
		code = huff_decode_code(&decoder->huff);
		if (code < 0) {
			return 0;
		}

		// The code may be either a literal byte value or a copy command.
		if (code < 256) {
			*buf++ = (uint8_t) code;
			sz--;
		} else {
			int count = code - 256 + LZH5_THRESHOLD;
			int offset = huff_decode_offset(&decoder->huff);
			if (offset < 0) {
				return 0;
			}
			uint8_t *src = buf - offset - 1;

			count = count < sz ? count : sz;
			sz -= count;

			if (offset > 7) {
				while (count >= 8) {
					typedef uint64_t u_uint64_t __attribute__((aligned(1)));
					*(u_uint64_t*)buf = *(u_uint64_t*)src;
					buf += 8;
					src += 8;
					count -= 8;
				}
			}
			while (count > 0) {
				*buf++ = *src++;
				count--;
			}
		}
	}

end:
	return buf - buf_orig;
}


/*************************************************
 * Libdragon API
 *************************************************/

_Static_assert(sizeof(LHANewDecoderPartial) <= DECOMPRESS_LZH5_STATE_SIZE, "LZH5 state size is wrong");

void decompress_lzh5_init(void *state, FILE *fp, int winsize)
{
	LHANewDecoderPartial *decoder = (LHANewDecoderPartial *)state;
	lha_lh_new_init_partial(decoder, fp, state+sizeof(LHANewDecoderPartial), winsize);
}

ssize_t decompress_lzh5_read(void *state, void *buf, size_t len)
{
	LHANewDecoderPartial *decoder = (LHANewDecoderPartial *)state;
	return lha_lh_new_read_partial(decoder, buf, len);
}

int decompress_lzh5_pos(void *state) {
	LHANewDecoderPartial *decoder = (LHANewDecoderPartial *)state;
	return decoder->decoded_bytes;
}

void* decompress_lzh5_full(const char *fn, FILE *fp, size_t cmp_size, size_t size)
{
	void *s = memalign(ASSET_ALIGNMENT, size);
	assertf(s, "asset_load: out of memory");

	uint32_t rom_addr = 0;
	#ifdef N64
	if (strncmp(fn, "rom:/", 5) == 0) {
		rom_addr = (dfs_rom_addr(fn+5) & 0x1fffffff) + ftell(fp);
	}
	#endif

	LHANewDecoder decoder;
	lha_lh_new_init(&decoder, fp, rom_addr);
	int n = lha_lh_new_read_full(&decoder, s, size); (void)n;
	assertf(n == size, "asset: decompression error on file %s: corrupted? (%d/%d)", fn, n, size);

	return s;
}

#undef MIN
