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

static bool parse_timecode_hhmmss(const std::string& s, double *out_seconds)
{
	// Accept: [hh:]mm:ss[.mmm]
	// Only mm:ss is mandatory.
	size_t c1 = s.find(':');
	if (c1 == std::string::npos) return false;
	size_t c2 = s.find(':', c1 + 1);

	int hh = 0;
	int mm = 0;
	double ss = 0.0;

	auto parse_int = [&](const std::string& t, int *out) -> bool {
		if (t.empty()) return false;
		char *end = NULL;
		long v = strtol(t.c_str(), &end, 10);
		if (end == t.c_str() || *end != 0) return false;
		if (v < 0 || v > INT32_MAX) return false;
		*out = (int)v;
		return true;
	};

	auto parse_sec = [&](const std::string& t, double *out) -> bool {
		double v = 0.0;
		if (!parse_double_strict(t.c_str(), &v)) return false;
		if (!(v >= 0.0 && v < 60.0)) return false;
		*out = v;
		return true;
	};

	if (c2 == std::string::npos) {
		// mm:ss
		if (!parse_int(s.substr(0, c1), &mm)) return false;
		if (!parse_sec(s.substr(c1 + 1), &ss)) return false;
	} else {
		// hh:mm:ss
		if (!parse_int(s.substr(0, c1), &hh)) return false;
		if (!parse_int(s.substr(c1 + 1, c2 - (c1 + 1)), &mm)) return false;
		if (!parse_sec(s.substr(c2 + 1), &ss)) return false;
	}

	if (mm < 0 || mm >= 60) return false;
	*out_seconds = (double)hh * 3600.0 + (double)mm * 60.0 + ss;
	return true;
}

// Parse a text file containing a mix of integers and timestamps (comments allowed with '#').
// Integers are treated as units directly (frames for video, samples for audio).
// Timestamps are parsed as [hh:]mm:ss[.mmm] and converted to units via the provided rate.
static std::vector<int> load_seek_frames_file(const std::string& path, double rate)
{
	if (!(rate > 0.0))
		fatal("seek: invalid rate (must be >0): %.9g", rate);

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

		// Extract first token up to whitespace or '#'
		char *e = p;
		while (*e && *e != ' ' && *e != '\t' && *e != '\r' && *e != '\n' && *e != '#')
			e++;
		std::string tok(p, (size_t)(e - p));

		// Skip trailing spaces/comments after token.
		while (*e == ' ' || *e == '\t') e++;
		if (*e != 0 && *e != '\r' && *e != '\n' && *e != '#') {
			fclose(f);
			fatal("seek: trailing garbage at %s:%d", path.c_str(), lineno);
		}

		double sec = 0.0;
		if (tok.find(':') != std::string::npos) {
			if (!parse_timecode_hhmmss(tok, &sec)) {
				fclose(f);
				fatal("seek: invalid timestamp '%s' at %s:%d", tok.c_str(), path.c_str(), lineno);
			}
			double u = sec * rate;
			if (!(u >= 0.0)) {
				fclose(f);
				fatal("seek: invalid timestamp '%s' at %s:%d", tok.c_str(), path.c_str(), lineno);
			}
			int64_t v = (int64_t)llround(u);
			if (v < 0 || v > INT32_MAX) {
				fclose(f);
				fatal("seek: timestamp out of range at %s:%d", path.c_str(), lineno);
			}
			frames.push_back((int)v);
		} else {
			char *end = NULL;
			long v = strtol(tok.c_str(), &end, 10);
			if (end == tok.c_str() || *end != 0) {
				fclose(f);
				fatal("seek: invalid integer '%s' at %s:%d", tok.c_str(), path.c_str(), lineno);
			}
			if (v < 0 || v > INT32_MAX) {
				fclose(f);
				fatal("seek: out of range integer at %s:%d", path.c_str(), lineno);
			}
			frames.push_back((int)v);
		}
	}
	fclose(f);

	if (frames.empty())
		fatal("seek: frames file is empty: %s", path.c_str());

	std::sort(frames.begin(), frames.end());
	frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
	return frames;
}


