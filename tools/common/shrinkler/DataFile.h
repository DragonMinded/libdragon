// Copyright 1999-2022 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Operations on raw data files, including loading, crunching and saving.

*/

#pragma once

#include <cstring>
#include <algorithm>
#include <string>
#include <utility>
#include <algorithm>

using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::string;

#include "AmigaWords.h"
#include "Pack.h"
#include "RangeDecoder.h"
#include "Verifier.h"

#define SHRINKLER_MAJOR_VERSION 4
#define SHRINKLER_MINOR_VERSION 7
#define FLAG_PARITY_CONTEXT (1 << 0)
#define NUM_RELOC_CONTEXTS 256


struct DataHeader {
	char magic[4];
	char major_version;
	char minor_version;
	Word header_size;
	Longword compressed_size;
	Longword uncompressed_size;
	Longword safety_margin;
	Longword flags;
};

class DataFile {
public: //rasky
	DataHeader header;
	vector<unsigned char> data;

	vector<unsigned char> compress(PackParams *params, RefEdgeFactory *edge_factory, bool show_progress) {
		vector<unsigned char> pack_buffer;
		RangeCoder range_coder(LZEncoder::NUM_CONTEXTS + NUM_RELOC_CONTEXTS, pack_buffer);

		// Print compression status header
		if (show_progress) {
			const char *ordinals[] = { "st", "nd", "rd", "th" };
			printf("Original");
			for (int p = 1 ; p <= params->iterations ; p++) {
				printf("  After %d%s pass", p, ordinals[min(p,4)-1]);
			}
			printf("\n");
		}

		// Crunch the data
		range_coder.reset();
		packData(&data[0], data.size(), 0, params, &range_coder, edge_factory, show_progress);
		range_coder.finish();
		if (show_progress) {
			printf("\n\n");
			fflush(stdout);
		}

		return pack_buffer;		
	}

	int verify(PackParams *params, vector<unsigned char>& pack_buffer, bool show_progress) {
		if (show_progress) {
			printf("Verifying... ");
			fflush(stdout);
		}
		RangeDecoder decoder(LZEncoder::NUM_CONTEXTS + NUM_RELOC_CONTEXTS, pack_buffer);
		LZDecoder lzd(&decoder, params->parity_context);

		// Verify data
		bool error = false;
		//rasky: we do 4-at-atime reads in decompressor, so use 4 here too
		LZVerifier verifier(0, &data[0], data.size(), data.size(), 4);
		decoder.reset();
		decoder.setListener(&verifier);
		if (!lzd.decode(verifier)) {
			error = true;
		}

		// Check length
		if (!error && verifier.size() != data.size()) {
			printf("Verify error: data has incorrect length (%d, should have been %d)!\n", verifier.size(), (int) data.size());
			error = true;
		}

		if (error) {
			internal_error();
		}
		
		if (show_progress) {
			printf("OK\n\n");
		}

		return verifier.front_overlap_margin + pack_buffer.size() - data.size();
	}

public:
	void load(const char *filename) {
		FILE *file;
		if ((file = fopen(filename, "rb"))) {
			fseek(file, 0, SEEK_END);
			int length = ftell(file);
			fseek(file, 0, SEEK_SET);
			data.resize(length);
			if (fread(&data[0], 1, data.size(), file) == data.size()) {
				fclose(file);
				return;
			}
		}

		printf("Error while reading file %s\n\n", filename);
		exit(1);
	}

	void save(const char *filename, bool write_header) {
		FILE *file;
		if ((file = fopen(filename, "wb"))) {
			bool ok = true;
			if (write_header) {
				ok = fwrite(&header, 1, sizeof(DataHeader), file) == sizeof(DataHeader);
			}
			if (ok && fwrite(&data[0], 1, data.size(), file) == data.size()) {
				fclose(file);
				return;
			}
		}

		printf("Error while writing file %s\n\n", filename);
		exit(1);
	}

	int size(bool include_header) {
		return (include_header ? sizeof(DataHeader) : 0) + data.size();
	}

	DataFile* crunch(PackParams *params, RefEdgeFactory *edge_factory, bool show_progress) {
		vector<unsigned char> pack_buffer = compress(params, edge_factory, show_progress);
		int margin = verify(params, pack_buffer, show_progress);

		if (show_progress)
			printf("Minimum safety margin for overlapped decrunching: %d\n\n", margin);

		DataFile *ef = new DataFile;
		ef->data = pack_buffer;
		ef->header.magic[0] = 'S';
		ef->header.magic[1] = 'h';
		ef->header.magic[2] = 'r';
		ef->header.magic[3] = 'i';
		ef->header.major_version = SHRINKLER_MAJOR_VERSION;
		ef->header.minor_version = SHRINKLER_MINOR_VERSION;
		ef->header.header_size = sizeof(DataHeader) - 8;
		ef->header.compressed_size = pack_buffer.size();
		ef->header.uncompressed_size = data.size();
		ef->header.safety_margin = margin;
		ef->header.flags = params->parity_context ? FLAG_PARITY_CONTEXT : 0;

		return ef;
	}
};
