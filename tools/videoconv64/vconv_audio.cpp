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

#define DR_WAV_IMPLEMENTATION
#include "../common/dr_wav.h"

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

static void inject_seekpoints_cue_into_wav(const std::string& wav_path, const std::vector<seek_point_t>& seek_points, double video_fps) {
	assert(video_fps > 0.0);

	// Open input WAV and stream-copy audio samples to a new WAV with CUE markers.
	drwav in;
	if (!drwav_init_file(&in, wav_path.c_str(), NULL)) {
		fatal("failed to open extracted wav: %s", wav_path.c_str());
	}
	if (in.translatedFormatTag != DR_WAVE_FORMAT_PCM || in.bitsPerSample != 16) {
		drwav_uninit(&in);
		fatal("unexpected WAV format (expected pcm_s16): %s", wav_path.c_str());
	}

	// Compute audio seek points in *samples* (PCM frames), assuming frame_idx maps to time via fps.
	// We will store them as SMPL "loops" (loop points) so audioconv64 can reuse them.
	std::vector<drwav_uint32> sp;
	sp.reserve(seek_points.size());
	for (const auto& p : seek_points) {
		// Map video frame index -> seconds -> audio sample index.
		double t = (double)p.frame / video_fps;
		double s = t * (double)in.sampleRate;
		int64_t si = (int64_t)floor(s + 0.5);
		if (si < 0) si = 0;
		if (si > (int64_t)in.totalPCMFrameCount) si = (int64_t)in.totalPCMFrameCount;
		// Avoid pushing 0; it's implicit and usually not useful as a seekpoint.
		if (si == 0) continue;
		sp.push_back((drwav_uint32)si);
	}
	std::sort(sp.begin(), sp.end());
	sp.erase(std::unique(sp.begin(), sp.end()), sp.end());
	if (sp.empty()) {
		drwav_uninit(&in);
		return;
	}

	// Build CUE metadata, one cue point per seek point.
	std::vector<drwav_cue_point> cue_points;
	cue_points.resize(sp.size());
	for (size_t i = 0; i < sp.size(); i++) {
		drwav_cue_point &cp = cue_points[i];
		cp.id = (drwav_uint32)(i + 1);
		cp.playOrderPosition = 0;
		cp.dataChunkId[0] = 'd';
		cp.dataChunkId[1] = 'a';
		cp.dataChunkId[2] = 't';
		cp.dataChunkId[3] = 'a';
		cp.chunkStart = 0;
		cp.blockStart = 0;
		cp.sampleOffset = sp[i];
	}

	drwav_metadata md{};
	md.type = drwav_metadata_type_cue;
	md.data.cue.cuePointCount = (drwav_uint32)cue_points.size();
	md.data.cue.pCuePoints = cue_points.data();

	std::string tmp = wav_path + ".cue.wav";
	drwav_data_format fmt{};
	fmt.container = drwav_container_riff;
	fmt.format = DR_WAVE_FORMAT_PCM;
	fmt.channels = in.channels;
	fmt.sampleRate = in.sampleRate;
	fmt.bitsPerSample = in.bitsPerSample;

	// Write output.
	FILE *outf = fopen(tmp.c_str(), "wb");
	if (!outf) {
		drwav_uninit(&in);
		fatal("cannot create temp wav: %s", tmp.c_str());
	}
	auto onWrite = [](void* pUserData, const void* pData, size_t bytesToWrite) -> size_t {
		return fwrite(pData, 1, bytesToWrite, (FILE*)pUserData);
	};
	auto onSeek = [](void* pUserData, int offset, drwav_seek_origin origin) -> drwav_bool32 {
		int whence = (origin == DRWAV_SEEK_SET) ? SEEK_SET : SEEK_CUR;
		return fseek((FILE*)pUserData, offset, whence) == 0;
	};

	drwav out;
	if (!drwav_init_write_with_metadata(&out, &fmt, onWrite, onSeek, outf, NULL, &md, 1)) {
		fclose(outf);
		drwav_uninit(&in);
		remove(tmp.c_str());
		fatal("failed to init wav writer with metadata");
	}

	// Stream copy PCM frames.
	std::vector<drwav_int16> buf;
	buf.resize(4096 * fmt.channels);
	drwav_uint64 frames_left = in.totalPCMFrameCount;
	while (frames_left > 0) {
		drwav_uint64 chunk = frames_left > 4096 ? 4096 : frames_left;
		drwav_uint64 got = drwav_read_pcm_frames_s16(&in, chunk, buf.data());
		if (got == 0) break;
		drwav_write_pcm_frames(&out, got, buf.data());
		frames_left -= got;
	}

	drwav_uninit(&out);
	fclose(outf);
	drwav_uninit(&in);

	// Replace original wav.
	remove(wav_path.c_str());
	if (rename(tmp.c_str(), wav_path.c_str()) != 0) {
		remove(tmp.c_str());
		fatal("failed to replace wav with smpl-injected wav: %s", wav_path.c_str());
	}

	verbose(1, "Injected %d audio seek points into WAV CUE (fps=%.3f, sr=%u)", (int)sp.size(), video_fps, (unsigned)fmt.sampleRate);
}

AudioResult vconv_audio_bridge(std::vector<seek_point_t> seek_points, double video_fps, bool show_progress) {
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
			// Make the extracted WAV match audioconv64's target params so seek points are stable.
			"-ar", std::to_string(VCONV_AUDIO_EXTRACT_RATE),
			"-ac", std::to_string(VCONV_AUDIO_EXTRACT_CHANNELS),
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

	// If we have seek points, inject them into the WAV as SMPL metadata before running audioconv64.
	if (!seek_points.empty()) {
		inject_seekpoints_cue_into_wav(tmpwav, seek_points, video_fps);
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

		// If we want determinate audio progress, run audioconv64 in verbose mode and parse its
		// "Resampling: <bytes> (xx.x%)" lines.
		const bool want_prog = show_progress && cfg.progress && cfg.verbose == 0;
		if (want_prog) cmd.insert(cmd.begin() + 1, "-v");

		double last_pct = -1.0;
		std::string out;
		int rc = run_process_pipe(cmd, &out, [&](const std::string& line) {
			if (!want_prog) return;
			const char *prefix = "Resampling:";
			if (line.rfind(prefix, 0) != 0) return;
			// Parse "... (XX.X%)"
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

		// Do not print a newline here: video/audio phases share the same terminal line.
		// main() will print a single final newline at the end of the whole process.
		if (want_prog) { progressbar_clear(); }

		remove(tmpwav.c_str());
		// audioconv64 currently returns 0 even on some errors; verify output exists.
		if (rc != 0 || !file_exists(produced_wav64)) {
			if (cfg.verbose >= 1) {
				verbose(1, "[audioconv64] %s", out.c_str());
			}
			if (want_prog) fprintf(stderr, "\n");
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


