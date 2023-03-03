// Decoder for algorithm -lh5- of the LZH family.
// This code comes from https://github.com/fragglet/lhasa
// and has been turned into a single file header with only
// the -lh5- algo.
// This was also modified to allow for full streaming decompression
// (up to 1 byte at a time). Before, the code would decompress one
// internal LHA block at a time, writing a non predictable number of 
// bytes in the output buffer.
// This file is ISC Licensed.

#include "lzh5_internal.h"

//////////////////////// bit_stream_reader.c

/*

Copyright (c) 2011, 2012, Simon Howard

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted, provided
that the above copyright notice and this permission notice appear
in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 */

#include <string.h>

//
// Data structure used to read bits from an input source as a stream.
//
// This file is designed to be #included by other source files to
// make a complete decoder.
//

typedef struct {

	// File pointer to read from.

	FILE *fp;

	// Internal cache of bytes read from the input stream.

	uint8_t buf[128] __attribute__((aligned(8)));
	int buf_idx;
	int buf_size;

	// Bits from the input stream that are waiting to be read.

	uint64_t bit_buffer;
	int bits;

} BitStreamReader;

// Initialize bit stream reader structure.

static void bit_stream_reader_init(BitStreamReader *reader, FILE *fp)
{
	reader->fp = fp;
	reader->buf_idx = 0;
	reader->buf_size = 0;
	reader->bits = 0;
	reader->bit_buffer = 0;
}

// Refill the bit buffer with other 64 bits from the input stream.

static int refill_bits(BitStreamReader *reader)
{
	if (reader->buf_idx >= reader->buf_size) {
		reader->buf_size = fread(reader->buf, 1, sizeof(reader->buf), reader->fp);
		reader->buf_idx = 0;
	}

	reader->bit_buffer = *(uint64_t*)(&reader->buf[reader->buf_idx]);
	if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
		reader->bit_buffer = __builtin_bswap64(reader->bit_buffer);
	reader->bits = (reader->buf_size - reader->buf_idx) * 8;
	if (reader->bits > 64)
		reader->bits = 64;
	reader->buf_idx += 8;
	return reader->buf_size > 0;
}

// Internal continuation of read_bits
// Returns -1 for failure.

__attribute__((noinline))
static int __read_bits2(BitStreamReader *reader,
                      unsigned int n, int result)
{
	if (!refill_bits(reader))
		return -1;
	result |= reader->bit_buffer >> (64 - n);
	reader->bit_buffer <<= n;
	reader->bits -= n;
	return result;
}

// Read multiple bits from the input stream.
// Returns -1 for failure.

__attribute__((noinline))
static int read_bits(BitStreamReader *reader,
                     unsigned int n)
{
	int result = reader->bit_buffer >> (64 - n);
	reader->bit_buffer <<= n;
	reader->bits -= n;
	if (__builtin_expect(reader->bits >= 0, 1)) {
		return result;
	}
	return __read_bits2(reader, -reader->bits, result);
}


// Read a bit from the input stream.
// Returns -1 for failure.
static int read_bit(BitStreamReader *reader)
{
	return read_bits(reader, 1);
}


static uint64_t peek_bits(BitStreamReader *reader, int *n)
{
	*n = reader->bits;
	return reader->bit_buffer;
}

static int skip_bits(BitStreamReader *reader, int n)
{
	reader->bit_buffer <<= n;
	reader->bits -= n;
	if (__builtin_expect(reader->bits <= 0, 0)) {
		refill_bits(reader);
		if (reader->bits < 0)
			return -1;
	}
	return 0;
}


//////////////////////// tree_decode.c
typedef uint16_t TreeElement;

/*

Copyright (c) 2011, 2012, Simon Howard

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted, provided
that the above copyright notice and this permission notice appear
in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 */

