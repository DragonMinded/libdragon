#!/bin/python3
# This script is kept as documentation, it is not used by the code.
# It shows how the VLC tables are generated for AC coefficient decoding.
# Original pl_mpeg decodes huffman codes via bit-tables, which is hugely
# inefficient. AC decoding is by far the biggest CPU hog in our MPEG1
# implementation (after much of the rest was moved to RSP), so we optimize
# it by switching that specific table (PLM_VIDEO_DCT_COEFF) into byte tables
# for faster decoding. This script generates the byte tables.

PLM_VIDEO_DCT_COEFF = [
	[  1 << 1,        0], [       0,   0x0001],  # 0: x
	[  2 << 1,        0], [  3 << 1,        0],  # 1: 0x
	[  4 << 1,        0], [  5 << 1,        0],  # 2: 00x
	[  6 << 1,        0], [       0,   0x0101],  # 3: 01x
	[  7 << 1,        0], [  8 << 1,        0],  # 4: 000x
	[  9 << 1,        0], [ 10 << 1,        0],  # 5: 001x
	[       0,   0x0002], [       0,   0x0201],  # 6: 010x
	[ 11 << 1,        0], [ 12 << 1,        0],  # 7: 0000x
	[ 13 << 1,        0], [ 14 << 1,        0],  # 8: 0001x
	[ 15 << 1,        0], [       0,   0x0003],  # 9: 0010x
	[       0,   0x0401], [       0,   0x0301],  #10: 0011x
	[ 16 << 1,        0], [       0,   0xffff],  #11: 0000 0x
	[ 17 << 1,        0], [ 18 << 1,        0],  #12: 0000 1x
	[       0,   0x0701], [       0,   0x0601],  #13: 0001 0x
	[       0,   0x0102], [       0,   0x0501],  #14: 0001 1x
	[ 19 << 1,        0], [ 20 << 1,        0],  #15: 0010 0x
	[ 21 << 1,        0], [ 22 << 1,        0],  #16: 0000 00x
	[       0,   0x0202], [       0,   0x0901],  #17: 0000 10x
	[       0,   0x0004], [       0,   0x0801],  #18: 0000 11x
	[ 23 << 1,        0], [ 24 << 1,        0],  #19: 0010 00x
	[ 25 << 1,        0], [ 26 << 1,        0],  #20: 0010 01x
	[ 27 << 1,        0], [ 28 << 1,        0],  #21: 0000 000x
	[ 29 << 1,        0], [ 30 << 1,        0],  #22: 0000 001x
	[       0,   0x0d01], [       0,   0x0006],  #23: 0010 000x
	[       0,   0x0c01], [       0,   0x0b01],  #24: 0010 001x
	[       0,   0x0302], [       0,   0x0103],  #25: 0010 010x
	[       0,   0x0005], [       0,   0x0a01],  #26: 0010 011x
	[ 31 << 1,        0], [ 32 << 1,        0],  #27: 0000 0000x
	[ 33 << 1,        0], [ 34 << 1,        0],  #28: 0000 0001x
	[ 35 << 1,        0], [ 36 << 1,        0],  #29: 0000 0010x
	[ 37 << 1,        0], [ 38 << 1,        0],  #30: 0000 0011x
	[ 39 << 1,        0], [ 40 << 1,        0],  #31: 0000 0000 0x
	[ 41 << 1,        0], [ 42 << 1,        0],  #32: 0000 0000 1x
	[ 43 << 1,        0], [ 44 << 1,        0],  #33: 0000 0001 0x
	[ 45 << 1,        0], [ 46 << 1,        0],  #34: 0000 0001 1x
	[       0,   0x1001], [       0,   0x0502],  #35: 0000 0010 0x
	[       0,   0x0007], [       0,   0x0203],  #36: 0000 0010 1x
	[       0,   0x0104], [       0,   0x0f01],  #37: 0000 0011 0x
	[       0,   0x0e01], [       0,   0x0402],  #38: 0000 0011 1x
	[ 47 << 1,        0], [ 48 << 1,        0],  #39: 0000 0000 00x
	[ 49 << 1,        0], [ 50 << 1,        0],  #40: 0000 0000 01x
	[ 51 << 1,        0], [ 52 << 1,        0],  #41: 0000 0000 10x
	[ 53 << 1,        0], [ 54 << 1,        0],  #42: 0000 0000 11x
	[ 55 << 1,        0], [ 56 << 1,        0],  #43: 0000 0001 00x
	[ 57 << 1,        0], [ 58 << 1,        0],  #44: 0000 0001 01x
	[ 59 << 1,        0], [ 60 << 1,        0],  #45: 0000 0001 10x
	[ 61 << 1,        0], [ 62 << 1,        0],  #46: 0000 0001 11x
	[      -1,        0], [ 63 << 1,        0],  #47: 0000 0000 000x
	[ 64 << 1,        0], [ 65 << 1,        0],  #48: 0000 0000 001x
	[ 66 << 1,        0], [ 67 << 1,        0],  #49: 0000 0000 010x
	[ 68 << 1,        0], [ 69 << 1,        0],  #50: 0000 0000 011x
	[ 70 << 1,        0], [ 71 << 1,        0],  #51: 0000 0000 100x
	[ 72 << 1,        0], [ 73 << 1,        0],  #52: 0000 0000 101x
	[ 74 << 1,        0], [ 75 << 1,        0],  #53: 0000 0000 110x
	[ 76 << 1,        0], [ 77 << 1,        0],  #54: 0000 0000 111x
	[       0,   0x000b], [       0,   0x0802],  #55: 0000 0001 000x
	[       0,   0x0403], [       0,   0x000a],  #56: 0000 0001 001x
	[       0,   0x0204], [       0,   0x0702],  #57: 0000 0001 010x
	[       0,   0x1501], [       0,   0x1401],  #58: 0000 0001 011x
	[       0,   0x0009], [       0,   0x1301],  #59: 0000 0001 100x
	[       0,   0x1201], [       0,   0x0105],  #60: 0000 0001 101x
	[       0,   0x0303], [       0,   0x0008],  #61: 0000 0001 110x
	[       0,   0x0602], [       0,   0x1101],  #62: 0000 0001 111x
	[ 78 << 1,        0], [ 79 << 1,        0],  #63: 0000 0000 0001x
	[ 80 << 1,        0], [ 81 << 1,        0],  #64: 0000 0000 0010x
	[ 82 << 1,        0], [ 83 << 1,        0],  #65: 0000 0000 0011x
	[ 84 << 1,        0], [ 85 << 1,        0],  #66: 0000 0000 0100x
	[ 86 << 1,        0], [ 87 << 1,        0],  #67: 0000 0000 0101x
	[ 88 << 1,        0], [ 89 << 1,        0],  #68: 0000 0000 0110x
	[ 90 << 1,        0], [ 91 << 1,        0],  #69: 0000 0000 0111x
	[       0,   0x0a02], [       0,   0x0902],  #70: 0000 0000 1000x
	[       0,   0x0503], [       0,   0x0304],  #71: 0000 0000 1001x
	[       0,   0x0205], [       0,   0x0107],  #72: 0000 0000 1010x
	[       0,   0x0106], [       0,   0x000f],  #73: 0000 0000 1011x
	[       0,   0x000e], [       0,   0x000d],  #74: 0000 0000 1100x
	[       0,   0x000c], [       0,   0x1a01],  #75: 0000 0000 1101x
	[       0,   0x1901], [       0,   0x1801],  #76: 0000 0000 1110x
	[       0,   0x1701], [       0,   0x1601],  #77: 0000 0000 1111x
	[ 92 << 1,        0], [ 93 << 1,        0],  #78: 0000 0000 0001 0x
	[ 94 << 1,        0], [ 95 << 1,        0],  #79: 0000 0000 0001 1x
	[ 96 << 1,        0], [ 97 << 1,        0],  #80: 0000 0000 0010 0x
	[ 98 << 1,        0], [ 99 << 1,        0],  #81: 0000 0000 0010 1x
	[100 << 1,        0], [101 << 1,        0],  #82: 0000 0000 0011 0x
	[102 << 1,        0], [103 << 1,        0],  #83: 0000 0000 0011 1x
	[       0,   0x001f], [       0,   0x001e],  #84: 0000 0000 0100 0x
	[       0,   0x001d], [       0,   0x001c],  #85: 0000 0000 0100 1x
	[       0,   0x001b], [       0,   0x001a],  #86: 0000 0000 0101 0x
	[       0,   0x0019], [       0,   0x0018],  #87: 0000 0000 0101 1x
	[       0,   0x0017], [       0,   0x0016],  #88: 0000 0000 0110 0x
	[       0,   0x0015], [       0,   0x0014],  #89: 0000 0000 0110 1x
	[       0,   0x0013], [       0,   0x0012],  #90: 0000 0000 0111 0x
	[       0,   0x0011], [       0,   0x0010],  #91: 0000 0000 0111 1x
	[104 << 1,        0], [105 << 1,        0],  #92: 0000 0000 0001 00x
	[106 << 1,        0], [107 << 1,        0],  #93: 0000 0000 0001 01x
	[108 << 1,        0], [109 << 1,        0],  #94: 0000 0000 0001 10x
	[110 << 1,        0], [111 << 1,        0],  #95: 0000 0000 0001 11x
	[       0,   0x0028], [       0,   0x0027],  #96: 0000 0000 0010 00x
	[       0,   0x0026], [       0,   0x0025],  #97: 0000 0000 0010 01x
	[       0,   0x0024], [       0,   0x0023],  #98: 0000 0000 0010 10x
	[       0,   0x0022], [       0,   0x0021],  #99: 0000 0000 0010 11x
	[       0,   0x0020], [       0,   0x010e],  #100: 0000 0000 0011 00x
	[       0,   0x010d], [       0,   0x010c],  #101: 0000 0000 0011 01x
	[       0,   0x010b], [       0,   0x010a],  #102: 0000 0000 0011 10x
	[       0,   0x0109], [       0,   0x0108],  #103: 0000 0000 0011 11x
	[       0,   0x0112], [       0,   0x0111],  #104: 0000 0000 0001 000x
	[       0,   0x0110], [       0,   0x010f],  #105: 0000 0000 0001 001x
	[       0,   0x0603], [       0,   0x1002],  #106: 0000 0000 0001 010x
	[       0,   0x0f02], [       0,   0x0e02],  #107: 0000 0000 0001 011x
	[       0,   0x0d02], [       0,   0x0c02],  #108: 0000 0000 0001 100x
	[       0,   0x0b02], [       0,   0x1f01],  #109: 0000 0000 0001 101x
	[       0,   0x1e01], [       0,   0x1d01],  #110: 0000 0000 0001 110x
	[       0,   0x1c01], [       0,   0x1b01],  #111: 0000 0000 0001 111x
]

