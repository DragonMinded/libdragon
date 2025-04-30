#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#define HUFF_TABLE_SIZE     16
#define HUFF_CONTEXTS       3
#define HUFF_CONTEXT_LEN    (HUFF_CONTEXTS * 24)

// Huffman tree node
typedef struct HuffNode {
    int symbol;
    int frequency;
    struct HuffNode *left, *right;
} HuffNode;

typedef struct {
    int value;
    int length;
} HuffCode;

typedef struct {
    int freq[HUFF_CONTEXTS][HUFF_TABLE_SIZE];
} HuffFreq;

typedef struct {
    HuffNode *root;
    HuffCode codes[HUFF_TABLE_SIZE];
} HuffCtx;

int compare_freq(const void *a, const void *b) {
    HuffNode *node_a = *(HuffNode**)a;
    HuffNode *node_b = *(HuffNode**)b;
    return node_a->frequency - node_b->frequency;
}

HuffNode* build_huffman_tree(int frequencies[], int size) {
    HuffNode* heap[16];
    int hsize = 0;
    for (int i = 0; i < size; i++) {
        if (frequencies[i] == 0) {
            continue;
        }
        heap[hsize] = (HuffNode*)malloc(sizeof(HuffNode));
        heap[hsize]->symbol = i;
        heap[hsize]->frequency = frequencies[i];
        heap[hsize]->left = heap[hsize]->right = NULL;
        hsize++;
    }
    qsort(heap, hsize, sizeof(HuffNode*), compare_freq);

    while (hsize > 1) {
        HuffNode* left = heap[0];
        HuffNode* right = heap[1];
        HuffNode* parent = (HuffNode*)malloc(sizeof(HuffNode));
        parent->symbol = -1;
        parent->frequency = left->frequency + right->frequency;
        parent->left = left;
        parent->right = right;
        memmove(&heap[0], &heap[2], (hsize - 2) * sizeof(HuffNode*));
        heap[hsize - 2] = parent;
        hsize--;
        qsort(heap, hsize, sizeof(HuffNode*), compare_freq);
    }
    return heap[0];
}

void free_huffman_tree(HuffNode* root) {
    if (root->left) free_huffman_tree(root->left);
    if (root->right) free_huffman_tree(root->right);
    free(root);
}

void generate_huffman_codes(HuffNode* root, int code, int length, HuffCode codes[]) {
    if (root->symbol != -1) {
        codes[root->symbol].value = code;
        codes[root->symbol].length = length;
    } else {
        generate_huffman_codes(root->left, (code << 1) | 0, length + 1, codes);
        generate_huffman_codes(root->right, (code << 1) | 1, length + 1, codes);
    }
}

void calculate_frequencies(uint8_t *input_data, int data_len, HuffFreq *freq) {
    assert(data_len % 9 == 0);

    memset(freq, 0, sizeof(HuffFreq));
    while (data_len > 0) {
        for (int i = 0; i < 18; i+=2) {
            int sym0 = input_data[i/2] >> 4;
            int sym1 = input_data[i/2] & 15;
            int c0 = (i+0 < 2) ? i+0 : 2;
            int c1 = (i+1 < 2) ? i+1 : 2;
            freq->freq[c0][sym0]++;
            freq->freq[c1][sym1]++;
        }
        input_data += 9;
        data_len -= 9;
    }

    // Now normalize the frequencies so that they sum to 1
    for (int i = 0; i < HUFF_CONTEXTS; i++) {
        int sum = 0;
        for (int j = 0; j < HUFF_TABLE_SIZE; j++) {
            sum += freq->freq[i][j];
        }
        if (sum == 0) {
            continue;
        }
        for (int j = 0; j < HUFF_TABLE_SIZE; j++) {
            int nfreq = (freq->freq[i][j] * 255 + sum - 1) / sum;
            if (nfreq == 0) assert(freq->freq[i][j] == 0);
            freq->freq[i][j] = nfreq;
        }
    }
}

