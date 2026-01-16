/*
    videoconv64: shared utilities
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <atomic>

static std::atomic<int> g_progress_mode{ (int)PROGRESS_MODE_VIDEO };

void progressbar_set_mode(progress_mode_t mode) {
	g_progress_mode.store((int)mode, std::memory_order_relaxed);
}

const char* progressbar_get_mode_label(void) {
	switch ((progress_mode_t)g_progress_mode.load(std::memory_order_relaxed)) {
		case PROGRESS_MODE_VIDEO: return "Video";
		case PROGRESS_MODE_AUDIO: return "Audio";
		case PROGRESS_MODE_VIDEO_AUDIO: return "Video/Audio";
		default: return "Video";
	}
}

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

void progressbar_clear(void) {
	if (!cfg.progress) return;
	// Clear current line (best-effort, no ANSI). Print spaces and carriage return.
	fprintf(stderr, "\r%*s\r", 80, "");
	fflush(stderr);
}

static void format_mmss(char out[16], int sec) {
	if (sec < 0 || sec > 99 * 60 + 59) {
		strcpy(out, "--:--");
		return;
	}
	int mm = sec / 60;
	int ss = sec % 60;
	snprintf(out, 16, "%02d:%02d", mm, ss);
}

void progressbar_infinite_update(int sec) {
	// When duration is unknown, show an "infinite" progress bar animation + time (MM:SS).
	if (!cfg.progress) return;

	const int width = 40;
	const int block = 8;
	int64_t now = now_ms();

	// Move the block at ~5 chars/sec (200ms per step).
	int step = (int)((now / 200) % (width + block));
	int start = step - block;
	int end = step;

	char bar[width + 1];
	for (int i = 0; i < width; i++) {
		bar[i] = (i >= start && i < end) ? '#' : '-';
	}
	bar[width] = '\0';

	char mmss[16];
	format_mmss(mmss, sec);
	fprintf(stderr, "\r%s [%s]  %s", progressbar_get_mode_label(), bar, mmss);
	fflush(stderr);
}

static void format_eta_mmss(char out[16], double eta_sec) {
	if (!(eta_sec > 0.0) || eta_sec > 99 * 60 + 59) {
		strcpy(out, "--:--");
		return;
	}
	int sec = (int)(eta_sec + 0.5);
	int mm = sec / 60;
	int ss = sec % 60;
	snprintf(out, 16, "%02d:%02d", mm, ss);
}

void progressbar_update(double overall_pct, double eta_sec) {
	// Draw an ASCII progress bar on a single terminal line (stderr).
	// Called only when cfg.verbose == 0.
	if (!cfg.progress) return;
	if (overall_pct < 0.0) overall_pct = 0.0;
	if (overall_pct > 100.0) overall_pct = 100.0;
	const int width = 40;
	int filled = (int)((overall_pct / 100.0) * width + 0.5);
	if (filled < 0) filled = 0;
	if (filled > width) filled = width;

	char bar[width + 1];
	for (int i = 0; i < width; i++) bar[i] = (i < filled) ? '#' : '-';
	bar[width] = '\0';

	char eta[16];
	format_eta_mmss(eta, eta_sec);

	if (eta_sec >= 0.0) {
		fprintf(stderr, "\r%s [%s] %6.1f%% ETA %s", progressbar_get_mode_label(), bar, overall_pct, eta);
	} else {
		fprintf(stderr, "\r%s [%s] %6.1f%%", progressbar_get_mode_label(), bar, overall_pct);
	}
	fflush(stderr);
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

