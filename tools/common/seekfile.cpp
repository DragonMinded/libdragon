/*
    seekfile: shared helpers to parse "--seek" style arguments (either seconds or a file list)
    Used by videoconv64 and audioconv64.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string>
#include <vector>
#include <algorithm>

// Both videoconv64 and audioconv64 provide a fatal(const char*, ...) with printf-style formatting.
__attribute__((noreturn, format(printf, 1, 2)))
void fatal(const char *str, ...);

static bool parse_double_strict(const char *s, double *out)
{
	if (!s || !*s) return false;
	char *end = NULL;
	double v = strtod(s, &end);
	if (end == s) return false;
	while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
	if (*end != 0) return false;
	*out = v;
	return true;
}

// Parse a text file containing one integer per line (comments allowed with '#').
// In videoconv64 these are video frame indices; in audioconv64 these are sample offsets.
static std::vector<int> load_seek_frames_file(const std::string& path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) fatal("seek: cannot open frames file: %s", path.c_str());

	std::vector<int> frames;
	char line[256];
	int lineno = 0;
	while (fgets(line, sizeof(line), f)) {
		lineno++;
		char *p = line;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		if (*p == 0) continue;
		if (*p == '#') continue;

		char *end = NULL;
		long v = strtol(p, &end, 10);
		if (end == p) {
			fclose(f);
			fatal("seek: invalid integer at %s:%d", path.c_str(), lineno);
		}
		while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
		if (*end != 0 && *end != '#') {
			fclose(f);
			fatal("seek: trailing garbage at %s:%d", path.c_str(), lineno);
		}
		if (v < 0 || v > INT32_MAX) {
			fclose(f);
			fatal("seek: out of range integer at %s:%d", path.c_str(), lineno);
		}
		frames.push_back((int)v);
	}
	fclose(f);

	if (frames.empty())
		fatal("seek: frames file is empty: %s", path.c_str());

	std::sort(frames.begin(), frames.end());
	frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
	return frames;
}


