/*
    videoconv64 audio bridge module
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include "../common/json.hpp"
#include "../common/utils.h"
#include "../common/polyfill.h"

#include <string>
#include <errno.h>
#include <string.h>
#include <vector>
#include <math.h>
#include <algorithm>

using json = nlohmann::json;

// Audio extraction format (fixed, independent of user-requested final output).
// We then inject SMPL loop points in this domain, and let audioconv64 resample/downmix if requested.
#define VCONV_AUDIO_EXTRACT_RATE     48000
#define VCONV_AUDIO_EXTRACT_CHANNELS 2

static std::string effective_output_dir(void) {
	// Current tool behavior: if cfg.output_dir is empty, outputs go to current directory.
	return cfg.output_dir.empty() ? std::string(".") : cfg.output_dir;
}

static std::string audioconv64_path(void) {
	const char *inst = n64_tools_dir();
	if (!inst) {
		fatal("N64_INST is not defined but it is required to find audioconv64");
	}
#ifdef _WIN32
	return join_path(join_path(inst, "bin"), "audioconv64.exe");
#else
	return join_path(join_path(inst, "bin"), "audioconv64");
#endif
}

static bool file_exists(const std::string& p) {
	FILE *f = fopen(p.c_str(), "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

struct AudioStreamInfo {
	int stream_index = -1;
	std::string language;
};

static std::vector<AudioStreamInfo> ffprobe_list_audio_streams(void) {
	std::vector<std::string> cmd = {
		cfg.ffprobe_path,
		"-v", "error",
		"-select_streams", "a",
		"-show_entries", "stream=index:stream_tags=language",
		"-of", "json",
		cfg.input_file,
	};

	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) return {};

	std::vector<AudioStreamInfo> res;
	try {
		json j = json::parse(out);
		if (!j.contains("streams") || !j["streams"].is_array()) return res;
		for (const auto &s : j["streams"]) {
			AudioStreamInfo ai;
			ai.stream_index = s.value("index", -1);
			if (s.contains("tags") && s["tags"].is_object()) {
				ai.language = s["tags"].value("language", std::string());
			}
			res.push_back(std::move(ai));
		}
	} catch (...) {
		return {};
	}
	return res;
}

static std::string output_wav64_path(int out_idx, int total) {
	std::string base = strip_ext(base_name(cfg.input_file));
	std::string name;
	if (total <= 1) name = base + ".wav64";
	else name = base + "." + std::to_string(out_idx) + ".wav64";
	return join_path(effective_output_dir(), name);
}

static std::string extract_audio_to_tmpwav(const std::string &tmpwav, bool from_container, int stream_index, const std::string &audio_file) {
	std::vector<std::string> cmd = {
		cfg.ffmpeg_path,
		"-hide_banner",
		"-nostats",
		"-y",
		"-v", "error",
		"-i", from_container ? cfg.input_file : audio_file,
		"-vn",
	};
	if (from_container && stream_index >= 0) {
		cmd.push_back("-map");
		cmd.push_back("0:" + std::to_string(stream_index));
	}
	cmd.insert(cmd.end(), {
		"-c:a", "pcm_s16le",
		"-ar", std::to_string(VCONV_AUDIO_EXTRACT_RATE),
		"-ac", std::to_string(VCONV_AUDIO_EXTRACT_CHANNELS),
		"-f", "wav",
		tmpwav,
	});
	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) {
		remove(tmpwav.c_str());
		fatal("ffmpeg audio extraction failed (rc=%d)", rc);
	}
	return tmpwav;
}


static std::string format_timecode(double sec)
{
	if (sec < 0) sec = 0;
	int64_t ms = (int64_t)floor(sec * 1000.0 + 0.5);
	int64_t s = ms / 1000;
	int msec = (int)(ms % 1000);
	int hh = (int)(s / 3600);
	int mm = (int)((s % 3600) / 60);
	int ss = (int)(s % 60);

	char buf[64];
	if (hh > 0) {
		snprintf(buf, sizeof(buf), "%d:%02d:%02d.%03d", hh, mm, ss, msec);
	} else {
		snprintf(buf, sizeof(buf), "%02d:%02d.%03d", mm, ss, msec);
	}
	return std::string(buf);
}

static std::string write_audio_seekfile(const std::string& wav_path, const std::vector<seek_point_t>& seek_points, double video_fps)
{
	assert(video_fps > 0.0);
	std::string tmp = wav_path + ".seek.txt";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (!f) fatal("cannot create audio seekfile: %s", tmp.c_str());

	// Use timestamps so the mapping remains valid even if FPS is overridden.
	// audioconv64 --wav-seek will convert timestamps to sample offsets using the final sample rate.
	for (const auto& p : seek_points) {
		double t = (double)p.frame / video_fps;
		std::string tc = format_timecode(t);
		fprintf(f, "%s\n", tc.c_str());
	}
	fclose(f);
	return tmp;
}

AudioResult vconv_audio_bridge(std::vector<seek_point_t> seek_points, double video_fps, bool show_progress) {
	AudioResult ar;
	if (!cfg.audio) return ar;

	std::vector<AudioStreamInfo> container_streams = ffprobe_list_audio_streams();
	const int total_sources = (int)container_streams.size() + (int)cfg.audio_files.size();
	if (total_sources == 0) {
		verbose(1, "No audio sources found; skipping audio conversion");
		return ar;
	}

	// Ensure audioconv64 exists if we need it.
	const std::string ac64 = audioconv64_path();
	{
		std::string out;
		int rc = run_process({ ac64, "--help" }, out);
		if (rc != 0) fatal("audioconv64 not found or not executable: %s", ac64.c_str());
	}

	int out_idx = 0;

	auto convert_one = [&](bool from_container, int stream_index, const std::string &src_path) {
		std::string stem = strip_ext(base_name(cfg.input_file));
		std::string tmpwav = join_path(temp_dir(), "videoconv64_" + stem + "_" + std::to_string((long long)now_ms()) + "_" + std::to_string(out_idx) + ".wav");

		verbose(1, "Extracting audio...");
		extract_audio_to_tmpwav(tmpwav, from_container, stream_index, src_path);

		// If seeking is enabled, generate a temporary seekfile and pass it to audioconv64.
		// This is simpler and more robust than injecting CUE metadata into the WAV.
		std::string seekfile;
		if (!seek_points.empty()) {
			seekfile = write_audio_seekfile(tmpwav, seek_points, video_fps);
		}

		verbose(1, "Converting audio with audioconv64...");
		std::string out_wav64 = output_wav64_path(out_idx, total_sources);
		std::string produced_wav64 = join_path(effective_output_dir(), strip_ext(base_name(tmpwav)) + ".wav64");
		{
			std::vector<std::string> cmd = {
				ac64,
				"-o", effective_output_dir(),
			};
			if (!cfg.audio_compress.empty()) {
				cmd.push_back("--wav-compress");
				cmd.push_back(cfg.audio_compress);
			}
			cmd.push_back("--wav-resample");
			cmd.push_back(std::to_string(cfg.audio_rate));
			if (cfg.audio_channels == 1) cmd.push_back("--wav-mono");
			if (!seekfile.empty()) {
				cmd.push_back("--wav-seek");
				cmd.push_back(seekfile);
			}
			cmd.push_back(tmpwav);

			const bool want_prog = show_progress && cfg.progress && cfg.verbose == 0 && total_sources == 1;
			if (want_prog) cmd.insert(cmd.begin() + 1, "-v");

			double last_pct = -1.0;
			std::string out;
			int rc = run_process_pipe(cmd, &out, [&](const std::string& line) {
				if (!want_prog) return;
				const char *prefix = "Resampling:";
				if (line.rfind(prefix, 0) != 0) return;
				size_t lp = line.find('(');
				size_t pp = line.find('%');
				if (lp == std::string::npos || pp == std::string::npos || pp <= lp + 1) return;
				std::string pct_s = line.substr(lp + 1, pp - (lp + 1));
				double pct = atof(pct_s.c_str());
				if (!(pct >= 0.0 && pct <= 100.0)) return;
				if (pct - last_pct >= 0.2 || pct == 100.0) {
					last_pct = pct;
					progressbar_update(pct, -1.0);
				}
			});
			if (want_prog) { progressbar_clear(); }

			remove(tmpwav.c_str());
			if (!seekfile.empty()) remove(seekfile.c_str());
			if (rc != 0 || !file_exists(produced_wav64)) {
				if (cfg.verbose >= 1) verbose(1, "[audioconv64] %s", out.c_str());
				if (want_prog) fprintf(stderr, "\n");
				fatal("audioconv64 did not produce output: %s", produced_wav64.c_str());
			}
		}

		remove(out_wav64.c_str());
		if (rename(produced_wav64.c_str(), out_wav64.c_str()) != 0) {
			fatal("Failed to rename audio output: %s -> %s (%s)",
				produced_wav64.c_str(), out_wav64.c_str(), strerror(errno));
		}

		verbose(1, "Output audio: %s", out_wav64.c_str());
		ar.produced = true;
		ar.wav64_path = out_wav64;
		out_idx++;
	};

	// Container audio streams
	for (const auto &as : container_streams) {
		if (as.stream_index < 0) continue;
		verbose(1, "Audio track: stream=%d", as.stream_index);
		convert_one(true, as.stream_index, std::string());
	}

	// Extra CLI audio files
	for (const auto &af : cfg.audio_files) {
		verbose(1, "Audio file: %s", af.c_str());
		convert_one(false, -1, af);
	}

	return ar;
}