# Lookup a code of 8 bits into a bit table.
# Return:
#   Positive number: 16-bit value, where the lowest 13 bits are the symbol
#                    found during the lookup and the highest 3 bits are
#                    the number of consumed code bits to find the symbol.
#   Negative number: Index into the table to continue the lookup, when no
#                    symbol was found (and 8 code bits were consumed).
#   None: 		     Invalid sequence of code bits that should not happen in 
#                    well formed files.
def lookup8(table, code, idx=0):
    for i in range(8):
        idx += ((code >> 7) & 1)
        code <<= 1
        if table[idx][0] == -1:
            return None
        if table[idx][0] == 0:
            sym = table[idx][1]
            assert sym & 0xE000 == 0 or sym == 0xFFFF
            return sym | (i << 13)
        idx = table[idx][0]
    return -idx

qtables = []

# Build the quick lookup tables. The tables are stored in qtables, which is
# an array of tables. qtables[0] is the first table, while other tables are
# allocated lazily, as necessary, when code words of more than 8 bits are found.
# Each table is an array of 256 entries, where each entry is either:
#   0: Invalid code
#   >0: 16-bit value, where the lowest 13 bits is the symbol, and the top 3 bits
#       is the number of code bits consumed to find the symbol (minus 1).
#   <0: Index into qtables of the next table to continue lookup.
#
# NOTE: there is a special case for 0xffff (65535). This is the only symbol that
# doesn't fit in 13-bits. In this case, we put 0xffff in the table (positive value),
# and the decoder will have to know that the code word (0b000001) for 0xffff is
# exactly 6 bits long (you can search for it in the above PLM_VIDEO_DCT_COEFF table).
def build_table(table, idx=0):
    tidx = len(qtables)
    qtables.append([None]*256)
    qt = qtables[-1]
    for code in range(256):
        value = lookup8(table, code, idx)
        if value is None:
            qt[code] = 0
        elif value > 0:
            qt[code] = value
        else:
            qt[code] = -build_table(table, -value)
    return tidx