// Common tree decoding code.
//
// A recurring feature used by the different LHA algorithms is to
// encode a set of codes, which have varying bit lengths. This is
// implemented using a binary tree, stored inside an array of
// elements.
//
// This file is implemented as a "template" file to be #include-d by
// other files. The typedef for TreeElement must be defined before
// include.


// Upper bit is set in a node value to indicate a leaf.

#define TREE_NODE_LEAF    (TreeElement) (1 << (sizeof(TreeElement) * 8 - 1))

// Structure used to hold data needed to build the tree.

typedef struct {
	// The tree data and its size (must not be exceeded)

	TreeElement *tree;
	unsigned int tree_len;

	// Counter used to allocate entries from the tree.
	// Every time a new node is allocated, this increase by 2.

	unsigned int tree_allocated;

	// The next tree entry.
	// As entries are allocated sequentially, the range from
	// next_entry..tree_allocated-1 constitutes the indices into
	// the tree that are available to be filled in. By the
	// end of the tree build, next_entry should = tree_allocated.

	unsigned int next_entry;
} TreeBuildData;

// Initialize all elements of the given tree to a good initial state.

static void init_tree(TreeElement *tree, size_t tree_len)
{
	unsigned int i;

	for (i = 0; i < tree_len; ++i) {
		tree[i] = TREE_NODE_LEAF;
	}
}

// Set tree to always decode to a single code.

static void set_tree_single(TreeElement *tree, TreeElement code)
{
	tree[0] = (TreeElement) code | TREE_NODE_LEAF;
}

// "Expand" the list of queue entries. This generates a new child
// node at each of the entries currently in the queue, adding the
// children of those nodes into the queue to replace them.
// The effect of this is to add an extra level to the tree, and
// to increase the tree depth of the indices in the queue.

static void expand_queue(TreeBuildData *build)
{
	unsigned int end_offset;
	unsigned int new_nodes;

	// Sanity check that there is enough space in the tree for
	// all the new nodes.

	new_nodes = (build->tree_allocated - build->next_entry) * 2;

	if (build->tree_allocated + new_nodes > build->tree_len) {
		return;
	}

	// Go through all entries currently in the allocated range, and
	// allocate a subnode for each.

	end_offset = build->tree_allocated;

	while (build->next_entry < end_offset) {
		build->tree[build->next_entry] = build->tree_allocated;
		build->tree_allocated += 2;
		++build->next_entry;
	}
}

// Read the next entry from the queue of entries waiting to be used.

static unsigned int read_next_entry(TreeBuildData *build)
{
	unsigned int result;

	// Sanity check.

	if (build->next_entry >= build->tree_allocated) {
		return 0;
	}

	result = build->next_entry;
	++build->next_entry;

	return result;
}

// Add all codes to the tree that have the specified length.
// Returns non-zero if there are any entries in code_lengths[] still
// waiting to be added to the tree.

static int add_codes_with_length(TreeBuildData *build,
                                 uint8_t *code_lengths,
                                 unsigned int num_code_lengths,
                                 unsigned int code_len)
{
	unsigned int i;
	unsigned int node;
	int codes_remaining;

	codes_remaining = 0;

	for (i = 0; i < num_code_lengths; ++i) {

		// Does this code belong at this depth in the tree?

		if (code_lengths[i] == code_len) {
			node = read_next_entry(build);

			build->tree[node] = (TreeElement) i | TREE_NODE_LEAF;
		}

		// More work to be done after this pass?

		else if (code_lengths[i] > code_len) {
			codes_remaining = 1;
		}
	}

	return codes_remaining;
}

// Build a tree, given the specified array of codes indicating the
// required depth within the tree at which each code should be
// located.

