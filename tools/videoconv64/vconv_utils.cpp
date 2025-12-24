/*
    videoconv64: shared utilities
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include <time.h>

int64_t now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void sleep_ms(int ms) {
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

std::string temp_dir(void) {
#ifdef _WIN32
	const char *tmp = getenv("TEMP");
	if (!tmp || !tmp[0]) tmp = getenv("TMP");
	if (!tmp || !tmp[0]) tmp = ".";
	return std::string(tmp);
#else
	const char *tmp = getenv("TMPDIR");
	if (!tmp || !tmp[0]) tmp = "/tmp";
	return std::string(tmp);
#endif
}

std::string join_path(const std::string& dir, const std::string& file) {
	if (dir.empty()) return file;
	char last = dir[dir.size() - 1];
	if (last == '/' || last == '\\') return dir + file;
	return dir + "/" + file;
}

std::string base_name(const std::string& path) {
	size_t s = path.find_last_of("/\\");
	return (s == std::string::npos) ? path : path.substr(s + 1);
}

std::string strip_ext(const std::string& name) {
	size_t dot = name.find_last_of('.');
	if (dot == std::string::npos) return name;
	return name.substr(0, dot);
}

std::string format_cmdline_for_log(const std::vector<std::string>& argv) {
	std::string out;
	for (size_t i = 0; i < argv.size(); i++) {
		const std::string &a = argv[i];
		if (i) out += " ";
		bool need_quote = a.find_first_of(" \t\v\"") != std::string::npos;
		if (!need_quote) {
			out += a;
			continue;
		}
		out += "\"";
		for (size_t j = 0; j < a.size(); j++) {
			char c = a[j];
			if (c == '\\' || c == '"') out += '\\';
			out += c;
		}
		out += "\"";
	}
	return out;
}

