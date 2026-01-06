/*
    videoconv64 encoding module (shared)
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include <string.h>
#include <math.h>

#include <map>
#include <deque>
#include <algorithm>

static double quality_strength(double q) {
	// strength in [0..1], where 0 => minimal processing (high quality),
	// 1 => stronger cleanup (low quality).
	if (q < 0.0) q = 0.0;
	if (q > 100.0) q = 100.0;
	return (100.0 - q) / 100.0;
}

static double clamp_double(double v, double lo, double hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static std::string ffmpeg_scale_in_matrix_from_ffprobe(const std::string& cs) {
	// Map ffprobe stream.color_space to scale's in_color_matrix option.
	// We keep this intentionally conservative: return empty if unknown.
	if (cs == "bt709") return "bt709";
	// BT.601 family (common names in FFmpeg): smpte170m (NTSC), bt470bg (PAL)
	if (cs == "smpte170m" || cs == "bt470bg" || cs == "bt601") return "bt601";
	return std::string();
}

static std::string ffmpeg_scale_in_range_from_ffprobe(const std::string& r) {
	// ffprobe stream.color_range: "tv" (limited), "pc" (full)
	if (r == "tv" || r == "pc") return r;
	return std::string();
}

std::string build_filterchain(const AnalysisResult &ar, const char *out_matrix, const char *out_range) {
	std::vector<std::string> filters;

	// Deinterlace only if detected interlaced, unless user forced on/off.
	if (cfg.deinterlace == "on") {
		filters.push_back("bwdif=deint=all:parity=auto");
	} else if (cfg.deinterlace != "off" && ar.metrics.interlaced) {
		filters.push_back("bwdif=deint=interlaced:parity=auto");
	}

	{
		// Scaling is often the heaviest part of the pipeline. In quick mode, use a cheaper scaler.
		// We also enforce output matrix/range here so we get a real conversion (not just tagging).
		const char *scaler = cfg.quick ? "bicubic" : "lanczos";
		std::string f = "scale=" + std::to_string(ar.out_width) + ":" + std::to_string(ar.out_height) + ":flags=" + scaler;
		std::string in_mtx = ffmpeg_scale_in_matrix_from_ffprobe(ar.meta.color_space);
		std::string in_rng = ffmpeg_scale_in_range_from_ffprobe(ar.meta.color_range);
		if (!in_mtx.empty()) f += ":in_color_matrix=" + in_mtx;
		if (!in_rng.empty()) f += ":in_range=" + in_rng;
		f += std::string(":out_color_matrix=") + out_matrix;
		f += std::string(":out_range=") + out_range;
		filters.push_back(f);
	}

	{
		// Always set fps: either user forced it, or we picked a sane value in analysis.
		char buf[64];
		snprintf(buf, sizeof(buf), "fps=%.3f", ar.out_fps);
		filters.push_back(buf);
	}

	filters.push_back("format=yuv420p");

	// In quick mode we skip expensive cleanup filters (denoise/deband/sharpen) to maximize speed.
	if (cfg.quick) {
		std::string vf;
		for (size_t i = 0; i < filters.size(); i++) {
			if (i) vf += ",";
			vf += filters[i];
		}
		return vf;
	}

	// profile=none disables only the profile-specific cleanup filters below (denoise/deband/sharpen).
	// Deinterlacing is still controlled solely by --deinterlace.
	if (ar.selected_profile == "none") {
		std::string vf;
		for (size_t i = 0; i < filters.size(); i++) {
			if (i) vf += ",";
			vf += filters[i];
		}
		return vf;
	}

	const double s = quality_strength((double)cfg.quality);

	// Content-dependent tweaks
	if (ar.selected_profile == "cartoon") {
		// Denoise for cartoons should be mostly spatial: temporal denoise can easily create
		// smearing/ghosting on moving line art. Keep temporal very low.
		{
			double l = clamp_double(0.35 + 0.70 * s, 0.2, 1.2);
			double c = clamp_double(0.35 + 0.70 * s, 0.2, 1.2);
			double lt = clamp_double(0.0 + 2.0 * s, 0.0, 3.0);
			double ct = clamp_double(0.0 + 2.0 * s, 0.0, 3.0);
			char buf[128];
			snprintf(buf, sizeof(buf), "hqdn3d=%.2f:%.2f:%.0f:%.0f", l, c, lt, ct);
			filters.push_back(buf);
		}
		// Debanding is important for cartoons/anime (flat areas).
		{
			// deband thresholds are in [3e-05..0.5]. Scale gently with s.
			double t = clamp_double(0.018 + 0.030 * s, 0.010, 0.060);
			char buf[256];
			snprintf(buf, sizeof(buf),
				"deband=1thr=%.4f:2thr=%.4f:3thr=%.4f:range=16:blur=1:coupling=0",
				t, t, t);
			filters.push_back(buf);
		}
		{
			// Be conservative with sharpening on line art to avoid halos/ringing.
			double amt = clamp_double(0.10 + 0.20 * (1.0 - s), 0.08, 0.35);
			char buf[64];
			snprintf(buf, sizeof(buf), "unsharp=3:3:%.2f:3:3:0.0", amt);
			filters.push_back(buf);
		}
	} else if (ar.selected_profile == "noisy") {
		{
			double l = clamp_double(1.0 + 1.6 * s, 0.6, 3.0);
			double c = clamp_double(1.0 + 1.6 * s, 0.6, 3.0);
			double lt = clamp_double(4.0 + 8.0 * s, 0.0, 16.0);
			double ct = clamp_double(4.0 + 8.0 * s, 0.0, 16.0);
			char buf[128];
			snprintf(buf, sizeof(buf), "hqdn3d=%.2f:%.2f:%.0f:%.0f", l, c, lt, ct);
			filters.push_back(buf);
		}
		{
			double amt = clamp_double(0.10 + 0.18 * (1.0 - s), 0.05, 0.30);
			char buf[64];
			snprintf(buf, sizeof(buf), "unsharp=5:5:%.2f:3:3:0.0", amt);
			filters.push_back(buf);
		}
	} else {
		{
			double l = clamp_double(0.6 + 1.0 * s, 0.3, 2.0);
			double c = clamp_double(0.6 + 1.0 * s, 0.3, 2.0);
			double lt = clamp_double(3.0 + 7.0 * s, 0.0, 16.0);
			double ct = clamp_double(3.0 + 7.0 * s, 0.0, 16.0);
			char buf[128];
			snprintf(buf, sizeof(buf), "hqdn3d=%.2f:%.2f:%.0f:%.0f", l, c, lt, ct);
			filters.push_back(buf);
		}
		// gradfun reduces banding; second parameter is radius (must be 4..32)
		{
			int thr = (int)(5 + 7 * s + 0.5); // 5..12
			char buf[64];
			snprintf(buf, sizeof(buf), "gradfun=%d:16", thr);
			filters.push_back(buf);
		}
		{
			double amt = clamp_double(0.12 + 0.23 * (1.0 - s), 0.08, 0.35);
			char buf[64];
			snprintf(buf, sizeof(buf), "unsharp=5:5:%.2f:3:3:0.0", amt);
			filters.push_back(buf);
		}
	}

	std::string vf;
	for (size_t i = 0; i < filters.size(); i++) {
		if (i) vf += ",";
		vf += filters[i];
	}
	return vf;
}

std::string ffmpeg_force_keyframes_from_frames(const std::vector<int>& frames, double fps)
{
	if (frames.empty() || fps <= 0.0) return std::string();

	// Ensure monotonic unique list (callers generally already do this).
	std::vector<int> f = frames;
	std::sort(f.begin(), f.end());
	f.erase(std::unique(f.begin(), f.end()), f.end());

	std::string out;
	out.reserve(f.size() * 8);
	for (size_t i = 0; i < f.size(); i++) {
		if (f[i] < 0) continue;
		double t = (double)f[i] / fps;
		char buf[64];
		// Use fixed decimal seconds; ffmpeg accepts this as timestamp.
		snprintf(buf, sizeof(buf), "%.6f", t);
		if (!out.empty()) out.push_back(',');
		out += buf;
	}
	return out;
}

std::vector<std::string> ffmpeg_keyframe_args(double fps)
{
	std::vector<std::string> args;

	// GOP size: if user provided --seek <seconds>, convert it to a GOP in frames
	// and emit it as -g <frames>. If no seek interval is provided, do not emit -g.
	if (cfg.seek_interval_sec > 0.0) {
		int gop = (int)floor(cfg.seek_interval_sec * fps + 0.5);
		if (gop < 1) gop = 1;
		args.push_back("-g");
		args.push_back(std::to_string(gop));
	}

	// Forced keyframes: list of required I/IDR frames provided by the user.
	if (!cfg.seek_frames.empty()) {
		std::string fkf = ffmpeg_force_keyframes_from_frames(cfg.seek_frames, fps);
		if (!fkf.empty()) {
			args.push_back("-force_key_frames");
			args.push_back(fkf);
		}
	}

	return args;
}

static void parse_progress_kv_line(const std::string& line, std::map<std::string,std::string> &kv) {
	size_t eq = line.find('=');
	if (eq == std::string::npos) return;
	std::string k = line.substr(0, eq);
	std::string v = line.substr(eq + 1);
	kv[k] = v;
}

static void report_progress(const std::map<std::string,std::string> &kv, double duration_sec) {
	if (!cfg.progress) return;
	// ffmpeg outputs out_time_ms, out_time, frame, fps, speed, progress=continue|end
	double pct = -1.0;
	if (duration_sec > 0.0) {
		auto it = kv.find("out_time_ms");
		if (it != kv.end()) {
			double out_ms = atof(it->second.c_str());
			pct = (out_ms / 1000000.0) / duration_sec * 100.0;
			if (pct < 0) pct = 0;
			if (pct > 100) pct = 100;
		}
	}

	auto it_time = kv.find("out_time");
	auto it_speed = kv.find("speed");
	auto it_fps = kv.find("fps");
	const char *speed = (it_speed != kv.end()) ? it_speed->second.c_str() : "?";
	const char *fps = (it_fps != kv.end()) ? it_fps->second.c_str() : "?";

	if (pct >= 0.0) {
		verbose(1, "progress: %.1f%% time=%s fps=%s speed=%s",
			pct,
			it_time != kv.end() ? it_time->second.c_str() : "?",
			fps,
			speed);
	} else {
		verbose(1, "progress: time=%s fps=%s speed=%s",
			it_time != kv.end() ? it_time->second.c_str() : "?",
			fps,
			speed);
	}
}

static void progress_unknown_update(const std::map<std::string,std::string> &kv, const progress_state_t &ps) {
	// When duration is unknown, show the shared infinite progress bar animation.
	// Prefer out_time_ms (microseconds) if present; fallback to out_time (HH:MM:SS.micro)
	int sec = 0;
	auto it_ms = kv.find("out_time_ms");
	if (it_ms != kv.end()) {
		double out_us = atof(it_ms->second.c_str());
		sec = (int)((out_us / 1000000.0) + 0.5);
	} else {
		auto it_t = kv.find("out_time");
		if (it_t != kv.end()) {
			int hh = 0, mm = 0;
			double ss = 0.0;
			if (sscanf(it_t->second.c_str(), "%d:%d:%lf", &hh, &mm, &ss) == 3) {
				sec = hh * 3600 + mm * 60 + (int)(ss + 0.5);
			}
		}
	}

	(void)ps;
	progressbar_infinite_update(sec);
}

struct ffmpeg_progress_ctx_t {
	double duration_sec;
	int pass_idx;
	progress_state_t *ps;

	std::map<std::string,std::string> kv;
	std::deque<std::string> tail_lines;
	double last_overall_pct;
};

int run_ffmpeg_with_progress(const std::vector<std::string>& argv, double duration_sec, int pass_idx, progress_state_t &ps) {
	std::string cmdline = format_cmdline_for_log(argv);

	ffmpeg_progress_ctx_t ctx;
	ctx.duration_sec = duration_sec;
	ctx.pass_idx = pass_idx;
	ctx.ps = &ps;
	ctx.last_overall_pct = -1.0;

	int rc = run_process_pipe(argv, NULL, [&](const std::string& line) {
		// Always keep a tail of lines (used on error).
		const size_t MAX_TAIL_LINES = 80;
		if (ctx.tail_lines.size() >= MAX_TAIL_LINES) ctx.tail_lines.pop_front();
		ctx.tail_lines.push_back(line);

		parse_progress_kv_line(line, ctx.kv);
		if (line == "progress=continue") {
			if (cfg.verbose >= 1) {
				report_progress(ctx.kv, ctx.duration_sec);
			} else {
				double pct = -1.0;
				if (ctx.duration_sec > 0.0) {
					auto it = ctx.kv.find("out_time_ms");
					if (it != ctx.kv.end()) {
						double out_ms = atof(it->second.c_str());
						pct = (out_ms / 1000000.0) / ctx.duration_sec * 100.0;
					}
				}
				if (pct >= 0.0) {
					double overall = ((double)ctx.pass_idx + (pct / 100.0)) / (double)ctx.ps->pass_count * 100.0;
					int64_t now = now_ms();
					if ((now - ctx.ps->last_draw_ms) >= 200 || ctx.last_overall_pct < 0.0) {
						ctx.ps->last_draw_ms = now;
						ctx.last_overall_pct = overall;
						double eta = 0.0;
						if (overall > 0.5) {
							double elapsed = (double)(now - ctx.ps->start_ms) / 1000.0;
							eta = elapsed * (100.0 - overall) / overall;
						}
						progressbar_update(overall, eta);
					}
				} else if (ctx.duration_sec <= 0.0) {
					int64_t now = now_ms();
					if ((now - ctx.ps->last_draw_ms) >= 200) {
						ctx.ps->last_draw_ms = now;
						progress_unknown_update(ctx.kv, *ctx.ps);
					}
				}
			}
			ctx.kv.clear();
		} else if (line == "progress=end") {
			if (cfg.verbose >= 1) {
				report_progress(ctx.kv, ctx.duration_sec);
			} else {
				if (ctx.duration_sec > 0.0) {
					double overall = ((double)(ctx.pass_idx + 1) / (double)ctx.ps->pass_count) * 100.0;
					int64_t now = now_ms();
					ctx.ps->last_draw_ms = now;
					ctx.last_overall_pct = overall;
					progressbar_update(overall, 0.0);
				} else {
					// Final update for unknown-duration streams.
					progress_unknown_update(ctx.kv, *ctx.ps);
				}
			}
			ctx.kv.clear();
		}
	});

	if (rc != 0 && cfg.verbose >= 1) {
		verbose(1, "[exec] %s", cmdline.c_str());
		verbose(1, "[exit] rc=%d", rc);
		for (const auto& l : ctx.tail_lines)
			verbose(1, "[ffmpeg] %s", l.c_str());
	} else {
		if (cfg.verbose >= 2) verbose(2, "[exit] rc=%d", rc);
	}

	return rc;
}