static void build_tree(TreeElement *tree, size_t tree_len,
                       uint8_t *code_lengths, unsigned int num_code_lengths)
{
	TreeBuildData build;
	unsigned int code_len;

	build.tree = tree;
	build.tree_len = tree_len;

	// Start with a single entry in the queue - the root node
	// pointer.

	build.next_entry = 0;

	// We always have the root ...

	build.tree_allocated = 1;

	// Iterate over each possible code length.
	// Note: code_len == 0 is deliberately skipped over, as 0
	// indicates "not used".

	code_len = 0;

	do {
		// Advance to the next code length by allocating extra
		// nodes to the tree - the slots waiting in the queue
		// will now be one level deeper in the tree (and the
		// codes 1 bit longer).

		expand_queue(&build);
		++code_len;

		// Add all codes that have this length.

	} while (add_codes_with_length(&build, code_lengths,
	                               num_code_lengths, code_len));
}

/*
static void display_tree(TreeElement *tree, unsigned int node, int offset)
{
	unsigned int i;

	if (node & TREE_NODE_LEAF) {
		for (i = 0; i < offset; ++i) putchar(' ');
		printf("leaf %i\n", node & ~TREE_NODE_LEAF);
	} else {
		for (i = 0; i < offset; ++i) putchar(' ');
		printf("0 ->\n");
		display_tree(tree, tree[node], offset + 4);
		for (i = 0; i < offset; ++i) putchar(' ');
		printf("1 ->\n");
		display_tree(tree, tree[node + 1], offset + 4);
	}
}
*/

// Read bits from the input stream, traversing the specified tree
// from the root node until we reach a leaf.  The leaf value is
// returned.

static int read_from_tree(BitStreamReader *reader, TreeElement *tree)
{
	TreeElement code;
	int bit;
	uint64_t bits=0; int n=0, used=0;

	// Start from root.

	code = tree[0];

	while ((code & TREE_NODE_LEAF) == 0) {

		if (used == n) {
			if (skip_bits(reader, used) < 0)
				return -1;
			bits = peek_bits(reader, &n);
			used = 0;
		}

		bit = bits >> 63;
		bits <<= 1;
		used++;

		code = tree[code + (unsigned int) bit];
	}

	if (skip_bits(reader, used) < 0)
		return -1;

	// Mask off leaf bit to get the plain code.

	return (int) (code & ~TREE_NODE_LEAF);
}




//////////////////////// lh5_decoder.c

/*

Copyright (c) 2011, 2012, Simon Howard

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted, provided
that the above copyright notice and this permission notice appear
in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 */

//
// Decoder for the -lh5- algorithm.
//
// This is the "new" algorithm that appeared in LHA v2, replacing
// the older -lh1-. -lh4- seems to be identical to -lh5-.
//

// 16 KiB history ring buffer:

#define HISTORY_BITS    14   /* 2^14 = 16384 */

// Number of bits to encode HISTORY_BITS:

#define OFFSET_BITS     4

// Name of the variable for the encoder:

#define DECODER_NAME lha_lh5_decoder


//////////////////////// lh_new_decoder.c


/*

Copyright (c) 2011, 2012, Simon Howard

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted, provided
that the above copyright notice and this permission notice appear
in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 */

// Decoder for "new-style" LHA algorithms, used with LHA v2 and onwards
// (-lh4-, -lh5-, -lh6-, -lh7-).
//
// This file is designed to be a template. It is #included by other
// files to generate an optimized decoder.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


// Threshold for copying. The first copy code starts from here.

#define COPY_THRESHOLD       3 /* bytes */

// Ring buffer containing history has a size that is a power of two.
// The number of bits is specified.

#define RING_BUFFER_SIZE     (1 << HISTORY_BITS)

// Required size of the output buffer.  At most, a single call to read()
// might result in a copy of the entire ring buffer.

#define OUTPUT_BUFFER_SIZE   RING_BUFFER_SIZE

// Number of different command codes. 0-255 range are literal byte
// values, while higher values indicate copy from history.

#define NUM_CODES            510

// Number of possible codes in the "temporary table" used to encode the
// codes table.

#define MAX_TEMP_CODES       20

