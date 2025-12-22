/*
    videoconv64 encoding module (shared)
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include <string.h>

#include <map>

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

std::string build_filterchain(const AnalysisResult &ar) {
	std::vector<std::string> filters;

	// Deinterlace only if detected interlaced, unless user forced on/off.
	if (cfg.deinterlace == "on") {
		filters.push_back("bwdif=deint=all:parity=auto");
	} else if (cfg.deinterlace != "off" && ar.metrics.interlaced) {
		filters.push_back("bwdif=deint=interlaced:parity=auto");
	}

	{
		// Scaling is often the heaviest part of the pipeline. In quick mode, use a cheaper scaler.
		char buf[128];
		const char *scaler = cfg.quick ? "bicubic" : "lanczos";
		snprintf(buf, sizeof(buf), "scale=%d:%d:flags=%s", ar.out_width, ar.out_height, scaler);
		filters.push_back(buf);
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

	const double s = quality_strength((double)cfg.quality);

	// Content-dependent tweaks
	if (ar.selected_profile == "cartoon") {
		// Luma denoise kept small for cartoons; scale with quality.
		{
			double l = clamp_double(0.35 + 0.70 * s, 0.2, 1.2);
			double c = clamp_double(0.35 + 0.70 * s, 0.2, 1.2);
			double lt = clamp_double(2.0 + 6.0 * s, 0.0, 12.0);
			double ct = clamp_double(2.0 + 6.0 * s, 0.0, 12.0);
			char buf[128];
			snprintf(buf, sizeof(buf), "hqdn3d=%.2f:%.2f:%.0f:%.0f", l, c, lt, ct);
			filters.push_back(buf);
		}
		// gradfun reduces banding; second parameter is radius (must be 4..32)
		{
			int thr = (int)(6 + 8 * s + 0.5); // 6..14
			char buf[64];
			snprintf(buf, sizeof(buf), "gradfun=%d:16", thr);
			filters.push_back(buf);
		}
		{
			double amt = clamp_double(0.15 + 0.35 * (1.0 - s), 0.10, 0.50);
			char buf[64];
			snprintf(buf, sizeof(buf), "unsharp=5:5:%.2f:3:3:0.0", amt);
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

static void format_eta_mmss(char out[8], double eta_sec) {
	if (!(eta_sec > 0.0) || eta_sec > 99 * 60 + 59) {
		strcpy(out, "--:--");
		return;
	}
	int sec = (int)(eta_sec + 0.5);
	int mm = sec / 60;
	int ss = sec % 60;
	snprintf(out, 8, "%02d:%02d", mm, ss);
}

static void progressbar_update(double overall_pct, double eta_sec) {
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

	char eta[8];
	format_eta_mmss(eta, eta_sec);

	// Example: [####-----]  42% ETA 01:23
	fprintf(stderr, "\r[%s] %6.1f%% ETA %s", bar, overall_pct, eta);
	fflush(stderr);
}

void progressbar_clear(void) {
	if (!cfg.progress) return;
	// Clear current line (best-effort, no ANSI). Print spaces and carriage return.
	fprintf(stderr, "\r%*s\r", 80, "");
	fflush(stderr);
}

struct ffmpeg_progress_ctx_t {
	double duration_sec;
	int pass_idx;
	progress_state_t *ps;

	std::map<std::string,std::string> kv;
	std::vector<std::string> tail_lines;
	double last_overall_pct;
};

int run_ffmpeg_with_progress(const std::vector<std::string>& argv, double duration_sec, int pass_idx, progress_state_t &ps) {
	std::string cmdline = format_cmdline_for_log(argv);
	if (cfg.verbose >= 2) verbose(2, "[exec] %s", cmdline.c_str());

	ffmpeg_progress_ctx_t ctx;
	ctx.duration_sec = duration_sec;
	ctx.pass_idx = pass_idx;
	ctx.ps = &ps;
	ctx.last_overall_pct = -1.0;

	int rc = run_process_pipe(argv, NULL, [&](const std::string& line) {
		// Forward raw output when very verbose (helps debugging filter errors).
		if (cfg.verbose >= 3) verbose(3, "[ffmpeg] %s", line.c_str());

		// Always keep a tail of lines (used on error).
		const size_t MAX_TAIL_LINES = 80;
		if (ctx.tail_lines.size() >= MAX_TAIL_LINES) ctx.tail_lines.erase(ctx.tail_lines.begin());
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
				}
			}
			ctx.kv.clear();
		} else if (line == "progress=end") {
			if (cfg.verbose >= 1) {
				report_progress(ctx.kv, ctx.duration_sec);
			} else {
				double overall = ((double)(ctx.pass_idx + 1) / (double)ctx.ps->pass_count) * 100.0;
				int64_t now = now_ms();
				ctx.ps->last_draw_ms = now;
				ctx.last_overall_pct = overall;
				progressbar_update(overall, 0.0);
			}
			ctx.kv.clear();
		}
	});

	if (rc != 0 && cfg.verbose >= 1) {
		verbose(1, "[exec] %s", cmdline.c_str());
		verbose(1, "[exit] rc=%d", rc);
		for (size_t i = 0; i < ctx.tail_lines.size(); i++)
			verbose(1, "[ffmpeg] %s", ctx.tail_lines[i].c_str());
	} else {
		if (cfg.verbose >= 2) verbose(2, "[exit] rc=%d", rc);
	}

	return rc;
}


