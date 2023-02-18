// Decoder for algorithm -lh5- of the LZH family.
// This code comes from https://github.com/fragglet/lhasa
// and has been turned into a single file header with only
// the -lh5- algo.
// This was also modified to allow for full streaming decompression
// (up to 1 byte at a time). Before, the code would decompress one
// internal LHA block at a time, writing a non predictable number of 
// bytes in the output buffer.
// This file is ISC Licensed.

#ifndef LZH5_H
#define LZH5_H

//////////////////////// public/lha_decoder.h

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

#ifndef LHASA_PUBLIC_LHA_DECODER_H
#define LHASA_PUBLIC_LHA_DECODER_H

#include <stdlib.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file lha_decoder.h
 *
 * @brief Raw LHA data decoder.
 *
 * This file defines the interface to the decompression code, which can
 * be used to decompress the raw compressed data from an LZH file.
 *
 * Implementations of the various compression algorithms used in LZH
 * archives are provided - these are represented by the
 * @ref LHADecoderType structure, and can be retrieved using the
 * @ref lha_decoder_for_name function. One of these can then be passed to
 * the @ref lha_decoder_new function to create a @ref LHADecoder structure
 * and decompress the data.
 */

/**
 * Opaque type representing a type of decoder.
 *
 * This is an implementation of the decompression code for one of the
 * algorithms used in LZH archive files. Pointers to these structures are
 * retrieved by using the @ref lha_decoder_for_name function.
 */

typedef struct _LHADecoderType LHADecoderType;

/**
 * Opaque type representing an instance of a decoder.
 *
 * This is a decoder structure being used to decompress a stream of
 * compressed data. Instantiated using the @ref lha_decoder_new
 * function and freed using the @ref lha_decoder_free function.
 */

typedef struct _LHADecoder LHADecoder;

/**
 * Callback function invoked when a decoder wants to read more compressed
 * data.
 *
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 * @param user_data  Extra pointer to pass to the decoder.
 * @return           Number of bytes read.
 */

typedef size_t (*LHADecoderCallback)(void *buf, size_t buf_len,
                                     void *user_data);

/**
 * Callback function used for monitoring decode progress.
 * The callback is invoked for every block processed (block size depends on
 * decode algorithm).
 *
 * @param num_blocks      Number of blocks processed so far.
 * @param total_blocks    Total number of blocks to process.
 * @param callback_data  Extra user-specified data passed to the callback.
 */

typedef void (*LHADecoderProgressCallback)(unsigned int num_blocks,
                                           unsigned int total_blocks,
                                           void *callback_data);

/**
 * Get the decoder type for the specified name.
 *
 * @param name           String identifying the decoder type, for
 *                       example, "-lh1-".
 * @return               Pointer to the decoder type, or NULL if there
 *                       is no decoder type for the specified name.
 */

LHADecoderType *lha_decoder_for_name(char *name);

/**
 * Allocate a new decoder for the specified type.
 *
 * @param dtype          The decoder type.
 * @param callback       Callback function for the decoder to call to read
 *                       more compressed data.
 * @param callback_data  Extra data to pass to the callback function.
 * @param stream_length  Length of the uncompressed data, in bytes. When
 *                       this point is reached, decompression will stop.
 * @return               Pointer to the new decoder, or NULL for failure.
 */

LHADecoder *lha_decoder_new(LHADecoderType *dtype,
                            LHADecoderCallback callback,
                            void *callback_data,
                            size_t stream_length);

/**
 * Free a decoder.
 *
 * @param decoder        The decoder to free.
 */

void lha_decoder_free(LHADecoder *decoder);

/**
 * Set a callback function to monitor decode progress.
 *
 * @param decoder        The decoder.
 * @param callback       Callback function to monitor decode progress.
 * @param callback_data  Extra data to pass to the decoder.
 */

void lha_decoder_monitor(LHADecoder *decoder,
                         LHADecoderProgressCallback callback,
                         void *callback_data);

/**
 * Decode (decompress) more data.
 *
 * @param decoder        The decoder.
 * @param buf            Pointer to buffer to store decompressed data.
 * @param buf_len        Size of the buffer, in bytes.
 * @return               Number of bytes decompressed.
 */

size_t lha_decoder_read(LHADecoder *decoder, uint8_t *buf, size_t buf_len);

