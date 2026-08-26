/*
    videoconv64: convert video files to formats used by the Libdragon SDK
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <functional>
#include <string>
#include <vector>
#include <stdint.h>

// Included here so that every module gets it. Among other things, it provides
// a rename() that replaces the destination file, as the Windows one fails if
// the destination already exists.
#include "../common/polyfill.h"

struct Config {
	std::string input_file;
	std::string output_dir;          // directory only
	int verbose = 0;                 // repeatable -v, -vv, ...
	std::vector<std::string> extra_files;    // extra positional files after input_file (classified via ffprobe)
	std::vector<std::string> subtitle_files; // extra subtitle files (after classification)
	std::vector<std::string> audio_files;    // extra audio files (after classification)

	std::string codec = "mpeg1";
	int width = 320;
	double fps = 0.0;                // 0 => auto; >0 => forced
	std::string profile = "auto";
	int quality = 80;                // 0..100
	bool quick = false;
	bool debug_weightp = false;     // hidden debug toggle: enable H.264 weightp
	bool progress = true;
	std::string deinterlace = "auto";
	// TODO: decide how to expose automatic anamorphic PAR (non-square pixels) in the CLI.
	// For now, this is an internal toggle (default on).
	bool par_auto = true;
	bool audio = true;
	bool seek = false;               // generate .seek file with IDR/I-frame offsets (opt-in)
	double seek_interval_sec = 0.0;  // if >0, request keyframes every N seconds (converted to -g)
	std::string seek_frames_file;    // if set, text file with frame indices that must be keyframes
	std::vector<int> seek_frames;    // parsed contents of seek_frames_file (sorted, unique)
	std::string audio_compress;       // passed to audioconv64 --wav-compress (no validation)
	int audio_rate = 32000;           // passed to audioconv64 --wav-resample
	int audio_channels = 1;           // 1 => --wav-mono, 2 => no flag
	std::string quant_matrix = "n64";
	std::vector<std::string> ffmpeg_opts; // extra ffmpeg argv tokens, appended near output (repeatable --ffmpeg-opts; each value may contain multiple space-separated args)

	std::string ffmpeg_path = "ffmpeg";
	std::string ffprobe_path = "ffprobe";
};

extern Config cfg;

struct CodecInfo {
	const char *name;
	const char *default_ext;  // output extension (auto-derived)
	int align_w;              // width alignment requirement (pixels)
	int align_h;              // height alignment requirement (pixels)
};

__attribute__((format(printf, 2, 3)))
void verbose(int level, const char *str, ...);

__attribute__((noreturn, format(printf, 1, 2)))
void fatal(const char *str, ...);

// Track an output file that must be deleted if the conversion fails. Without
// this, a partial (or stale) file left behind would make build systems believe
// that the conversion succeeded, and they would not run it again.
void artifact_register(const std::string& path);

// Declare all registered artifacts as final: from now on they will survive
// even if the process exits with an error.
void artifact_commit_all(void);

// Shared small utilities (implemented in vconv_utils.cpp)
int64_t now_ms(void);
std::string temp_dir(void);
std::string join_path(const std::string& dir, const std::string& file);
std::string base_name(const std::string& path);
std::string strip_ext(const std::string& name);
std::string format_cmdline_for_log(const std::vector<std::string>& argv);
std::vector<std::string> split_shell_args(const std::string& s);
void sleep_ms(int ms);

// Build ffmpeg -force_key_frames argument from a list of frame indices.
// Each index is converted to a timestamp via t = frame / fps.
std::string ffmpeg_force_keyframes_from_frames(const std::vector<int>& frames, double fps);

// Build ffmpeg args to control keyframe placement based on cfg.seek_interval_sec / cfg.seek_frames.
// Emits:
// - "-g <N>" only if cfg.seek_interval_sec > 0 (N = round(cfg.seek_interval_sec * fps))
// - "-force_key_frames <t0,t1,...>" if cfg.seek_frames is set
std::vector<std::string> ffmpeg_keyframe_args(double fps);

// Shared progress helpers (implemented in vconv_utils.cpp).
void progressbar_clear(void);
void progressbar_infinite_update(int sec);
void progressbar_update(double overall_pct, double eta_sec);
typedef enum {
	PROGRESS_MODE_VIDEO = 0,
	PROGRESS_MODE_AUDIO = 1,
	PROGRESS_MODE_VIDEO_AUDIO = 2,
} progress_mode_t;
void progressbar_set_mode(progress_mode_t mode);
const char* progressbar_get_mode_label(void);

// Streaming process runner:
// - runs argv with combined stdout/stderr
// - optionally appends raw output to out
// - calls cb for each output line (without trailing newline)
int run_process_pipe(
	const std::vector<std::string>& argv,
	std::string *out,
	const std::function<void(const std::string&)> &cb
);

// Convenience wrapper: capture combined stdout/stderr into out and return exit code.
int run_process(const std::vector<std::string>& argv, std::string &out);

void check_tool_available(const std::string& tool_path, const char *tool_name);

// Analysis results
struct SourceMeta {
	int width = 0;
	int height = 0;
	double par = 1.0;          // pixel aspect ratio
	double fps = 0.0;          // avg_frame_rate
	double duration = 0.0;     // seconds, may be 0 if unknown
	std::string pix_fmt;
	std::string color_space;      // eg: "bt709", "smpte170m", "bt470bg", ...
	std::string color_range;      // eg: "tv" (limited), "pc" (full)
	std::string color_primaries;  // eg: "bt709"
	std::string color_transfer;   // eg: "bt709"
};

struct AnalysisMetrics {
	bool interlaced = false;
	int tff = 0;
	int bff = 0;
	int progressive = 0;
	int undetermined = 0;

	double diff_mean = 0.0;       // motion/noise proxy
	double diff_p95 = 0.0;
	double flat_mean = 0.0;       // flatness proxy
	double flat_p95 = 0.0;
};

struct AnalysisResult {
	SourceMeta meta;
	AnalysisMetrics metrics;

	int out_width = 0;
	int out_height = 0;
	double out_par = 1.0;        // output pixel aspect ratio (PAR)
	int sar_num = 1;             // PAR as rational (NUM/DEN) to embed into bitstreams
	int sar_den = 1;
	double out_fps = 0.0;
	std::string selected_profile; // resolved profile (auto -> actual)
};

AnalysisResult vconv_analyze(const CodecInfo &ci);

// Encoding
struct progress_state_t {
	int pass_count;
	int64_t start_ms;
	int64_t last_draw_ms;
};

struct EncodeResult {
	std::string video_path;  // produced .m1v
	std::string vf_used;     // exact -vf used for encoding (for metrics/debug)
};

struct seek_point_t {
	uint32_t offset;
	uint32_t frame;
};

// Build an ffmpeg -vf chain for decoding/processing/encoding.
// out_matrix: "bt601" or "bt709"
// out_range:  "tv" (limited) or "pc" (full)
std::string build_filterchain(const AnalysisResult &ar, const char *out_matrix, const char *out_range);
int run_ffmpeg_with_progress(const std::vector<std::string>& argv, double duration_sec, int pass_idx, progress_state_t &ps);

EncodeResult vconv_encode_mpeg1(const CodecInfo &ci, const AnalysisResult &ar);
EncodeResult vconv_encode_h264(const CodecInfo &ci, const AnalysisResult &ar);

// Optional post-processing: generate a .seek sidecar file for the produced elementary stream.
std::vector<seek_point_t> vconv_generate_seek(const CodecInfo &ci, const std::string &video_path);

// Subtitles (SUB64)
// Convert subtitles (from container tracks and/or extra CLI subtitle files) to .sub64 sidecars.
// Best-effort: if no subtitles are found, does nothing.
void vconv_process_subtitles(const AnalysisResult &ar);

// Audio bridge
struct AudioResult {
	bool produced = false;
	std::string wav64_path;
};

AudioResult vconv_audio_bridge(std::vector<seek_point_t> seek_points = {}, double video_fps = 0.0, bool show_progress = false);


