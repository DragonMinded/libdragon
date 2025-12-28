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

#include <string>
#include <errno.h>
#include <string.h>

using json = nlohmann::json;

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

static bool has_audio_stream(void) {
	std::vector<std::string> cmd = {
		cfg.ffprobe_path,
		"-v", "error",
		"-select_streams", "a:0",
		"-show_entries", "stream=codec_type",
		"-of", "json",
		cfg.input_file,
	};

	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) return false;

	try {
		json j = json::parse(out);
		if (!j.contains("streams") || j["streams"].empty()) return false;
		return true;
	} catch (...) {
		return false;
	}
}

AudioResult vconv_audio_bridge(void) {
	AudioResult ar;
	if (!cfg.audio) return ar;

	if (!has_audio_stream()) {
		verbose(1, "No audio stream found; skipping audio conversion");
		return ar;
	}

	// Ensure audioconv64 exists if we need it.
	const std::string ac64 = audioconv64_path();
	{
		std::string out;
		int rc = run_process({ ac64, "--help" }, out);
		if (rc != 0) fatal("audioconv64 not found or not executable: %s", ac64.c_str());
	}

	// Extract audio to a temporary WAV file.
	std::string stem = strip_ext(base_name(cfg.input_file));
	// Use a unique basename so we can deterministically rename the final .wav64
	// to match the original input basename.
	std::string tmpwav = join_path(temp_dir(), "videoconv64_" + stem + "_" + std::to_string((long long)now_ms()) + ".wav");

	verbose(1, "Extracting audio...");
	{
		std::vector<std::string> cmd = {
			cfg.ffmpeg_path,
			"-hide_banner",
			"-nostats",
			"-y",
			"-v", "error",
			"-i", cfg.input_file,
			"-vn",
			"-c:a", "pcm_s16le",
			"-f", "wav",
			tmpwav,
		};
		std::string out;
		int rc = run_process(cmd, out);
		if (rc != 0) {
			remove(tmpwav.c_str());
			fatal("ffmpeg audio extraction failed (rc=%d)", rc);
		}
	}

	// Convert to wav64 using audioconv64. Output next to the video.
	verbose(1, "Converting audio with audioconv64...");
	std::string out_wav64 = join_path(effective_output_dir(), strip_ext(base_name(cfg.input_file)) + ".wav64");
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
		// Always pass these through; audioconv64 will validate ranges.
		cmd.push_back("--wav-resample");
		cmd.push_back(std::to_string(cfg.audio_rate));
		if (cfg.audio_channels == 1) {
			cmd.push_back("--wav-mono");
		}
		cmd.push_back(tmpwav);
		std::string out;
		int rc = run_process(cmd, out);
		remove(tmpwav.c_str());
		// audioconv64 currently returns 0 even on some errors; verify output exists.
		if (rc != 0 || !file_exists(produced_wav64)) {
			if (cfg.verbose >= 1) {
				verbose(1, "[audioconv64] %s", out.c_str());
			}
			fatal("audioconv64 did not produce output: %s", produced_wav64.c_str());
		}
	}

	// Rename final output to match input basename.
	remove(out_wav64.c_str()); // best-effort overwrite
	if (rename(produced_wav64.c_str(), out_wav64.c_str()) != 0) {
		fatal("Failed to rename audio output: %s -> %s (%s)",
			produced_wav64.c_str(),
			out_wav64.c_str(),
			strerror(errno));
	}

	ar.produced = true;
	ar.wav64_path = out_wav64;
	verbose(1, "Output audio: %s", ar.wav64_path.c_str());

	return ar;
}


