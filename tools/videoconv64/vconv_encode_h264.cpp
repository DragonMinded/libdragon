/*
    videoconv64 encoding module (H.264)
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include <math.h>

static int clamp_int(int v, int lo, int hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static int quality_to_h264_crf(int q) {
	// Must satisfy: q=80 -> CRF 24, q=100 -> CRF 14
	//
	// Use a sqrt curve so quality doesn't drop too fast in the mid range:
	//   crf = 14 + k*sqrt(100-q), with k chosen so q=80 => +10.
	// This also yields q=0 => ~36 (reasonable worst).
	q = clamp_int(q, 0, 100);
	double k = 10.0 / sqrt(20.0); // ~= 2.2360679
	int crf = (int)(14.0 + k * sqrt((double)(100 - q)) + 0.5);
	return clamp_int(crf, 14, 36);
}

static std::string make_output_video_path(const CodecInfo &ci) {
	std::string name = strip_ext(base_name(cfg.input_file)) + ci.default_ext;
	if (cfg.output_dir.empty()) return name;
	return join_path(cfg.output_dir, name);
}

EncodeResult vconv_encode_h264(const CodecInfo &ci, const AnalysisResult &ar) {
	EncodeResult er;
	er.video_path = make_output_video_path(ci);

	std::string vf = build_filterchain(ar);
	er.vf_used = vf;

	int crf = quality_to_h264_crf(cfg.quality);
	const int maxrate_kbps = 400;
	const int bufsize_kbps = 800;

	verbose(1, "H.264 quality=%d -> crf=%d maxrate=%d kbps bufsize=%d kbps", cfg.quality, crf, maxrate_kbps, bufsize_kbps);

	std::vector<std::string> cmd = {
		cfg.ffmpeg_path,
		"-hide_banner",
		"-nostats",
		"-y",
		"-i", cfg.input_file,
		"-an",
		"-vf", vf,
		"-c:v", "libx264",
		"-profile:v", "baseline",
		"-pix_fmt", "yuv420p",
		"-crf", std::to_string(crf),
		"-maxrate", std::to_string(maxrate_kbps) + "k",
		"-bufsize", std::to_string(bufsize_kbps) + "k",
		"-bf", "0",
		"-g", std::to_string((int)(ar.out_fps + 0.5)),
		"-preset", cfg.quick ? "veryfast" : "slower",
		"-x264-params", "no-deblock=1:no-info=1",
		"-f", "h264",
		"-progress", "pipe:1",
		"-v", "error",
	};

	// Signal SAR in VUI using ffmpeg's dedicated option (more portable than x264-params).
	if (ar.sar_num != 1 || ar.sar_den != 1) {
		const std::string sar = std::to_string(ar.sar_num) + ":" + std::to_string(ar.sar_den);
        cmd.push_back("-sar");
        cmd.push_back(sar);
		verbose(1, "H.264: signaling SAR %s", sar.c_str());
	}

    cmd.push_back(er.video_path);

	progress_state_t ps = { .pass_count = 1, .start_ms = now_ms(), .last_draw_ms = 0 };
	int rc = run_ffmpeg_with_progress(cmd, ar.meta.duration, 0, ps);
	if (cfg.verbose == 0 && cfg.progress) { progressbar_clear(); fprintf(stderr, "\n"); }
	if (rc != 0) fatal("ffmpeg failed (rc=%d)", rc);
	return er;
}