typedef struct _LHANewDecoder {
	// Input bit stream.

	BitStreamReader bit_stream_reader;

	// Number of commands remaining before we start a new block.

	unsigned int block_remaining;

	// Table used for the code tree.

	TreeElement code_tree[NUM_CODES * 2];

	// Table used to encode the offset tree, used to read offsets
	// into the history buffer. This same table is also used to
	// encode the temp-table, which is bigger; hence the size.

	TreeElement offset_tree[MAX_TEMP_CODES * 2];
} LHANewDecoder;


typedef struct _LHANewDecoderPartial {
	// Decoder

	LHANewDecoder decoder;

	// Ring buffer of past data.  Used for position-based copies.

	uint8_t ringbuf[RING_BUFFER_SIZE];
	unsigned int ringbuf_pos;
	int ringbuf_copy_pos;
	int ringbuf_copy_count;

	int decoded_bytes;

} LHANewDecoderPartial;


// Initialize the history ring buffer.

static void init_ring_buffer(LHANewDecoderPartial *decoder)
{
	memset(decoder->ringbuf, ' ', RING_BUFFER_SIZE);
	decoder->ringbuf_pos = 0;
	decoder->ringbuf_copy_pos = 0;
	decoder->ringbuf_copy_count = 0;
}

static int lha_lh_new_init(LHANewDecoder *decoder, FILE *fp)
{
	// Initialize input stream reader.

	bit_stream_reader_init(&decoder->bit_stream_reader, fp);

	// First read starts the first block.

	decoder->block_remaining = 0;

	// Initialize tree tables to a known state.

	init_tree(decoder->code_tree, NUM_CODES * 2);
	init_tree(decoder->offset_tree, MAX_TEMP_CODES * 2);

	return 1;
}

static int lha_lh_new_init_partial(LHANewDecoderPartial *decoder, FILE *fp)
{
	lha_lh_new_init(&decoder->decoder, fp);

	// Initialize data structures.

	init_ring_buffer(decoder);

	decoder->decoded_bytes = 0;

	return 1;
}

// Read a length value - this is normally a value in the 0-7 range, but
// sometimes can be longer.

static int read_length_value(LHANewDecoder *decoder)
{
	int i, len;

	len = read_bits(&decoder->bit_stream_reader, 3);

	if (len < 0) {
		return -1;
	}

	if (len == 7) {
		// Read more bits to extend the length until we reach a '0'.

		for (;;) {
			i = read_bit(&decoder->bit_stream_reader);

			if (i < 0) {
				return -1;
			} else if (i == 0) {
				break;
			}

			++len;
		}
	}

	return len;
}

// Read the values from the input stream that define the temporary table
// used for encoding the code table.

static int read_temp_table(LHANewDecoder *decoder)
{
	int i, j, n, len, code;
	uint8_t code_lengths[MAX_TEMP_CODES];

	// How many codes?

	n = read_bits(&decoder->bit_stream_reader, 5);

	if (n < 0) {
		return 0;
	}

	// n=0 is a special case, meaning only a single code that
	// is of zero length.

	if (n == 0) {
		code = read_bits(&decoder->bit_stream_reader, 5);

		if (code < 0) {
			return 0;
		}

		set_tree_single(decoder->offset_tree, code);
		return 1;
	}

	// Enforce a hard limit on the number of codes.

	if (n > MAX_TEMP_CODES) {
		n = MAX_TEMP_CODES;
	}

	// Read the length of each code.

	for (i = 0; i < n; ++i) {
		len = read_length_value(decoder);

		if (len < 0) {
			return 0;
		}

		code_lengths[i] = len;

		// After the first three lengths, there is a 2-bit
		// field to allow skipping over up to a further three
		// lengths. Not sure of the reason for this ...

		if (i == 2) {
			len = read_bits(&decoder->bit_stream_reader, 2);

			if (len < 0) {
				return 0;
			}

			for (j = 0; j < len; ++j) {
				++i;
				code_lengths[i] = 0;
			}
		}
	}

	build_tree(decoder->offset_tree, MAX_TEMP_CODES * 2, code_lengths, n);

	return 1;
}

