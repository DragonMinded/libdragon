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

    HuffCtx ctx[HUFF_CONTEXTS] = {0};
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
