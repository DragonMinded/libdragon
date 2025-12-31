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
	// Curve configuration
	const double CRF_Q100 = 14.0;   // CRF at q=100 (best)
	const double CRF_Q0   = 36.0;   // CRF at q=0   (worst)
	const double DECAY    = 0.5;    // Decay speed (0.5 = sqrt curve, 1.0 = linear)

	q = clamp_int(q, 0, 100);

	// Normalized inverted quality (0.0 at q=100, 1.0 at q=0)
	double x = (100 - q) / 100.0;

	// Apply power curve: crf = min + (max-min) * x^decay
	double crf = CRF_Q100 + (CRF_Q0 - CRF_Q100) * pow(x, DECAY);

	return clamp_int((int)(crf + 0.5), (int)CRF_Q100, (int)CRF_Q0);
}

static std::string make_output_video_path(const CodecInfo &ci) {
	std::string name = strip_ext(base_name(cfg.input_file)) + ci.default_ext;
	if (cfg.output_dir.empty()) return name;
	return join_path(cfg.output_dir, name);
}

EncodeResult vconv_encode_h264(const CodecInfo &ci, const AnalysisResult &ar) {
	EncodeResult er;
	er.video_path = make_output_video_path(ci);

	// H.264 output is always forced to BT.709 + Full range.
	std::string vf = build_filterchain(ar, "bt709", "pc");
	er.vf_used = vf;

	int crf = quality_to_h264_crf(cfg.quality);
	int maxrate_kbps = (int)floor(300.0 * 24 / ar.out_fps + 0.5);
	maxrate_kbps = clamp_int(maxrate_kbps, 50, 5000);
	const int bufsize_kbps = maxrate_kbps * 0.5;

	verbose(1, "H.264 quality=%d -> crf=%d maxrate=%d kbps bufsize=%d kbps (fps=%.3f)",
		cfg.quality, crf, maxrate_kbps, bufsize_kbps, ar.out_fps);

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
		// Ensure output bitstream advertises the intended colorspace too (VUI).
		"-colorspace", "bt709",
		"-color_range", "pc",
		"-color_primaries", "bt709",
		"-color_trc", "bt709",
		"-crf", std::to_string(crf),
		"-maxrate", std::to_string(maxrate_kbps) + "k",
		"-bufsize", std::to_string(bufsize_kbps) + "k",
		"-bf", "0",
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

	// Keyframe placement options (GOP size / forced keyframes).
	{
		std::vector<std::string> kf = ffmpeg_keyframe_args(ar.out_fps);
		cmd.insert(cmd.end(), kf.begin(), kf.end());
	}

    cmd.push_back(er.video_path);

	progress_state_t ps = { .pass_count = 1, .start_ms = now_ms(), .last_draw_ms = 0 };
	int rc = run_ffmpeg_with_progress(cmd, ar.meta.duration, 0, ps);
	// Do not print a newline here: next phases (eg: Audio) should reuse the same line.
	// A single final newline is printed by main() when the whole pipeline is done.
	if (cfg.verbose == 0 && cfg.progress) { progressbar_clear(); }
	if (rc != 0) fatal("ffmpeg failed (rc=%d)", rc);
	return er;
}


