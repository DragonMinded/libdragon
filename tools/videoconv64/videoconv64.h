/*
    videoconv64: convert video files to formats used by the Libdragon SDK
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <functional>
#include <string>
#include <vector>
#include <stdint.h>

struct Config {
	std::string input_file;
	std::string output_dir;          // directory only
	int verbose = 0;                 // repeatable -v, -vv, ...

	std::string codec = "mpeg1";
	int width = 320;
	double fps = 0.0;                // 0 => auto; >0 => forced
	std::string profile = "auto";
	int quality = 80;                // 0..100
	bool quick = false;
	bool progress = true;
	std::string deinterlace = "auto";
	// TODO: decide how to expose automatic anamorphic PAR (non-square pixels) in the CLI.
	// For now, this is an internal toggle (default on).
	bool par_auto = true;
	bool audio = true;
	std::string audio_compress;       // passed to audioconv64 --wav-compress (no validation)
	int audio_rate = 32000;           // passed to audioconv64 --wav-resample
	int audio_channels = 1;           // 1 => --wav-mono, 2 => no flag
	std::string quant_matrix = "n64";

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

// Shared small utilities (implemented in vconv_utils.cpp)
int64_t now_ms(void);
std::string temp_dir(void);
std::string join_path(const std::string& dir, const std::string& file);
std::string base_name(const std::string& path);
std::string strip_ext(const std::string& name);
std::string format_cmdline_for_log(const std::vector<std::string>& argv);
void sleep_ms(int ms);

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

// Build an ffmpeg -vf chain for decoding/processing/encoding.
// out_matrix: "bt601" or "bt709"
// out_range:  "tv" (limited) or "pc" (full)
std::string build_filterchain(const AnalysisResult &ar, const char *out_matrix, const char *out_range);
void progressbar_clear(void);
int run_ffmpeg_with_progress(const std::vector<std::string>& argv, double duration_sec, int pass_idx, progress_state_t &ps);

EncodeResult vconv_encode_mpeg1(const CodecInfo &ci, const AnalysisResult &ar);
EncodeResult vconv_encode_h264(const CodecInfo &ci, const AnalysisResult &ar);

// Audio bridge
struct AudioResult {
	bool produced = false;
	std::string wav64_path;
};

AudioResult vconv_audio_bridge(void);