int huffv_compress(uint8_t *input_data, int data_len, uint8_t *output, int output_len, uint8_t *outctx, int outctxlen) {
    assert(data_len > 0);

    HuffFreq freq;
    calculate_frequencies(input_data, data_len, &freq);

    HuffCtx ctx[HUFF_CONTEXTS] = {{0}};
    for (int i = 0; i < HUFF_CONTEXTS; i++) {
        while (1) {
            // Initialize codes with invalid length so we can detect if we can't encode this context
            for (int j = 0; j < HUFF_TABLE_SIZE; j++) {
                ctx[i].codes[j].length = -1;
            }

            ctx[i].root = build_huffman_tree(freq.freq[i], HUFF_TABLE_SIZE);
            generate_huffman_codes(ctx[i].root, 0, 0, ctx[i].codes);

            // Check if all codes are 8 bits or less, otherwise we can't encode this context
            // We limit the maximum code length to 8 bits so that the decompressor
            // can use a compact table to decode.
            bool valid = true;
            for (int j = 0; j < HUFF_TABLE_SIZE; j++) {
                if (ctx[i].codes[j].length > 8) {
                    valid = false;
                    break;
                }
            }
            if (valid) break;
            free_huffman_tree(ctx[i].root);

            // Scale the frequencies and retry. This is what bzip2 does. It's not
            // the best solution but it's simple and works.
            for (int j = 0; j < HUFF_TABLE_SIZE; j++) {
                if (freq.freq[i][j] > 1)
                    freq.freq[i][j] /= 2;
            }
        }
    }

    int written = 0;

    // Serialize the compression contexts
    for (int i = 0; i < HUFF_CONTEXTS; i++) {
        // Write lengths as 4-bit
        for (int j=0; j<16; j+=2) {
            int l = 0;
            assert(ctx[i].codes[j+0].length < 16);
            assert(ctx[i].codes[j+1].length < 16);
            // Negative lengths (unused symbols) are encoded
            // as 0xF. Notice that 0 length is a valid length
            // (only one symbol in the context).
            int l0 = ctx[i].codes[j+0].length < 0 ? 0xF : ctx[i].codes[j+0].length;
            int l1 = ctx[i].codes[j+1].length < 0 ? 0xF : ctx[i].codes[j+1].length;
            l |= l0 << 4;
            l |= l1 << 0;
            assert(outctxlen-- > 0);
            *outctx++ = l;
        }

        // Write values as 8 bits
        for (int j=0; j<16; j++) {
            assert(ctx[i].codes[j].value < 256);
            assert(outctxlen-- > 0);
            *outctx++ = ctx[i].codes[j].value;
        }
    }
    assert(outctxlen == 0);

    uint32_t buffer = 0;
    int bit_pos = 0;

    assert(data_len % 9 == 0);
    while (data_len > 0) {
        for (int i = 0; i < 18; i += 2) {
            int sym0 = input_data[i/2] >> 4;
            int sym1 = input_data[i/2] & 15;
            int c0 = (i+0 < 2) ? i+0 : 2;
            int c1 = (i+1 < 2) ? i+1 : 2;

            // 0-lengths happen when a context only has a single symbol, so
            // there's no need to encode it.
            assert(ctx[c0].codes[sym0].length >= 0);
            assert(ctx[c1].codes[sym1].length >= 0);

            buffer = (buffer << ctx[c0].codes[sym0].length) | ctx[c0].codes[sym0].value;
            bit_pos += ctx[c0].codes[sym0].length;
            buffer = (buffer << ctx[c1].codes[sym1].length) | ctx[c1].codes[sym1].value;
            bit_pos += ctx[c1].codes[sym1].length;

            while (bit_pos >= 8) {
                assert(written < output_len);
                output[written++] = buffer >> (bit_pos - 8);
                bit_pos -= 8;
            }
        }

        input_data += 9;
        data_len -= 9;
    }

    if (bit_pos > 0) {
        assert(written < output_len);
        assert(bit_pos < 8);
        output[written++] = buffer << (8 - bit_pos);
    }

    for (int i=0; i<HUFF_CONTEXTS; i++) {
        free_huffman_tree(ctx[i].root);
    }

    return written;
}


/*
 * SerializedCtx represents the serialized Huffman context:
 * - The first 8 bytes contain 16 code lengths (2 per byte, 4 bits each),
 * - The following 16 bytes contain the Huffman code values for symbols 0..15.
 */
typedef struct {
    uint8_t lengths[8];  // 2 lengths per byte: high nibble for even symbols, low nibble for odd.
    uint8_t values[16];  // Huffman code values for symbols 0..15.
} HuffSerializedCtx;

/*
 * HuffLookup represents the lookup table for a single context.
 * Each table has 256 entries that map an 8-bit index read from the bit stream
 * to a byte containing:
 *   - high nibble: the decoded symbol (0..15)
 *   - low nibble: the length (in bits) of the Huffman code
 */
typedef struct {
    uint8_t codes[256];
} HuffLookup;