// Code table codes can indicate that a sequence of codes should be
// skipped over. The number to skip is Huffman-encoded. Given a skip
// range (0-2), this reads the number of codes to skip over.

static int read_skip_count(LHANewDecoder *decoder, int skiprange)
{
	int result;

	// skiprange=0 => 1 code.

	if (skiprange == 0) {
		result = 1;
	}

	// skiprange=1 => 3-18 codes.

	else if (skiprange == 1) {
		result = read_bits(&decoder->bit_stream_reader, 4);

		if (result < 0) {
			return -1;
		}

		result += 3;
	}

	// skiprange=2 => 20+ codes.

	else {
		result = read_bits(&decoder->bit_stream_reader, 9);

		if (result < 0) {
			return -1;
		}

		result += 20;
	}

	return result;
}

static int read_code_table(LHANewDecoder *decoder)
{
	int i, j, n, skip_count, code;
	uint8_t code_lengths[NUM_CODES];

	// How many codes?

	n = read_bits(&decoder->bit_stream_reader, 9);

	if (n < 0) {
		return 0;
	}

	// n=0 implies a single code of zero length; all inputs
	// decode to the same code.

	if (n == 0) {
		code = read_bits(&decoder->bit_stream_reader, 9);

		if (code < 0) {
			return 0;
		}

		set_tree_single(decoder->code_tree, code);

		return 1;
	}

	if (n > NUM_CODES) {
		n = NUM_CODES;
	}

	// Read the length of each code.
	// The lengths are encoded using the temp-table previously read;
	// offset_tree is reused temporarily to hold it.

	i = 0;

	while (i < n) {
		code = read_from_tree(&decoder->bit_stream_reader,
		                      decoder->offset_tree);

		if (code < 0) {
			return 0;
		}

		// The code that was read can have different meanings.
		// If in the range 0-2, it indicates that a number of
		// codes are unused and should be skipped over.
		// Values greater than two represent a frequency count.

		if (code <= 2) {
			skip_count = read_skip_count(decoder, code);

			if (skip_count < 0) {
				return 0;
			}

			for (j = 0; j < skip_count && i < n; ++j) {
				code_lengths[i] = 0;
				++i;
			}
		} else {
			code_lengths[i] = code - 2;
			++i;
		}
	}

	build_tree(decoder->code_tree, NUM_CODES * 2, code_lengths, n);

	return 1;
}

static int read_offset_table(LHANewDecoder *decoder)
{
	int i, n, len, code;
	uint8_t code_lengths[HISTORY_BITS];

	// How many codes?

	n = read_bits(&decoder->bit_stream_reader, OFFSET_BITS);

	if (n < 0) {
		return 0;
	}

	// n=0 is a special case, meaning only a single code that
	// is of zero length.

	if (n == 0) {
		code = read_bits(&decoder->bit_stream_reader, OFFSET_BITS);

		if (code < 0) {
			return 0;
		}

		set_tree_single(decoder->offset_tree, code);
		return 1;
	}

	// Enforce a hard limit on the number of codes.

	if (n > HISTORY_BITS) {
		n = HISTORY_BITS;
	}

	// Read the length of each code.

	for (i = 0; i < n; ++i) {
		len = read_length_value(decoder);

		if (len < 0) {
			return 0;
		}

		code_lengths[i] = len;
	}

	build_tree(decoder->offset_tree, MAX_TEMP_CODES * 2, code_lengths, n);

	return 1;
}

// Start reading a new block from the input stream.

static int start_new_block(LHANewDecoder *decoder)
{
	int len;

	// Read length of new block (in commands).

	len = read_bits(&decoder->bit_stream_reader, 16);

	if (len < 0) {
		return 0;
	}

	decoder->block_remaining = (size_t) len;

	// Read the temporary decode table, used to encode the codes table.
	// The position table data structure is reused for this.

	if (!read_temp_table(decoder)) {
		return 0;
	}

	// Read the code table; this is encoded *using* the temp table.

	if (!read_code_table(decoder)) {
		return 0;
	}

	// Read the offset table.

	if (!read_offset_table(decoder)) {
		return 0;
	}

	return 1;
}

