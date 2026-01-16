/*
	n64metadata: a program used to embed metadata into an N64 ROM,
	according to the Homebrew Header Specification:
	Copyright (C) 2025 Giovanni Bajo (giovannibajo@gmail.com)

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
 */
/*
	This tool implements the specification of ROM metadata for the Homebrew Header:
	https://n64brew.dev/wiki/ROM_Metadata

	It is designed to be a self-contained C++ file with zero dependencies, and
	zero assumptions about the SDK used to build the ROM. In other words, it
	does not depend on Libdragon in any way, and can be used for any N64 ROM.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <unistd.h>
#include <cerrno>

bool flag_verbose = false;
bool flag_external = false;
bool flag_force = false;
int flag_padding = 16384;

// Binary little-endian helpers
static inline void w16(FILE *f, uint16_t v) { fputc(v, f); fputc(v >> 8, f); }
static inline void w32(FILE *f, uint32_t v) { w16(f, v); w16(f, v >> 16); }
static inline uint16_t r16(const uint8_t *p) { return (uint16_t)p[0]   | (uint16_t)p[1] << 8; }
static inline uint32_t r32(const uint8_t *p) { return (uint32_t)r16(p) | (uint32_t)r16(p + 2) << 16; }
static inline uint16_t r16be(const uint8_t *p) { return ((uint16_t)p[0] << 8) | (uint16_t)p[1]; }
static inline uint32_t r32be(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]; }

__attribute__((format(printf, 1, 2)))
static void verbose(const char *fmt, ...)
{
    if (!flag_verbose) return;
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

__attribute__((noreturn, format(printf, 1, 2)))
static void fatal(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
    fprintf(stderr, "n64metadata: fatal: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

// UTF-8 validation
static bool validUTF8(const std::vector<uint8_t> &s)
{
	const unsigned char *p = (const unsigned char *)s.data();
	const unsigned char *e = p + s.size();

	while (p < e) {
		unsigned c = *p++;
		int n = (c <  0x80) ? 0 :
		        (c >= 0xF0) ? 3 :
		        (c >= 0xE0) ? 2 :
		        (c >= 0xC0) ? 1 : -1;

		if (n < 0 || p + n > e)
			return false;

		while (n--) {
			if ((*p++ & 0xC0) != 0x80)
				return false;
		}
	}

	return true;
}

static void usage(void)
{
	fprintf(stderr, "Usage: n64metadata [options] <rom.z64> <metadata.ini>\n\n");
	fprintf(stderr, "Embed or generate metadata ZIPs for N64 Homebrew Header.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -h, --help       	Show this help message and exit\n");
	fprintf(stderr, "  -v, --verbose    	Verbose output\n");
	fprintf(stderr, "  -e, --external   	Do not modify ROM; write sidecar .meta ZIP\n");
	fprintf(stderr, "  -f, --force      	Overwrite existing embedded metadata in ROM\n");
	fprintf(stderr, "  -P, --padding <N>    Padding alignment for the final ROM (default: 16 KiB)\n");
	fprintf(stderr, "\n");
}

static uint32_t crc_table[256];

static void crc32_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1)	c = 0xEDB88320 ^ (c >> 1);
            else		c = c >> 1;
        }
        crc_table[i] = c;
    }
}

static uint32_t crc32(const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
}

struct ZipEntry {
	std::string name;
	std::vector<uint8_t> data;
};

static void trim(std::string &s)
{
	while (!s.empty() && isspace((unsigned char)s.front()))
		s.erase(s.begin());
	while (!s.empty() && isspace((unsigned char)s.back()))
		s.pop_back();
}

static bool has_prefix(const std::string &s, const char *pfx)
{
	size_t n = strlen(pfx);
	return s.size() >= n && s.compare(0, n, pfx) == 0;
}

static bool has_suffix(const std::string &s, const char *sfx)
{
	size_t n = strlen(sfx);
	return s.size() >= n && s.compare(s.size() - n, n, sfx) == 0;
}

static bool validate_rel_path(const std::string &fn, const char *ini,
                              int lineno)
{
	if (fn.empty()) {
		fprintf(stderr, "%s:%d: error: empty filename is not allowed\n",
		        ini, lineno);
		return false;
	}

	if (fn[0] == '/' || (fn.size() >= 2 && isalpha((unsigned char)fn[0]) && fn[1] == ':')) {
		fprintf(stderr, "%s:%d: error: absolute paths are not supported: %s\n",
		        ini, lineno, fn.c_str());
		return false;
	}

	size_t pos = 0;
	while (pos <= fn.size()) {
		size_t slash = fn.find('/', pos);
		size_t len = (slash == std::string::npos) ? fn.size() - pos : slash - pos;
		std::string comp = fn.substr(pos, len);

		if (comp.empty()) {
			fprintf(stderr, "%s:%d: error: empty path component in '%s'\n", ini, lineno, fn.c_str());
			return false;
		}

		if (comp == "." || comp == "..") {
			fprintf(stderr, "%s:%d: error: '.' and '..' components are not allowed " "in paths: %s\n", ini, lineno, fn.c_str());
			return false;
		}

		if (slash == std::string::npos)
			break;

		pos = slash + 1;
	}

	return true;
}

// Minimal PNG parser: extract width/height from IHDR
static bool get_png_dimensions(const std::vector<uint8_t> &data, int &w, int &h)
{
	static const uint8_t sig[8] = { 0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A };
	if (data.size() < 8 + 8 + 13 + 4) return false;
	if (memcmp(data.data(), sig, 8) != 0) return false;
	size_t pos = 8;
	while (pos + 8 <= data.size()) {
		uint32_t len = r32be(&data[pos]);
		uint32_t type = r32be(&data[pos + 4]);
		pos += 8;
		if (pos + len + 4 > data.size()) return false;
		if (type == 0x49484452) { // "IHDR"
			if (len < 8) return false;
			w = (int)r32be(&data[pos + 0]);
			h = (int)r32be(&data[pos + 4]);
			return (w > 0 && h > 0);
		}
		pos += len + 4; // skip data + CRC
	}
	return false;
}

// Minimal JPEG parser: extract width/height from SOF markers
static bool get_jpeg_dimensions(const std::vector<uint8_t> &data, int &w, int &h)
{
	if (data.size() < 4) return false;
	if (data[0] != 0xFF || data[1] != 0xD8) return false; // SOI
	size_t pos = 2;
	while (pos + 1 < data.size()) {
		// Skip fill bytes 0xFF
		if (data[pos] != 0xFF) { pos++; continue; }
		while (pos < data.size() && data[pos] == 0xFF) pos++;
		if (pos >= data.size()) break;
		uint8_t marker = data[pos++];
		// Standalone markers without length
		if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
			if (marker == 0xD9) break; // EOI
			continue;
		}
		// Segment with length
		if (pos + 2 > data.size()) return false;
		uint16_t seglen = r16be(&data[pos]);
		if (seglen < 2) return false;
		if (pos + seglen > data.size()) return false;
		// SOF0/1/2/3/5/6/7/9/A/B/D/E/F contain dimensions
		if ((marker >= 0xC0 && marker <= 0xC3) ||
		    (marker >= 0xC5 && marker <= 0xC7) ||
		    (marker >= 0xC9 && marker <= 0xCB) ||
		    (marker >= 0xCD && marker <= 0xCF)) {
			if (seglen < 7) return false;
			size_t p = pos + 3; // skip length(2) and precision(1)
			if (p + 4 > data.size()) return false;
			h = (int)r16be(&data[p + 0]);
			w = (int)r16be(&data[p + 2]);
			return (w > 0 && h > 0);
		}
		// Stop at SOS if we haven't found SOF yet (markers after SOS are in entropy-coded data)
		if (marker == 0xDA) break;
		pos += seglen;
	}
	return false;
}

// Read a file into a vector
static bool slurp(const std::string &path, std::vector<uint8_t> &data, const char *ctx)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;

	std::stringstream buf;
	buf << f.rdbuf();
	std::string tmp = buf.str();
	data.assign(tmp.begin(), tmp.end());
	return true;
}

static int calc_zip_size(const std::vector<ZipEntry> &entries)
{
	int size = 0;
	for (const ZipEntry &e : entries) {
		size += 30 + e.name.size() + e.data.size();
        size += 46 + e.name.size();
	}
    const int eocd_size = 22;
	return size + eocd_size;
}

// Create an uncompressed ZIP file from a list of entries
static void write_zip(FILE *z, const std::vector<ZipEntry> &entries, int padding)
{
	size_t n = entries.size();

	std::vector<long> offs(n);
	std::vector<uint32_t> crc(n), sz(n);

	// Write files
	for (size_t i = 0; i < n; i++) {
		const ZipEntry &e = entries[i];

		offs[i] = ftell(z);
		sz[i] = (uint32_t)e.data.size();
		crc[i] = crc32(e.data.data(), e.data.size());

		w32(z, 0x04034b50);                 // Local file header signature
		w16(z, 20);                         // Version needed to extract
		w16(z, 0);                          // General purpose bit flag
		w16(z, 0);                          // Compression method (0 = no compression)  
		w16(z, 0);                          // File last modification time
		w16(z, 0);                          // File last modification date
		w32(z, crc[i]);                     // CRC32 of the file data
		w32(z, sz[i]);                      // Compressed size
		w32(z, sz[i]);                      // Uncompressed size
		w16(z, (uint16_t)e.name.size());    // Filename length
		w16(z, 0);                          // Extra field length
		fwrite(e.name.c_str(), 1, e.name.size(), z);
		fwrite(e.data.data(), 1, e.data.size(), z);
	}

    // Add padding if requested. We put the padding *within* the ZIP, so that
	// if the ZIP is replaced, also the padding is removed.
    for (int i = 0; i < padding; i++) {
        fputc(0, z);
    }

    long cd = ftell(z);

	// Write central directory
	for (size_t i = 0; i < n; i++) {
		const ZipEntry &e = entries[i];

		w32(z, 0x02014b50);              // Central directory entry signature
		w16(z, 20);                      // Version made by
		w16(z, 20);                      // Version needed to extract
		w16(z, 0);                       // General purpose bit flag
		w16(z, 0);                       // Compression method
		w16(z, 0);                       // File last modification time
		w16(z, 0);                       // File last modification date
		w32(z, crc[i]);                  // CRC32 of the file data
		w32(z, sz[i]);                   // Compressed size
		w32(z, sz[i]);                   // Uncompressed size
		w16(z, (uint16_t)e.name.size()); // Filename length
		w16(z, 0);                       // Extra field length
		w16(z, 0);                       // File comment length
		w16(z, 0);                       // Disk number start
		w16(z, 0);                       // Internal file attributes
		w32(z, 0);                       // External file attributes
		w32(z, (uint32_t)offs[i]);       // Relative offset of local header
		fwrite(e.name.c_str(), 1, e.name.size(), z);
	}

	long cd_end = ftell(z);

	// Write end of central directory
	w32(z, 0x06054b50);
	w16(z, 0);                       // Number of this disk
	w16(z, 0);                       // Disk with start of central directory
	w16(z, (uint16_t)n);             // Total entries on this disk
	w16(z, (uint16_t)n);             // Total entries
	w32(z, (uint32_t)(cd_end - cd)); // Size of central directory
	w32(z, (uint32_t)cd);            // Offset of central directory
	w16(z, 0);                       // Comment length
}

// Remove a previously embedded ZIP from the ROM
static bool remove_embedded_zip(FILE *rom, const char *rom_path)
{
	fseek(rom, 0, SEEK_END);
	long size = ftell(rom);

	const long max_search = (size < 200000) ? size : 200000;
	long start = size - max_search;
		
	std::vector<uint8_t> buf((size_t)max_search);
	fseek(rom, start, SEEK_SET);
	fread(buf.data(), 1, max_search, rom);

	long eocd_pos = -1;
	for (long i = max_search - 4; i >= 0; i--) {
        if (r32(&buf[i]) == 0x06054b50) {
			eocd_pos = i;
			break;
		}
	}

	if (eocd_pos < 0) {
		fprintf(stderr, "n64metadata: warning: could not find existing ZIP in %s\n", rom_path);
		return false;
	}

    // Read central directory position and size
	uint32_t cd_size = r32(&buf[eocd_pos + 12]);
	uint32_t cd_off  = r32(&buf[eocd_pos + 16]);
	if (cd_off + cd_size > size) {
		fprintf(stderr, "n64metadata: warning: central directory is out of range in %s\n", rom_path);
		return false;
	}

    // Read central directory
	std::vector<uint8_t> cd(cd_size);
	fseek(rom, cd_off, SEEK_SET);
	fread(cd.data(), 1, cd_size, rom);

    // Go through central directory to find the smallest local header offset,
    // that is, the first file in the ZIP.
	uint32_t min_local = 0xFFFFFFFFu;
	size_t pos = 0;

	while (pos + 46 <= cd_size) {
		if (r32(&cd[pos]) != 0x02014b50)
			break;

		uint16_t name_len    = r16(&cd[pos + 28]);
		uint16_t extra_len   = r16(&cd[pos + 30]);
		uint16_t comment_len = r16(&cd[pos + 32]);
		uint32_t local_off   = r32(&cd[pos + 42]);

		if (local_off < min_local)
            min_local = local_off;

        pos += 46 + name_len + extra_len + comment_len;
	}

    // Truncate ROM to remove previous ZIP
	verbose("n64metadata: info: truncating %s at offset %u to remove previous ZIP\n", rom_path, min_local);
	if (ftruncate(fileno(rom), (off_t)min_local) != 0) {
		fprintf(stderr, "n64metadata: warning: failed to truncate %s, leaving old ZIP\n", rom_path);
		return false;
	}

    // just in case, make sure the file pointer is valid after truncation
	fseek(rom, 0, SEEK_END);
	return true;
}

int main(int argc, char **argv)
{
	const char *rom_path = nullptr, *ini_path = nullptr;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
			usage();
			return 0;
		}
		else if (!strcmp(a, "-v") || !strcmp(a, "--verbose")) {
			flag_verbose = true;
		}
		else if (!strcmp(a, "-e") || !strcmp(a, "--external")) {
			flag_external = true;
		}
		else if (!strcmp(a, "-f") || !strcmp(a, "--force")) {
			flag_force = true;
		}
		else if (!strcmp(a, "-P") || !strcmp(a, "--padding")) {
			if (++i >= argc) {
				fprintf(stderr, "n64metadata: error: missing argument for %s\n", a);
				return 1;
			}
            try {
                flag_padding = std::stoi(argv[i]);
                if (flag_padding < 0) {
                    fprintf(stderr, "n64metadata: error: padding must be positive\n");
                    return 1;
                }
            } catch (const std::invalid_argument &e) {
                fprintf(stderr, "n64metadata: error: invalid padding argument: %s\n", argv[i]);
                return 1;
            }
        }
		else if (a[0] == '-') {
			fprintf(stderr, "n64metadata: error: unknown option '%s'\n", a);
			usage();
			return 1;
		}
		else if (!rom_path) {
			rom_path = a;
		}
		else if (!ini_path) {
			ini_path = a;
		}
		else {
			fprintf(stderr, "n64metadata: error: too many arguments\n");
			usage();
			return 1;
		}
	}

	if (!rom_path || !ini_path) {
		fprintf(stderr, "n64metadata: error: missing ROM and/or INI file\n");
		usage();
		return 1;
	}

	crc32_init();

    std::vector<uint8_t> content;
    if (!slurp(ini_path, content, ini_path)) {
        fatal("%s: error: cannot open file\n", ini_path);
    }


	if (!validUTF8(content)) {
		fatal("%s: error: file is not valid UTF-8\n", ini_path);
	}

	static std::vector<std::string> metaKeys = {
        "name", "author", "release-date", "osi-license", "website", "age-rating", "short-desc", "long-desc", "screenshots"
    };
	static std::vector<std::string> boxartKeys = { "front", "back", "top", "bottom", "left", "right" };
	static std::vector<std::string> cartartKeys = { "front", "back" };

	std::string ini_dir;
	{
		std::string ip(ini_path);
		size_t slash = ip.find_last_of('/');
		if (slash != std::string::npos)
			ini_dir = ip.substr(0, slash + 1);
	}

    std::string content_str(content.begin(), content.end());
    std::istringstream in(content_str);
    std::string section;
	int lineno = 0;
	bool has_error = false;
    std::vector<std::string> *valid_keys = nullptr;

    std::vector<ZipEntry> entries;

	std::string line;
	while (std::getline(in, line)) {
		lineno++;

		std::string s = line;
		trim(s);

		if (s.empty() || s[0] == '#' || s[0] == ';')
			continue;

		if (s.back() == '\r')
			s.pop_back();

		if (s.front() == '[' && s.back() == ']') {
			section = s.substr(1, s.size() - 2);

            if (section == "meta" || has_prefix(section, "meta.")) valid_keys = &metaKeys;
            else if (section == "boxart" || has_prefix(section, "boxart.")) valid_keys = &boxartKeys;
            else if (section == "cartart" || has_prefix(section, "cartart.")) valid_keys = &cartartKeys;
            else {
				fprintf(stderr, "%s:%d: warning: unknown section '%s'\n", ini_path, lineno, section.c_str());
                valid_keys = nullptr;
            }
            continue;
		}

		size_t eq = s.find('=');
		if (eq == std::string::npos) {
			fprintf(stderr,"%s:%d: error: invalid line (expected key = value)\n", ini_path, lineno);
			has_error = true;
			continue;
		}

		std::string key = s.substr(0, eq);
		std::string val = s.substr(eq + 1);

		trim(key);
		trim(val);

        if (valid_keys && std::find(valid_keys->begin(), valid_keys->end(), key) == valid_keys->end()) {
            fprintf(stderr, "%s:%d: warning: unknown key '%s'\n", ini_path, lineno, key.c_str());
            continue;
        }

        auto process_file = [&](const std::string &fn, std::initializer_list<const char *> valid_exts, bool check_utf8) {
            bool valid = false;
            for (const char *ext : valid_exts) {
                if (has_suffix(fn, ext)) {
                    valid = true;
                    break;
                }
            }

            if (!valid) {
                fprintf(stderr, "%s:%d: warning: invalid filename extension: %s (expected one of:", ini_path, lineno, fn.c_str());
                for (const char *ext : valid_exts) fprintf(stderr, " %s", ext);
                fprintf(stderr, ")\n");
                has_error = true;
                return;
            }

            if (!validate_rel_path(fn, ini_path, lineno)) {
                has_error = true;
                return;
            }

            ZipEntry e;
            if (!slurp(ini_dir + fn, e.data, ini_path)) {
                fprintf(stderr, "%s:%d: error: cannot open file '%s'\n", ini_path, lineno, fn.c_str());
                has_error = true;
            } else {
                if (check_utf8 && !validUTF8(e.data)) {
                    fprintf(stderr, "%s:%d: error: file '%s' is not valid UTF-8\n", ini_path, lineno, fn.c_str());
                    has_error = true;
                    return;
                }
                // Image size validation for PNG and JPEG
                if (has_suffix(fn, ".png")) {
                    int w = 0, h = 0;
                    if (!get_png_dimensions(e.data, w, h)) {
                        fprintf(stderr, "%s:%d: error: file '%s' is not a valid PNG or missing IHDR\n", ini_path, lineno, fn.c_str());
                        has_error = true;
                        return;
                    }
                    bool ok = ((w <= 320 && h <= 240) || (w <= 240 && h <= 320));
                    if (!ok) {
                        fprintf(stderr, "%s:%d: error: image '%s' has size %dx%d, exceeds 320x240/240x320\n", ini_path, lineno, fn.c_str(), w, h);
                        has_error = true;
                        return;
                    }
                } else if (has_suffix(fn, ".jpg") || has_suffix(fn, ".jpeg")) {
                    int w = 0, h = 0;
                    if (!get_jpeg_dimensions(e.data, w, h)) {
                        fprintf(stderr, "%s:%d: error: file '%s' is not a valid JPEG or missing SOF\n", ini_path, lineno, fn.c_str());
                        has_error = true;
                        return;
                    }
                    bool ok = ((w <= 320 && h <= 240) || (w <= 240 && h <= 320));
                    if (!ok) {
                        fprintf(stderr, "%s:%d: error: image '%s' has size %dx%d, exceeds 320x240/240x320\n", ini_path, lineno, fn.c_str(), w, h);
                        has_error = true;
                        return;
                    }
                }
                e.name = fn;
                entries.emplace_back(std::move(e));
            }
        };

		if (valid_keys == &metaKeys && key == "screenshots") {
			std::stringstream ss(val);
			std::string tok;

			while (std::getline(ss, tok, ',')) {
				trim(tok);
                if (tok.empty()) {
                    fprintf(stderr, "%s:%d: warning: empty filename in screenshots list\n", ini_path, lineno);
                    continue;
                }
                process_file(tok, { ".png", ".jpg", ".jpeg" }, false);
			}
		} else if (valid_keys == &metaKeys && key == "long-desc" && !val.empty()) {
             process_file(val, { ".txt" }, true);
        } else if (valid_keys == &metaKeys && key == "release-date" && !val.empty()) {
            // Check that the format is YYYY-MM-DD
            if (val.size() != 10 || val[4] != '-' || val[7] != '-') {
                fprintf(stderr, "%s:%d: warning: invalid release date format: %s\n", ini_path, lineno, val.c_str());
                has_error = true;
                continue;
            }

            // Basic validation of the date, nothing too fancy
            try {
                int year = std::stoi(val.substr(0, 4));
                int month = std::stoi(val.substr(5, 2));
                int day = std::stoi(val.substr(8, 2));
                if (year < 1900 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
                    fprintf(stderr, "%s:%d: warning: invalid release date: %s\n", ini_path, lineno, val.c_str());
                    has_error = true;
                    continue;
                }
            } catch (const std::invalid_argument &e) {
                fprintf(stderr, "%s:%d: warning: invalid release date format: %s\n", ini_path, lineno, val.c_str());
                has_error = true;
                continue;
            }
        } else if (valid_keys == &metaKeys && key == "age-rating" && !val.empty()) {
            // Check that the age rating is a valid integer value
            try {
                int age_rating = std::stoi(val);
                if (age_rating < 0 || age_rating > 18) {
                    fprintf(stderr, "%s:%d: warning: invalid age rating: %s (must be between 0 and 18)\n", ini_path, lineno, val.c_str());
                    has_error = true;
                    continue;
                }
            } catch (const std::invalid_argument &e) {
                fprintf(stderr, "%s:%d: warning: invalid age rating format: %s (should be a number)\n", ini_path, lineno, val.c_str());
                has_error = true;
                continue;
            }
        } else if ((valid_keys == &boxartKeys || valid_keys == &cartartKeys) && !val.empty()) {
            process_file(val, { ".png", ".jpg", ".jpeg" }, false);
        }
	}

	if (has_error)
		return 1;

    // Add metadata.ini as last entry
	ZipEntry meta_entry;
	meta_entry.name = "metadata.ini";
	meta_entry.data.assign(content.begin(), content.end());
	entries.push_back(meta_entry);

	if (flag_external) {
		std::string meta_path = rom_path;
		size_t slash = meta_path.find_last_of('/');
		size_t dot = meta_path.find_last_of('.');

		if (dot == std::string::npos || (slash != std::string::npos &&  dot < slash)) {
			meta_path += ".meta";
		} else {
			meta_path = meta_path.substr(0, dot) + ".meta";
		}

		verbose("n64metadata: info: writing external metadata to %s\n", meta_path.c_str());

        FILE *z = fopen(meta_path.c_str(), "wb");
		if (!z) fatal("cannot open %s for writing\n", meta_path.c_str());

		write_zip(z, entries, 0);
        if (ferror(z)) {
            fclose(z); remove(meta_path.c_str());
            fatal("failed to write ZIP to %s: %s\n", meta_path.c_str(), strerror(errno));
        }
		fclose(z);
		return 0;
	}

	FILE *rom = fopen(rom_path, "r+b");
	if (!rom) fatal("cannot open ROM %s\n", rom_path);

	uint8_t header[0x40];
	fread(header, 1, sizeof(header), rom);

    if (header[0x3C] != 'E' || header[0x3D] != 'D') {
        verbose("n64metadata: info: setting ROM for homebrew header\n");
        header[0x3C] = 'E'; header[0x3D] = 'D';
    }

	bool has_meta = (header[0x38] & 1) != 0;
	if (has_meta && !flag_force) {
		fatal("ROM already has embedded metadata; use --force to overwrite\n");
	}

	if (!has_meta) {
		header[0x38] |= 1;
		fseek(rom, 0, SEEK_SET);
		fwrite(header, 1, sizeof(header), rom);
	}
	else if (flag_force) {
		if (!remove_embedded_zip(rom, rom_path))
			fprintf(stderr, "n64metadata: warning: could not remove existing embedded metadata, appending new ZIP\n");
	}

	verbose("n64metadata: info: embedding metadata ZIP into %s\n", rom_path);
	fseek(rom, 0, SEEK_END);
    int size = ftell(rom);
    // For iQue compatibility, we need to pad the ROM to a multiple of 16 KiB. Write
    // the padding inside the ZIP, so that if the ZIP is replaced, also the padding is.
    int zip_size = calc_zip_size(entries);
    int padding = 0;
    if (flag_padding)
        padding = (flag_padding - ((size + zip_size) % flag_padding)) % flag_padding;
    verbose("n64metadata: info: ZIP file is %d bytes, padding ROM with %d bytes\n", zip_size, padding);
	write_zip(rom, entries, padding);
	if (ferror(rom))
        fatal("failed to append metadata ZIP to %s: %s\n", rom_path, strerror(errno));

	fclose(rom);
	return 0;
}