//=====================================================================
// Function: huffv_decompress_init
// Description:
//   Given a buffer (of length HUFF_CONTEXT_LEN) containing 3 serialized
//   contexts (each 24 bytes), this function builds an array of three
//   lookup tables that can be reused across multiple decompression calls.
// Parameters:
//   - ctx_data: pointer to the serialized context data (72 bytes)
//   - ctx_len: length of the context data (must be HUFF_CONTEXT_LEN)
//   - lookup: an array of 3 HuffLookup structures to be filled in
//=====================================================================
void huffv_decompress_init(const uint8_t *ctx_data, int ctx_len, HuffLookup lookup[HUFF_CONTEXTS])
{
    assert(ctx_len == HUFF_CONTEXT_LEN);

    for (int i = 0; i < HUFF_CONTEXTS; i++) {
        // Initialize the lookup table to zero.
        for (int j = 0; j < 256; j++) {
            lookup[i].codes[j] = 0;
        }
        // Interpret the current context (24 bytes) as a SerializedCtx.
        const HuffSerializedCtx *sctx = (const HuffSerializedCtx *)(ctx_data + i * 24);

        // For each symbol (0..15), extract its code length and code value.
        for (int sym = 0; sym < HUFF_TABLE_SIZE; sym++) {
            int len = sctx->lengths[sym/2] >> (4 * (~sym&1)) & 0xF;
            // The value 0xF indicates an unused symbol; skip it.
            if (len == 0xF)
                continue;
            assert(len <= 8);
            // Ensure that the code value fits in the specified number of bits.
            // (i.e., it must be less than 1 << len)
            assert(sctx->values[sym] < (1 << len));

            // Calculate the number of bits to shift so that the code is left-aligned in 8 bits.
            int shift = 8 - len;
            int code = sctx->values[sym] << shift;
            // Prepare the value to store in the lookup table:
            //   - high nibble: the symbol (0..15)
            //   - low nibble: the code length in bits (1..8)
            uint8_t table_value = (sym << 4) | (len & 0xF);

            // Fill every table entry that starts with the computed code.
            // There will be 1 << (8 - len) possible 8-bit values with this prefix.
            for (int k = 0; k < (1 << shift); k++) {
                assert(lookup[i].codes[code + k] == 0);  // ensure each entry is written only once.
                lookup[i].codes[code + k] = table_value;
            }
        }
        // Verify that all 256 entries have been filled.
        for (int j = 0; j < 256; j++) {
            assert(lookup[i].codes[j] != 0);
        }
    }
}

//=====================================================================
// Function: huffv_decompress
// Description:
//   Decompresses data using the provided lookup tables.
//   It reads symbols from the compressed bit stream using the lookup tables,
//   reconstructs the original nibble stream (each symbol is 4 bits), and then
//   combines every two nibbles into one output byte.
//   This function returns the number of bits consumed from the input buffer.
// Parameters:
//   - compressed: pointer to the compressed data buffer (bit stream)
//   - compressed_len: length (in bytes) of the compressed buffer
//   - lookup: an array of 3 prebuilt HuffLookup tables (one per context)
//   - output: pointer to the buffer where decompressed data is written
//   - output_len: expected size (in bytes) of the decompressed data; must be a multiple of 9,
//                 as each original block consists of 9 bytes (18 4-bit symbols)
// Returns:
//   The number of bits used/consumed from the compressed input buffer.
//   This value may be less than compressed_len * 8 if the output buffer is too short
//    to contain all the decompressed data. Also the last byte might always
//    be only partially consumed.
//=====================================================================
int huffv_decompress(uint8_t *compressed, int compressed_len,
                     const HuffLookup lookup[HUFF_CONTEXTS],
                     uint8_t *output, int output_len)
{
    // The output length must be a multiple of 9 bytes (each block decodes into 9 bytes).
    assert(output_len % 9 == 0);
    int num_blocks = output_len / 9;
    int out_index = 0;

    // Bit stream reading state.
    uint32_t bit_buffer = 0;
    int bit_count = 0;
    int comp_index = 0;  // Current index in the compressed input buffer

    // Process each block.
    for (int b = 0; b < num_blocks; b++) {
        uint8_t nibbles[18];  // Temporary array to store 18 decoded 4-bit symbols.

        // Each block consists of 18 symbols:
        // - The first symbol uses context 0,
        // - The second uses context 1,
        // - The remaining 16 symbols use context 2.
        for (int n = 0; n < 18; n++) {
            int context = (n < 2) ? n : 2;

            // Ensure there are at least 8 bits available in the buffer.
            while (bit_count < 8) {
                bit_buffer <<= 8;
                if (comp_index < compressed_len)
                    bit_buffer |= compressed[comp_index++];
                else
                    comp_index++; // just for statistics
                bit_count += 8;
            }
            // Extract the top 8 bits (without removing them yet) to use as index.
            uint8_t index = (bit_buffer >> (bit_count - 8)) & 0xFF;
            // Look up the corresponding symbol and the code length.
            uint8_t code_val = lookup[context].codes[index];
            int symbol = code_val >> 4;
            int code_len = code_val & 0x0F;
            assert(code_len <= 8);
            nibbles[n] = (uint8_t)symbol;
            // Remove the used bits from the bit buffer.
            bit_count -= code_len;
        }
        // Combine every two nibbles into one byte (the first nibble is the high half,
        // the second nibble is the low half) to reconstruct the original 9 bytes.
        for (int i = 0; i < 9; i++) {
            uint8_t byte = (nibbles[2 * i] << 4) | (nibbles[2 * i + 1] & 0x0F);
            output[out_index++] = byte;
        }
    }
    // Calculate the total number of bits consumed.
    // This is computed as the total number of bits read from the input buffer:
    // (comp_index * 8) minus the number of bits remaining in bit_buffer.
    int bits_consumed = comp_index * 8 - bit_count;
    return bits_consumed;
}
