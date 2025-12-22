/*
    videoconv64 encoding module (MPEG-1)
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

static int clamp_int(int v, int lo, int hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static int quality_to_mpeg1_bitrate_kbps(int q) {
	// Must satisfy: q=0 -> 200 kbps, q=80 -> 800 kbps, q=100 -> 950 kbps
	// Use a curve that drops faster in the mid range than linear:
	//   f(x) = x + k*x*(1-x)*(x-0.8)   with x=q/100, k>0
	// Term is 0 at x=0, x=0.8, x=1 so anchors remain stable.
	q = clamp_int(q, 0, 100);
	double x = (double)q / 100.0;
	double k = 1.5;
	double f = x + k * x * (1.0 - x) * (x - 0.8);
	if (f < 0.0) f = 0.0;
	if (f > 1.0) f = 1.0;
	int br = (int)(200.0 + 750.0 * f + 0.5);
	return clamp_int(br, 200, 950);
}

static int compute_bufsize_kbps(int bitrate_kbps) {
	// Internal knob: keep VBV fairly tight to avoid big bitrate spikes while streaming.
	// ~0.8s worth of data (tweakable later without changing the CLI).
	return (bitrate_kbps * 8) / 10;
}

static std::string make_output_video_path(const CodecInfo &ci) {
	std::string name = strip_ext(base_name(cfg.input_file)) + ci.default_ext;
	if (cfg.output_dir.empty()) return name;
	return join_path(cfg.output_dir, name);
}

static std::string make_passlog_prefix(void) {
	// Store 2-pass log files in a temporary directory (not in output dir).
	// Use time to avoid collisions across concurrent runs.
	char buf[64];
	snprintf(buf, sizeof(buf), "videoconv64_%lld", (long long)now_ms());
	return join_path(temp_dir(), std::string(buf));
}

static void cleanup_passlog(const std::string& passlog_prefix) {
	// ffmpeg usually produces: <prefix>-0.log and optionally <prefix>-0.log.mbtree
	// Remove best-effort; ignore errors.
	const std::string log0 = passlog_prefix + "-0.log";
	const std::string mbtree = passlog_prefix + "-0.log.mbtree";
	remove(log0.c_str());
	remove(mbtree.c_str());
}

EncodeResult vconv_encode_mpeg1(const CodecInfo &ci, const AnalysisResult &ar) {
	EncodeResult er;
	er.video_path = make_output_video_path(ci);

	std::string vf = build_filterchain(ar);
	er.vf_used = vf;
	int bitrate_kbps = quality_to_mpeg1_bitrate_kbps(cfg.quality);
	int buf_kbps = compute_bufsize_kbps(bitrate_kbps);
	verbose(1, "MPEG-1 quality=%d -> bitrate=%d kbps bufsize=%d kbps", cfg.quality, bitrate_kbps, buf_kbps);

	// Base arguments common to pass1/pass2.
	const char *trellis = cfg.quick ? "0" : (cfg.quality >= 70 ? "2" : (cfg.quality >= 40 ? "1" : "0"));
	std::vector<std::string> base = {
		cfg.ffmpeg_path,
		"-hide_banner",
		"-nostats",
		"-y",
		"-i", cfg.input_file,
		"-an",
		"-vf", vf,
		"-c:v", "mpeg1video",
		"-b:v", std::to_string(bitrate_kbps) + "k",
		"-maxrate", std::to_string(bitrate_kbps) + "k",
		"-bufsize", std::to_string(buf_kbps) + "k",
		"-bf", "2",
		"-g", std::to_string((int)(ar.out_fps + 0.5)),
		// Expensive encoder knobs: disable in quick mode.
		"-trellis", trellis,
	};

	// Extra RD effort only at high quality (keeps q~50 faster and a bit rougher).
	if (!cfg.quick && cfg.quality >= 70) {
		base.insert(base.end(), { "-mbd", "rd", "-cmp", "rd", "-subcmp", "rd" });
	}

	if (cfg.quant_matrix == "n64") {
		// Conservative intra matrix to push down high frequencies and reduce ringing/mosquito at low bitrate.
		base.push_back("-intra_matrix");
		base.push_back(
			"8,16,22,26,30,35,40,48,"
			"16,16,26,28,32,35,40,48,"
			"22,26,30,32,35,40,48,56,"
			"26,28,32,35,40,48,56,64,"
			"30,32,35,40,48,56,64,80,"
			"35,35,40,48,56,64,80,96,"
			"40,40,48,56,64,80,96,128,"
			"48,48,56,64,80,128,128,150"
		);
	}

	// Progress: use -progress pipe:1 so output is key=value lines.
	// duration_sec comes from ffprobe (may be 0).
	const double duration_sec = ar.meta.duration;

	if (cfg.quick) {
		progress_state_t ps = { .pass_count = 1, .start_ms = now_ms(), .last_draw_ms = 0 };
		std::vector<std::string> cmd = base;
		// Single-pass, avoid extra analysis passes; still provide machine-parseable progress.
		cmd.insert(cmd.end(), { "-progress", "pipe:1", "-v", "error" });
		cmd.push_back(er.video_path);
		int rc = run_ffmpeg_with_progress(cmd, duration_sec, 0, ps);
		if (rc != 0) fatal("ffmpeg failed (rc=%d)", rc);
		if (cfg.verbose == 0 && cfg.progress) { progressbar_clear(); fprintf(stderr, "\n"); }
		return er;
	}

	// 2-pass encode
	std::string passlog = make_passlog_prefix();
	progress_state_t ps = { .pass_count = 2, .start_ms = now_ms(), .last_draw_ms = 0 };

#ifdef _WIN32
	const char *null_sink = "NUL";
#else
	const char *null_sink = "/dev/null";
#endif

	{
		std::vector<std::string> cmd = base;
		cmd.insert(cmd.end(), { "-pass", "1", "-passlogfile", passlog, "-progress", "pipe:1", "-f", "null", null_sink, "-v", "error" });
		verbose(1, "Encoding pass 1...");
		int rc = run_ffmpeg_with_progress(cmd, duration_sec, 0, ps);
		if (rc != 0) {
			if (cfg.verbose == 0 && cfg.progress) { progressbar_clear(); fprintf(stderr, "\n"); }
			cleanup_passlog(passlog);
			fatal("ffmpeg pass 1 failed (rc=%d)", rc);
		}
	}
	{
		std::vector<std::string> cmd = base;
		cmd.insert(cmd.end(), { "-pass", "2", "-passlogfile", passlog, "-progress", "pipe:1", "-v", "error" });
		cmd.push_back(er.video_path);
		verbose(1, "Encoding pass 2...");
		int rc = run_ffmpeg_with_progress(cmd, duration_sec, 1, ps);
		if (rc != 0) {
			if (cfg.verbose == 0 && cfg.progress) { progressbar_clear(); fprintf(stderr, "\n"); }
			cleanup_passlog(passlog);
			fatal("ffmpeg pass 2 failed (rc=%d)", rc);
		}
	}

	cleanup_passlog(passlog);
	if (cfg.verbose == 0 && cfg.progress) { progressbar_clear(); fprintf(stderr, "\n"); }
	return er;
}


