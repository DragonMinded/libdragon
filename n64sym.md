# N64 SYMT Format v3

This document describes the version 3 of the Symbol Table format (SYMT) used by Libdragon for runtime symbolization and backtracing.

The format is designed to be extremely compact in ROM while requiring a minimal fixed amount of RAM (e.g. 512-1024 bytes) to be queried at runtime. It achieves this by using a combination of delta encoding, variable-length integers (VarInt), and dictionary-based compression with Front Coding and Canonical Huffman Coding.

## File Structure

The file starts with a global header followed by several variable-length sections. All offsets are relative to the beginning of the file. Everything is assumed to be big-endian unless specified.

### Header

```c
typedef struct {
    char magic[4];           // "SYMT"
    uint32_t version;        // 3
    uint32_t num_symbols;    // Total number of symbols
    uint32_t num_chunks;     // Total number of compressed symbol chunks

    // Offsets to sections
    uint32_t chunk_idx_off;  // Chunk Index
    uint32_t file_tab_off;   // File Block Table
    uint32_t func_tab_off;   // Func Block Table
    uint32_t huff_tab_off;   // Global Huffman Table
    uint32_t file_blob_off;  // File String Blob
    uint32_t func_blob_off;  // Func String Blob
    uint32_t stream_off;     // Compressed Symbol Stream
    
    // Size of sections (useful for bounds checking)
    uint32_t num_files;      // Number of file blocks
    uint32_t num_funcs;      // Number of func blocks
    uint32_t huff_tab_size;  // Size of Huffman table in bytes
    uint32_t file_blob_size; // Size of file blob in bytes
    uint32_t func_blob_size; // Size of func blob in bytes
    uint32_t stream_size;    // Size of compressed stream in bytes
} symtable_header_t;
```

---

## Sections

### 1. Chunk Index (`chunk_idx_off`)

A sparse index used to perform binary search on the symbols, using their address. This is the main entrypoint to lookup a symbol given its address. Symbols are divided into "Chunks" which are individually compressed, so the Chunk Index actually identifies the chunk in which a symbol is encoded.

The structure is an array of `num_chunks` entries:

```c
typedef struct {
    uint32_t start_addr;    // The memory address of the first symbol in this chunk
    uint32_t stream_off;    // The byte offset of this chunk within the Symbol Stream section
} chunk_index_entry_t;
```

Since the index is sorted by memory address, it can be queried via bisection, to identify the chunk in which the symbol is contained. Then the chunk must be parsed and decompressed linearly until the symbol is found.

### 2. String Indexes (`file_tab_off`, `func_tab_off`)

The SYMT file stores two groups of strings: file names and function names. Within each symbol, the filename and the function name are referenced by an integer ID.

The string indexes are sparse indexes used to locate strings in the compressed string blobs. Strings are organized in compressed blocks of variable size. To find a string with ID `N`, we perform a binary search on this index to find the block containing `N`.

The structure is an array of `num_files` (or `num_funcs`) entries:

```c
typedef struct {
    uint32_t start_idx;     // The index of the first string in this block
    uint32_t blob_off;      // The byte offset of this block within the respective Blob section
} string_index_entry_t;
```

### 3. Global Huffman Table (`huff_tab_off`)

Contains the shared Canonical Huffman Table used to decompress string suffixes in both the File and Func blobs. The table allows decoding individual characters from the compressed bitstream.

**Format:**
*   `MaxLen` (1 byte): Maximum codeword length.
*   `Padding` (1 byte): Zeros.
*   `LUT` (128 bytes): 64-entry Lookup Table for fast decoding of codes <= 6 bits. Each entry is `Symbol` (1 byte) + `Len` (1 byte).
*   `FirstCode` (array of uint16): First canonical code for each length (7..MaxLen).
*   `FirstSymbol` (array of uint16): Index of the first symbol for each length (7..MaxLen), relative to the start of the symbols array.
*   `Symbols` (array of uint8): The alphabet symbols with code length > 6, sorted by code length then lexicographically. Codes with length <= 6 are fully covered by the LUT and are not stored here.

**Notes:**
* Arrays `FirstCode` and `FirstSymbol` only exist if `MaxLen >= 7`. The size of these arrays is `MaxLen - 6`.
* The `num_symbols` canonical array is omitted. The decoder derives the number of symbols for each length from `FirstSymbol` (or equivalently from successive `FirstCode` values) and from the total size of the `Symbols` array.
* The total size of the Huffman table is stored in the header field `huff_tab_size` and is used at runtime to know how many bytes of the trailing `Symbols` array are present after the canonical arrays.

### 4. String Blobs (`file_blob_off`, `func_blob_off`)

Contains the actual string data, compressed using **Front Coding** combined with **Huffman Coding**. The data is divided into blocks pointed to by the String Index. Each block is independently compressed and fits into a fixed-size runtime buffer (e.g. 512 bytes).

**Block Format:**
Strings in the block are stored as delta from the previous string. The common prefix length is stored as a **delta** relative to the previous string's common prefix length, encoded using **Signed Exp-Golomb (k=0)**. The suffix itself is compressed using the Global Huffman Table, including the null terminator (`\0`).

```
[PrefixLenDelta] [HuffmanEncodedSuffix...]
```

* `PrefixLenDelta` (Exp-Golomb): The difference between the current string's common prefix length and the *previous* string's common prefix length. 
    *   `Delta = CurrentCommon - PrevCommon`.
    *   For the first string in a block, `PrevCommon` is 0.
    *   The delta is mapped to an unsigned integer using ZigZag encoding (`0->0`, `-1->1`, `1->2`, `-2->3`...) and then written using Exp-Golomb k=0 codes (interleaved in the bitstream).