/**
 * Get the current 16-bit CRC of the decompressed data.
 *
 * This should be called at the end of decompression to check that the
 * data was extracted correctly, and the value compared against the CRC
 * from the file header.
 *
 * @param decoder        The decoder.
 * @return               16-bit CRC of the data decoded so far.
 */

uint16_t lha_decoder_get_crc(LHADecoder *decoder);

/**
 * Get the count of the number of bytes decoded.
 *
 * This should be called at the end of decompression, and the value
 * compared against the file length from the file header.
 *
 * @param decoder        The decoder.
 * @return               The number of decoded bytes.
 */

size_t lha_decoder_get_length(LHADecoder *decoder);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef LHASA_LHA_DECODER_H */



//////////////////////// lha_decoder.h

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

#ifndef LHASA_LHA_DECODER_H
#define LHASA_LHA_DECODER_H

struct _LHADecoderType {

	/**
	 * Callback function to initialize the decoder.
	 *
	 * @param extra_data     Pointer to the extra data area allocated for
	 *                       the decoder.
	 * @param callback       Callback function to invoke to read more
	 *                       compressed data.
	 * @param callback_data  Extra pointer to pass to the callback.
	 * @return               Non-zero for success.
	 */

	int (*init)(void *extra_data,
	            LHADecoderCallback callback,
	            void *callback_data);

	/**
	 * Callback function to free the decoder.
	 *
	 * @param extra_data     Pointer to the extra data area allocated for
	 *                       the decoder.
	 */

	void (*free)(void *extra_data);

	/**
	 * Callback function to read (ie. decompress) data from the
	 * decoder.
	 *
	 * @param extra_data     Pointer to the decoder's custom data.
	 * @param buf            Pointer to the buffer in which to store
	 *                       the decompressed data.  The buffer is
	 *                       at least 'max_read' bytes in size.
	 * @return               Number of bytes decompressed.
	 */

	size_t (*read)(void *extra_data, uint8_t *buf);

	/** Number of bytes of extra data to allocate for the decoder. */

	size_t extra_size;

	/** Maximum number of bytes that might be put into the buffer by
	    a single call to read() */

	size_t max_read;

	/** Block size. Used for calculating number of blocks for
	    progress bar. */

	size_t block_size;
};

struct _LHADecoder {

	/** Type of decoder (algorithm) */

	LHADecoderType *dtype;

	/** Callback function to monitor decoder progress. */

	LHADecoderProgressCallback progress_callback;
	void *progress_callback_data;

	/** Last announced block position, for progress callback. */

	unsigned int last_block, total_blocks;

	/** Current position in the decode stream, and total length. */

	size_t stream_pos, stream_length;

	/** Output buffer, containing decoded data not yet returned. */

	unsigned int outbuf_pos, outbuf_len;
	uint8_t *outbuf;

	/** If true, the decoder read() function returned zero. */

	unsigned int decoder_failed;

	/** Current CRC of the output stream. */

	uint16_t crc;
};

#endif /* #ifndef LHASA_LHA_DECODER_H */


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

	// Callback function to invoke to read more data from the
	// input stream.

	LHADecoderCallback callback;
	void *callback_data;

	// Bits from the input stream that are waiting to be read.

	uint32_t bit_buffer;
	unsigned int bits;

} BitStreamReader;

// Initialize bit stream reader structure.

static void bit_stream_reader_init(BitStreamReader *reader,
                                   LHADecoderCallback callback,
                                   void *callback_data)
{
	reader->callback = callback;
	reader->callback_data = callback_data;

	reader->bits = 0;
	reader->bit_buffer = 0;
}

// Return the next n bits waiting to be read from the input stream,
// without removing any.  Returns -1 for failure.

static int peek_bits(BitStreamReader *reader,
                     unsigned int n)
{
	uint8_t buf[4];
	unsigned int fill_bytes;
	size_t bytes;

	if (n == 0) {
		return 0;
	}

	// If there are not enough bits in the buffer to satisfy this
	// request, we need to fill up the buffer with more bits.

	while (reader->bits < n) {

		// Maximum number of bytes we can fill?

		fill_bytes = (32 - reader->bits) / 8;

		// Read from input and fill bit_buffer.

		memset(buf, 0, sizeof(buf));
		bytes = reader->callback(buf, fill_bytes,
		                         reader->callback_data);

		// End of file?

		if (bytes == 0) {
			return -1;
		}

		reader->bit_buffer |= (uint32_t) buf[0] << (24 - reader->bits);
		reader->bit_buffer |= (uint32_t) buf[1] << (16 - reader->bits);
		reader->bit_buffer |= (uint32_t) buf[2] << (8 - reader->bits);
		reader->bit_buffer |= (uint32_t) buf[3];

		reader->bits += bytes * 8;
	}

	return (signed int) (reader->bit_buffer >> (32 - n));
}

