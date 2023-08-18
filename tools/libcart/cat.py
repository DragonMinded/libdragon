#!/usr/bin/env python3

import sys

_ULTRA64 = False
src = sys.argv[1].endswith(".c")
with open(sys.argv[1], "w") as f:
	if src:
		f.write("/*"+"*"*76+"*/\n")
		lib = "libultra" if _ULTRA64 else "libdragon"
		url = "https://github.com/devwizard64/libcart"
		f.write("/*"+("Adapted for use with "+lib+" - "+url).center(76)+"*/\n")
		f.write("/*"+"*"*76+"*/\n")
		f.write("\n")
		if _ULTRA64:
			f.write("#include <ultra64.h>\n")
			f.write("#include <cart.h>\n")
		else:
			f.write("#include \"n64types.h\"\n")
			f.write("#include \"n64sys.h\"\n")
			f.write("#include \"dma.h\"\n")
			f.write("#include \"libcart/cart.h\"\n")
		f.write("\n")
	flag = True
	for path in sys.argv[2:]:
		code = 0
		for line in open(path, "r"):
			if src:
				if line.startswith("extern"):
					if "__os" not in line: continue
				if (
					line.startswith("void __") or
					line.startswith("int __") or
					line.startswith("unsigned char __") or
					line.startswith("u32 __") or
					line.startswith("u64 __")
				): line = "static "+line
			if line == "#ifdef __GNUC__\n":
				if not _ULTRA64:
					code = 2
					continue
			if line == "#ifdef _ULTRA64\n":
				code = 2 if _ULTRA64 else 3
				continue
			if line == "#ifndef _ULTRA64\n":
				code = 3 if _ULTRA64 else 2
				continue
			if line.startswith("#else"):
				if code & 2:
					code ^= 1
					continue
			if line.startswith("#endif"):
				if code & 2:
					code = 0
					continue
			if code & 1: continue
			if _ULTRA64:
				line = line.replace("u_uint64_t", "u64")
				line = line.replace("uint16_t", "u16")
				line = line.replace("uint32_t", "u32")
				line = line.replace("uint64_t", "u64")
			else:
				if line.startswith("typedef"): continue
				if line.startswith("#define __cart_"): continue
				line = line.replace("u16", "uint16_t")
				line = line.replace("u32", "uint32_t")
				line = line.replace("u64", "uint64_t")
				line = line.replace("__cart_rd", "io_read")
				line = line.replace("__cart_wr", "io_write")
				if line.startswith("#define IO_WRITE"):
					line = line[:28] + "\\\n\t\t" + line[28:]
				if line == "__attribute__((aligned(16)))\n":
					line = line[:-1] + " "
				line = line.replace("\t", "    ")
			if src:
				if line.startswith("#include "): continue
				if line.endswith("_H__\n"):
					if line.startswith("#ifndef __"): continue
					if line.startswith("#define __"): continue
				if line.endswith("_H__ */\n"):
					if line.startswith("#endif /* __"): continue
			if line == "\n":
				if not flag:
					flag = True
					f.write(line)
			else:
				flag = False
				f.write(line)