* `HuffmanEncodedSuffix`: The suffix characters encoded using the global Huffman table. This is a bitstream. The stream continues until the Huffman code for `\0` is encountered.

**Note:** Blocks are padded to 2-byte alignment at the end.

### 5. Compressed Symbol Stream (`stream_off`)

Contains the symbol data. This is a continuous stream of bytes divided into Chunks (as defined in the Chunk Index). Each Chunk can be decompressed independently.

Inside a Chunk, symbols are stored sequentially using **Delta Encoding** relative to the previous symbol's state. The state consists of:
* `CurrentAddress` (Initialized to `chunk.start_addr`)
* `FileID` (Initialized to 0)
* `FuncID` (Initialized to 0)
* `Line` (Initialized to 0)

**Symbol Encoding:**
Each symbol starts with a 1-byte **Opcode**:

| Bit | Name | Description |
| :--- | :--- | :--- |
| 7 | `New File` | If 1, a signed VarInt follows specifying the delta for `FileID`. |
| 6 | `New Func` | If 1, a signed VarInt follows specifying the delta for `FuncID`. |
| 5 | `New Line` | If 1, a signed VarInt follows specifying the delta for `Line`. |
| 4 | `Is Func` | If 1, this symbol marks the start of a function. |
| 3 | `Is Inline` | If 1, this symbol is an inlined function instance. |
| 2-0 | `Addr Delta` | Address increment. <br> `0`: Delta = 0 (same address). <br> `1..6`: Delta = `Value * 4` bytes. <br> `7`: Larger Delta. Unsigned VarInt follows. Then Delta = `(Value + 7) * 4`. |

The opcode value `0x00` is used to signal the end of the chunk.

---

## Data Types

### VarInt (Unsigned)
Variable-length integer encoding (LEB128-style).
*   Read byte.
*   Take low 7 bits.
*   If high bit (0x80) is set, shift and read next byte.

**Examples:**
*   `127` (0x7F) -> `0x7F`
*   `128` (0x80) -> `0x80 0x01`
*   `300` (0x12C) -> `0xAC 0x02`

### Signed VarInt (ZigZag)
Maps signed integers to unsigned values to efficiently encode small negative numbers:
*   `0` -> `0`
*   `-1` -> `1`
*   `1` -> `2`
*   `-2` -> `3`
*   ...
*   Encoded/Decoded as standard VarInt.

**Examples:**
*   `-1` -> `1` -> `0x01`
*   `1` -> `2` -> `0x02`
*   `-64` -> `127` -> `0x7F`
*   `64` -> `128` -> `0x80 0x01`
*   `-300` -> `599` -> `0xD7 0x04`
*   `300` -> `600` -> `0xD8 0x04`

---

## Runtime Algorithms

These algorithms are designed to work with a small fixed-size buffer (e.g., 1KB) without dynamic allocation.

### Algorithm 1: Symbol Lookup

**Goal:** Find the symbol containing address `SearchAddr`.

1.  **Binary Search Chunk Index:**
    *   Load `chunk_index_t` entries from ROM.
    *   Find the chunk `C` such that `C.start_addr <= SearchAddr < (C+1).start_addr`.
2.  **Load Chunk:**
    *   DMA the chunk data from `stream_off + C.stream_off` into the RAM buffer.
3.  **Scan Stream:**
    *   Initialize state: `CurrAddr = C.start_addr`, `File=0`, `Func=0`, `Line=0`.
    *   Loop through opcodes in the buffer:
        *   Decode Address Delta. Update `CurrAddr`.
        *   If `CurrAddr > SearchAddr`: Stop. The *previous* valid symbol is the match.
        *   Update `File`, `Func`, `Line` based on flags and VarInts.
        *   Keep track of the "Best Match" seen so far.

### Algorithm 2: String Fetch

**Goal:** Retrieve string with ID `TargetID` from a Blob.

1.  **Binary Search String Index:**
    *   Load `string_index_entry_t` entries from ROM.
    *   Find the entry `E` such that `E.start_idx <= TargetID < (E+1).start_idx`.
2.  **Load Huffman Table:**
    *   (Optionally cached or loaded once) Load global Huffman table from `huff_tab_off`.
3.  **Load Block:**
    *   DMA the block from `BlobOffset + E.blob_off` into the RAM buffer.
4.  **Front Coding + Huffman Decode:**
    *   Initialize `CurrentString` (empty).
    *   Initialize `PrevCommon` = 0.
    *   Calculate `TargetInBlock = TargetID - E.start_idx`.
    *   Initialize BitReader on the data.
    *   Loop `i` from 0 to `TargetInBlock`:
        *   Read `PrefixLenDelta` (Exp-Golomb, ZigZag).
        *   `CurrentCommon = PrevCommon + PrefixLenDelta`.
        *   `PrevCommon = CurrentCommon`.
        *   Truncate `CurrentString` to `CurrentCommon`.
        *   **Decode Suffix:**
            *   Loop until `\0` decoded:
                *   Read next Huffman symbol using Table/LUT.
                *   If not `\0`, append char to `CurrentString`.
    *   `CurrentString` is now the result.

### Algorithm 3: Backtrace Symbolization

1.  Call **Symbol Lookup** with the PC address.
    *   Result: `SymbolAddr`, `FileID`, `FuncID`, `Line`, `Offset`.
2.  If `FuncID` is valid:
    *   Call **String Fetch** with `FuncID` to get the function name.
3.  If `FileID` is valid:
    *   Call **String Fetch** with `FileID` to get the filename.