// Read the next code from the input stream. Returns the code, or -1 if
// an error occurred.

static int read_code(LHANewDecoder *decoder)
{
	return read_from_tree(&decoder->bit_stream_reader, decoder->code_tree);
}

// Read an offset distance from the input stream.
// Returns the code, or -1 if an error occurred.

static int read_offset_code(LHANewDecoder *decoder)
{
	int bits, result;

	bits = read_from_tree(&decoder->bit_stream_reader,
	                      decoder->offset_tree);

	if (bits < 0) {
		return -1;
	}

	// The code read indicates the length of the offset in bits.
	//
	// The returned value looks like this:
	//   bits = 0  ->         0
	//   bits = 1  ->         1
	//   bits = 2  ->        1x
	//   bits = 3  ->       1xx
	//   bits = 4  ->      1xxx
	//             etc.

	if (bits == 0) {
		return 0;
	} else if (bits == 1) {
		return 1;
	} else {
		result = read_bits(&decoder->bit_stream_reader, bits - 1);

		if (result < 0) {
			return -1;
		}

		return result + (1 << (bits - 1));
	}
}

// Add a byte value to the output stream.

static void output_byte(LHANewDecoderPartial *decoder, uint8_t *buf,
                        size_t *buf_len, uint8_t b)
{
	if (buf) buf[*buf_len] = b;
	++*buf_len;

	decoder->ringbuf[decoder->ringbuf_pos] = b;
	decoder->ringbuf_pos = (decoder->ringbuf_pos + 1) % RING_BUFFER_SIZE;
}

// Copy a block from the history buffer.

static void set_copy_from_history(LHANewDecoderPartial *decoder, size_t count)
{
	int offset;

	offset = read_offset_code(&decoder->decoder);

	if (offset < 0) {
		return;
	}

	decoder->ringbuf_copy_pos = decoder->ringbuf_pos + RING_BUFFER_SIZE - (unsigned int) offset - 1;
	while (decoder->ringbuf_copy_pos < 0)
		decoder->ringbuf_copy_pos += RING_BUFFER_SIZE;
	while (decoder->ringbuf_copy_pos >= RING_BUFFER_SIZE)
		decoder->ringbuf_copy_pos -= RING_BUFFER_SIZE;

	decoder->ringbuf_copy_count = count;
}