// Read a bit from the input stream.
// Returns -1 for failure.

static int read_bits(BitStreamReader *reader,
                     unsigned int n)
{
	int result;

	result = peek_bits(reader, n);

	if (result >= 0) {
		reader->bit_buffer <<= n;
		reader->bits -= n;
	}

	return result;
}


// Read a bit from the input stream.
// Returns -1 for failure.

static int read_bit(BitStreamReader *reader)
{
	return read_bits(reader, 1);
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

	// Start from root.

	code = tree[0];

	while ((code & TREE_NODE_LEAF) == 0) {

		bit = read_bit(reader);

		if (bit < 0) {
			return -1;
		}

		code = tree[code + (unsigned int) bit];
	}

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

	// Ring buffer of past data.  Used for position-based copies.

	uint8_t ringbuf[RING_BUFFER_SIZE];
	unsigned int ringbuf_pos;
	int ringbuf_copy_pos;
	int ringbuf_copy_count;

	// Number of commands remaining before we start a new block.

	unsigned int block_remaining;

	// Table used for the code tree.

	TreeElement code_tree[NUM_CODES * 2];

	// Table used to encode the offset tree, used to read offsets
	// into the history buffer. This same table is also used to
	// encode the temp-table, which is bigger; hence the size.

	TreeElement offset_tree[MAX_TEMP_CODES * 2];
} LHANewDecoder;

// Initialize the history ring buffer.

static void init_ring_buffer(LHANewDecoder *decoder)
{
	memset(decoder->ringbuf, ' ', RING_BUFFER_SIZE);
	decoder->ringbuf_pos = 0;
	decoder->ringbuf_copy_pos = 0;
	decoder->ringbuf_copy_count = 0;
}

static int __attribute__((unused)) lha_lh_new_init(LHANewDecoder *decoder, LHADecoderCallback callback,
                           void *callback_data)
{
	// Initialize input stream reader.

	bit_stream_reader_init(&decoder->bit_stream_reader,
	                       callback, callback_data);

	// Initialize data structures.

	init_ring_buffer(decoder);

	// First read starts the first block.

	decoder->block_remaining = 0;

	// Initialize tree tables to a known state.

	init_tree(decoder->code_tree, NUM_CODES * 2);
	init_tree(decoder->offset_tree, MAX_TEMP_CODES * 2);

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

static void output_byte(LHANewDecoder *decoder, uint8_t *buf,
                        size_t *buf_len, uint8_t b)
{
	buf[*buf_len] = b;
	++*buf_len;

	decoder->ringbuf[decoder->ringbuf_pos] = b;
	decoder->ringbuf_pos = (decoder->ringbuf_pos + 1) % RING_BUFFER_SIZE;
}

// Copy a block from the history buffer.

static void set_copy_from_history(LHANewDecoder *decoder, uint8_t *buf, size_t count)
{
	int offset;

	offset = read_offset_code(decoder);

	if (offset < 0) {
		return;
	}

	decoder->ringbuf_copy_pos = decoder->ringbuf_pos + RING_BUFFER_SIZE - (unsigned int) offset - 1;
	decoder->ringbuf_copy_count = count;
}

static size_t __attribute__((unused)) lha_lh_new_read(LHANewDecoder *decoder, uint8_t *buf, int sz)
{
	size_t result = 0;
	int code;

	while (sz > 0) {	

		if (decoder->ringbuf_copy_count > 0) {
			output_byte(decoder, buf, &result,
			            decoder->ringbuf[decoder->ringbuf_copy_pos++ % RING_BUFFER_SIZE]);
			decoder->ringbuf_copy_count--;
			sz--;
			continue;
		}


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
			output_byte(decoder, buf, &result, (uint8_t) code);
			sz--;
		} else {
			set_copy_from_history(decoder, buf, code - 256 + COPY_THRESHOLD);
		}
	}

	return result;
}

#endif /* LZH5_H */