# Build the tables for PLM_VIDEO_DCT_COEFF
build_table(PLM_VIDEO_DCT_COEFF)

# Dump the qtables
#print(len(qtables))
# for qt in qtables:
#     print(qt)

# Perform a fast lookup of a 16-bit code using the fast byte tables.
# Returns tuple (bitlen, symbol), or None if the code is invalid
def fast_lookup(qtables, code):
    assert code < 65536
    code0 = code >> 8
    code1 = code & 0xFF
	# Hack #1: an initial 1 bit is the code for symbol "1", which is encoded
	# with a 50% probability so it has to be extremely common. Handle it with
	# an explicit if check. This also means that we will not need to use
	# half of the first qtable.
    if code0 >= 128:
        return (1, 1)
	# Hack #2: even without looking up the tables, we know that codewords requiring
	# a second table lookup are *exactly* 0x00 .. 0x03. This was probably designed
	# by MPEG so that fast byte lookups could use this shortcut.
    if code0 < 4:
		# Lookup the second byte into one of the other tables
        value = qtables[code0+1][code1]
        n = 8
    else:
		# Lookup the first byte into the first table
        value = qtables[0][code0]
        n = 0
    if value == 0:
        return None # invalid code
	# Hack #3: as explained above, 0xffff is a special case, in which the decoder
	# needs to know the exact length. We know it is 6 bits, so we can return it.
    if value == 65535:
        return (6, 0xffff)
	# Return the code
    return (n+1+(value>>13), value & 0x1fff)

# Perform a slow lookup of a 16-bit code using the slow bit tables
# This is basically a Python version of plm_buffer_read_vlc_bits.
# Returns tuple (bitlen, code), or None for an invalid symbol.
def slow_lookup(table, code):
    assert code < 65536
    idx = 0
    for i in range(16):
        idx += ((code >> 15) & 1)
        code <<= 1
        if table[idx][0] == -1:
            return None
        if table[idx][0] == 0:
            return (i+1, table[idx][1])
        idx = table[idx][0]
    assert 0

# Make sure our fast tables work. Compare slow and fast lookup for all
# 16-bit values.
for code in range(65536):
    sym0 = fast_lookup(qtables, code)
    sym1 = slow_lookup(PLM_VIDEO_DCT_COEFF, code)
    assert sym0 == sym1

print("OK")