static size_t lha_lh_new_read_partial(LHANewDecoderPartial *decoder, uint8_t *buf, int sz)
{
	size_t result = 0;
	int code;

	while (sz > 0) {
		if (decoder->ringbuf_copy_count > 0) {
			// Calculate number of bytes that we can copy in sequence without reaching the end of a buffer
			int wn = sz < decoder->ringbuf_copy_count ? sz : decoder->ringbuf_copy_count;
			wn = wn < RING_BUFFER_SIZE - decoder->ringbuf_copy_pos ? wn : RING_BUFFER_SIZE - decoder->ringbuf_copy_pos;
			wn = wn < RING_BUFFER_SIZE - decoder->ringbuf_pos      ? wn : RING_BUFFER_SIZE - decoder->ringbuf_pos;

			if (!buf) {
				// If buf is NULL, we're just skipping data
				decoder->ringbuf_pos += wn;
				decoder->ringbuf_copy_count -= wn;
				decoder->ringbuf_copy_pos += wn;
				sz -= wn;
				result += wn;
				decoder->ringbuf_copy_pos %= RING_BUFFER_SIZE;
				decoder->ringbuf_pos %= RING_BUFFER_SIZE;
				continue;
			}

			// Check if there's an overlap in the ring buffer between read and write pos, in which
			// case we need to copy byte by byte.
			if (decoder->ringbuf_pos < decoder->ringbuf_copy_pos || 
			    decoder->ringbuf_pos > decoder->ringbuf_copy_pos+7) {
				while (wn >= 8) {
					// Copy 8 bytes at at time, using a unaligned memory access (LDL/LDR/SDL/SDR)
					typedef uint64_t u_uint64_t __attribute__((aligned(1)));
					uint64_t value = *(u_uint64_t*)&decoder->ringbuf[decoder->ringbuf_copy_pos];
					*(u_uint64_t*)&buf[result] = value;
					*(u_uint64_t*)&decoder->ringbuf[decoder->ringbuf_pos] = value;

					decoder->ringbuf_copy_pos += 8;
					decoder->ringbuf_pos += 8;
					decoder->ringbuf_copy_count -= 8;
					result += 8;
					sz -= 8;
					wn -= 8;
				}
			}

			// Finish copying the remaining bytes
			while (wn > 0) {
				uint8_t value = decoder->ringbuf[decoder->ringbuf_copy_pos];
				buf[result] = value;
				decoder->ringbuf[decoder->ringbuf_pos] = value;

				decoder->ringbuf_copy_pos += 1;
				decoder->ringbuf_pos += 1;
				decoder->ringbuf_copy_count -= 1;
				result += 1;
				sz -= 1;
				wn -= 1;
			}
			decoder->ringbuf_copy_pos %= RING_BUFFER_SIZE;
			decoder->ringbuf_pos %= RING_BUFFER_SIZE;
			continue;
		}


		// Start of new block?
		while (decoder->decoder.block_remaining == 0) {
			if (!start_new_block(&decoder->decoder)) {
				return 0;
			}
		}

		--decoder->decoder.block_remaining;

		// Read next command from input stream.

		code = read_code(&decoder->decoder);

		if (code < 0) {
			return 0;
		}

		// The code may be either a literal byte value or a copy command.

		if (code < 256) {
			output_byte(decoder, buf, &result, (uint8_t) code);
			sz--;
		} else {
			set_copy_from_history(decoder, code - 256 + COPY_THRESHOLD);
		}
	}

	decoder->decoded_bytes += result;
	return result;
}

static size_t lha_lh_new_read_full(LHANewDecoder *decoder, uint8_t *buf, int sz)
{
	uint8_t *buf_orig = buf;
	int code;

	while (sz > 0) {
		// Start of new block?
		while (decoder->block_remaining == 0) {
			if (!start_new_block(decoder)) {
				return 0;
			}
		}
		--decoder->block_remaining;

		// Read next command from input stream.
		code = read_code(decoder);

		if (code < 0) {
			return 0;
		}

		// The code may be either a literal byte value or a copy command.
		if (code < 256) {
			*buf++ = (uint8_t) code;
			sz--;
		} else {
			int count = code - 256 + COPY_THRESHOLD;
			int offset = read_offset_code(decoder);

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

	return buf - buf_orig;
}


/*************************************************
 * Libdragon API
 *************************************************/

_Static_assert(sizeof(LHANewDecoderPartial) == DECOMPRESS_LZ5H_STATE_SIZE, "LZH5 state size is wrong");

void decompress_lz5h_init(void *state, FILE *fp)
{
	LHANewDecoderPartial *decoder = (LHANewDecoderPartial *)state;
	lha_lh_new_init_partial(decoder, fp);
}

size_t decompress_lz5h_read(void *state, void *buf, size_t len)
{
	LHANewDecoderPartial *decoder = (LHANewDecoderPartial *)state;
	return lha_lh_new_read_partial(decoder, buf, len);
}

int decompress_lz5h_pos(void *state) {
	LHANewDecoderPartial *decoder = (LHANewDecoderPartial *)state;
	return decoder->decoded_bytes;
}

size_t decompress_lz5h_full(FILE *fp, void *buf, size_t len)
{
	LHANewDecoder decoder;
	lha_lh_new_init(&decoder, fp);
	return lha_lh_new_read_full(&decoder, buf, len);
}
