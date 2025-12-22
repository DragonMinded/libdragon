/*
    videoconv64 analysis module
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include "../common/json.hpp"

#include <math.h>
#include <string.h>
#include <algorithm>

using json = nlohmann::json;

static double parse_fraction(const std::string& s) {
	size_t slash = s.find('/');
	if (slash == std::string::npos) return atof(s.c_str());
	double num = atof(s.substr(0, slash).c_str());
	double den = atof(s.substr(slash + 1).c_str());
	return den != 0.0 ? (num / den) : 0.0;
}

static int round_up(int v, int align) {
	return (v + align - 1) / align * align;
}

static int gcd_int(int a, int b) {
	if (a < 0) a = -a;
	if (b < 0) b = -b;
	while (b != 0) {
		int t = a % b;
		a = b;
		b = t;
	}
	return a ? a : 1;
}

static void rational_approx(double v, int max_num, int max_den, int *out_num, int *out_den) {
	// Cheap rational approximation suitable for SAR/PAR signaling.
	// We scan denominators up to max_den and pick the best numerator (bounded).
	if (!(v > 0.0)) { *out_num = 1; *out_den = 1; return; }
	double best_err = 1e100;
	int best_n = 1, best_d = 1;
	for (int d = 1; d <= max_den; d++) {
		int n = (int)floor(v * d + 0.5);
		if (n < 1) n = 1;
		if (n > max_num) n = max_num;
		double err = fabs(v - (double)n / (double)d);
		if (err < best_err) {
			best_err = err;
			best_n = n;
			best_d = d;
			if (best_err < 1e-9) break;
		}
	}
	int g = gcd_int(best_n, best_d);
	best_n /= g;
	best_d /= g;
	*out_num = best_n;
	*out_den = best_d;
}

static double choose_default_fps(double src_fps) {
	// Simple heuristic (TODO: can be tuned later)
	if (src_fps <= 0.0) return 24.0;
	if (fabs(src_fps - 23.976) < 0.2 || fabs(src_fps - 24.0) < 0.2) return 24.0;
	if (fabs(src_fps - 25.0) < 0.2) return 25.0;
	if (fabs(src_fps - 29.97) < 0.3 || fabs(src_fps - 30.0) < 0.3) return 30.0;
	return 24.0;
}

static SourceMeta ffprobe_analyze_source(void) {
	std::vector<std::string> cmd = {
		cfg.ffprobe_path,
		"-v", "error",
		"-select_streams", "v:0",
		"-show_entries", "stream=width,height,sample_aspect_ratio,avg_frame_rate,r_frame_rate,pix_fmt,duration",
		"-of", "json",
		cfg.input_file,
	};

	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) fatal("ffprobe failed (%d)", rc);

	SourceMeta m;
	try {
		json j = json::parse(out);
		if (!j.contains("streams") || j["streams"].empty()) fatal("ffprobe: no streams found");
		json s = j["streams"][0];

		m.width = s.value("width", 0);
		m.height = s.value("height", 0);
		m.pix_fmt = s.value("pix_fmt", std::string());

		std::string sar = s.value("sample_aspect_ratio", std::string("1:1"));
		size_t colon = sar.find(':');
		if (colon != std::string::npos) {
			double num = atof(sar.substr(0, colon).c_str());
			double den = atof(sar.substr(colon + 1).c_str());
			if (den != 0.0) m.par = num / den;
		}

		std::string afr = s.value("avg_frame_rate", std::string());
		if (!afr.empty()) m.fps = parse_fraction(afr);

		// duration might be missing or non-numeric
		if (s.contains("duration")) {
			if (s["duration"].is_string()) m.duration = atof(s["duration"].get<std::string>().c_str());
			else if (s["duration"].is_number()) m.duration = s["duration"].get<double>();
		}
	} catch (const std::exception& e) {
		fatal("ffprobe: JSON parse error: %s", e.what());
	}

	if (m.width <= 0 || m.height <= 0) fatal("ffprobe: invalid dimensions %dx%d", m.width, m.height);
	return m;
}

static bool parse_idet_line(const std::string& all, AnalysisMetrics &am) {
	// ffmpeg may print multiple idet blocks (eg: one early with zeros, then the final summary).
	// Always parse the LAST "Multi frame detection" line.
	size_t pos = all.rfind("Multi frame detection:");
	if (pos == std::string::npos) return false;

	// Parse ints by locating the labels (spacing can vary a lot).
	auto get_int_after = [&](const char *label, int *out) -> bool {
		size_t p = all.find(label, pos);
		if (p == std::string::npos) return false;
		p += strlen(label);
		while (p < all.size() && all[p] == ' ') p++;
		*out = atoi(all.c_str() + p);
		return true;
	};

	if (!get_int_after("TFF:", &am.tff)) return false;
	if (!get_int_after("BFF:", &am.bff)) return false;
	if (!get_int_after("Progressive:", &am.progressive)) return false;
	// Undetermined is optional in some builds; default to 0.
	get_int_after("Undetermined:", &am.undetermined);

	int inter = am.tff + am.bff;
	int denom = inter + am.progressive;
	if (denom <= 0) return false;
	double R = (double)inter / (double)denom;
	if (R > 0.10) am.interlaced = true;
	else if (R < 0.05) am.interlaced = false;
	else am.interlaced = false; // conservative fallback to progressive
	return true;
}

static std::vector<double> extract_signalstats_yavg(const std::string& all) {
	std::vector<double> vals;
	const char *needle = "lavfi.signalstats.YAVG=";
	size_t pos = 0;
	while (true) {
		pos = all.find(needle, pos);
		if (pos == std::string::npos) break;
		pos += strlen(needle);
		size_t end = pos;
		while (end < all.size() && ((all[end] >= '0' && all[end] <= '9') || all[end] == '.')) end++;
		if (end > pos) vals.push_back(atof(all.substr(pos, end - pos).c_str()));
		pos = end;
	}
	return vals;
}

static double mean_of(const std::vector<double>& v) {
	if (v.empty()) return 0.0;
	double s = 0.0;
	for (size_t i = 0; i < v.size(); i++) s += v[i];
	return s / (double)v.size();
}

static double percentile_of(std::vector<double> v, double p) {
	if (v.empty()) return 0.0;
	std::sort(v.begin(), v.end());
	double idx = p * (double)(v.size() - 1);
	size_t i0 = (size_t)floor(idx);
	size_t i1 = (size_t)ceil(idx);
	if (i0 == i1) return v[i0];
	double t = idx - (double)i0;
	return v[i0] * (1.0 - t) + v[i1] * t;
}

static void ffmpeg_idet_sample(const SourceMeta& m, AnalysisMetrics &am) {
	// Interlace detection can be relatively expensive because it requires decoding frames.
	// We keep it fast by sampling fewer frames and downscaling before running idet.
	// Sample 120 frames starting at 10% duration (or 0 if unknown)
	double ss = (m.duration > 0.0) ? (m.duration * 0.10) : 0.0;
	char ssbuf[64];
	snprintf(ssbuf, sizeof(ssbuf), "%.3f", ss);

	std::vector<std::string> cmd = {
		cfg.ffmpeg_path,
		"-hide_banner",
		"-nostats",
		"-v", "info",
		// Reduce demux probing for faster startup (best-effort).
		"-probesize", "32k",
		"-analyzeduration", "0",
		"-ss", ssbuf,
		"-i", cfg.input_file,
		"-an",
		// Downscale first: idet does not need full resolution.
		"-vf", "scale=160:-2:flags=fast_bilinear,idet",
		"-frames:v", "120",
		"-f", "null",
		"-",
	};

	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) {
		// If detection fails, default to progressive: deinterlacing when not needed is expensive and may harm quality.
		// We'll keep going with conservative settings.
		verbose(1, "idet failed (%d), assuming progressive", rc);
		return;
	}
	if (!parse_idet_line(out, am)) {
		verbose(1, "idet output did not contain a parsable summary, assuming progressive");
	}
}

struct Stats { double mean; double p95; };

static Stats ffmpeg_signalstats_stats(const char *vf, double ss) {
	char ssbuf[64];
	snprintf(ssbuf, sizeof(ssbuf), "%.3f", ss);

	std::vector<std::string> cmd = {
		cfg.ffmpeg_path,
		"-hide_banner",
		"-nostats",
		// metadata=print logs at INFO level, so we must allow INFO output.
		"-v", "info",
		"-ss", ssbuf,
		"-i", cfg.input_file,
		"-an",
		"-vf", vf,
		"-frames:v", "250",
		"-f", "null",
		"-",
	};

	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) return {0.0, 0.0};

	std::vector<double> y = extract_signalstats_yavg(out);
	return { mean_of(y), percentile_of(y, 0.95) };
}

AnalysisResult vconv_analyze(const CodecInfo &ci) {
	AnalysisResult r;
	r.meta = ffprobe_analyze_source();

	// Target output FPS
	r.out_fps = (cfg.fps > 0.0) ? cfg.fps : choose_default_fps(r.meta.fps);

	// Target resolution around cfg.width, then apply codec alignment.
	// We do not error out if the exact computed size doesn't match alignment:
	// we always round up to the codec requirements.
	double dar = ((double)r.meta.width * r.meta.par) / (double)r.meta.height;
	int req_w = cfg.width;
	int req_h = (dar > 0.0) ? (int)floor(((double)req_w / dar) + 0.5) : r.meta.height;
	r.out_width = round_up(req_w, ci.align_w);
	r.out_height = round_up(req_h, ci.align_h);

	verbose(1, "Target: %dx%d -> %dx%d (align %dx%d)",
		req_w, req_h, r.out_width, r.out_height, ci.align_w, ci.align_h);

	// Automatic anamorphic mode for N64 progressive:
	// If rounding/alignment leads to a full-height output (240 lines) for a wide DAR,
	// the VI would effectively letterbox by downscaling vertically. Instead, we
	// pre-letterbox in the encoded raster and signal PAR != 1.
	//
	// NOTE: we keep this behavior behind an internal toggle for now.
	if (cfg.par_auto && r.out_height == 240 && dar > (4.0 / 3.0) + 1e-6) {
		int target_h = (int)floor(240.0 * (4.0 / 3.0) / dar + 0.5);
		target_h = round_up(target_h, ci.align_h);
		// Clamp to sane range.
		if (target_h < ci.align_h) target_h = ci.align_h;
		if (target_h > 240) target_h = 240;

		if (target_h != r.out_height) {
			int old_h = r.out_height;
			r.out_height = target_h;
			verbose(1, "Anamorphic: forcing height %d -> %d for DAR=%.6f", old_h, r.out_height, dar);
		}

		// PAR = DAR * H / W
		r.out_par = dar * (double)r.out_height / (double)r.out_width;
		// Use a bounded rational for bitstream signaling. Keep denominators small.
		rational_approx(r.out_par, 255, 255, &r.sar_num, &r.sar_den);
		verbose(1, "Anamorphic: PAR=%.6f SAR=%d:%d", r.out_par, r.sar_num, r.sar_den);
	} else {
		r.out_par = 1.0;
		r.sar_num = 1;
		r.sar_den = 1;
	}

	// Interlace detection is only needed when deinterlace is auto.
	if (cfg.deinterlace == "auto") {
		ffmpeg_idet_sample(r.meta, r.metrics);
	} else {
		verbose(1, "Skipping idet (deinterlace forced: %s)", cfg.deinterlace.c_str());
	}

	// Content metrics are only needed to choose a profile when profile=auto and we're not in quick mode.
	// profile=none forces no profile-specific preprocessing.
	const bool need_profile_metrics = (cfg.profile == "auto") && !cfg.quick;
	if (need_profile_metrics) {
		// Content metrics are computed on a fixed 320x240 @ 24fps proxy to keep numbers comparable across inputs.
		// This does not change output size: it is only used to decide denoise/debanding/sharpen strength.
		double ss = (r.meta.duration > 0.0) ? (r.meta.duration * 0.10) : 0.0;

		// Motion/noise proxy:
		// - downscale + fps normalization to stabilize sampling
		// - compute frame-to-frame difference (tblend=difference)
		// - measure luma average on the diff frames (signalstats) as a proxy for motion+noise energy
		const char *vf_diff =
			"scale=320:240:flags=bicubic,fps=24,"
			"tblend=all_mode=difference,"
			"signalstats,metadata=print";

		// Flatness proxy:
		// - downscale + fps normalization
		// - blur a copy, subtract from original (difference blend)
		// - measure luma average of the residual: low values usually indicate large flat areas (cartoon/anime),
		//   where banding and ringing are more visible
		const char *vf_flat =
			"scale=320:240:flags=bicubic,fps=24,"
			"split[a][b];"
			"[a]gblur=sigma=1.0[a2];"
			"[b][a2]blend=all_mode=difference,"
			"signalstats,metadata=print";

		Stats diff = ffmpeg_signalstats_stats(vf_diff, ss);
		Stats flat = ffmpeg_signalstats_stats(vf_flat, ss);
		r.metrics.diff_mean = diff.mean;
		r.metrics.diff_p95 = diff.p95;
		r.metrics.flat_mean = flat.mean;
		r.metrics.flat_p95 = flat.p95;
	} else {
		if (cfg.profile != "auto") verbose(1, "Skipping content metrics (profile forced: %s)", cfg.profile.c_str());
		else verbose(1, "Skipping content metrics (quick mode)");
	}

	// Profile selection (TODO: can be tuned later)
	const double N_high = 4.0;
	const double HF_low = 2.5;

	r.selected_profile = cfg.profile;
	if (r.selected_profile == "auto") {
		if (!need_profile_metrics) {
			// In quick mode we skip profile-specific filters anyway; default to a conservative general-purpose profile.
			r.selected_profile = "film";
		} else {
			if (r.metrics.diff_mean >= N_high) r.selected_profile = "noisy";
			else if (r.metrics.flat_mean <= HF_low) r.selected_profile = "cartoon";
			else r.selected_profile = "film";
		}
	}

	verbose(1, "Source: %dx%d PAR=%.3f FPS=%.3f", r.meta.width, r.meta.height, r.meta.par, r.meta.fps);
	verbose(1, "FPS: %.3f", r.out_fps);
	verbose(1, "Interlace: %s (TFF=%d BFF=%d P=%d U=%d)",
		r.metrics.interlaced ? "yes" : "no", r.metrics.tff, r.metrics.bff, r.metrics.progressive, r.metrics.undetermined);
	if (need_profile_metrics) {
		verbose(1, "Metrics: diff_mean=%.3f diff_p95=%.3f flat_mean=%.3f flat_p95=%.3f",
			r.metrics.diff_mean, r.metrics.diff_p95, r.metrics.flat_mean, r.metrics.flat_p95);
	}
	verbose(1, "Profile: %s", r.selected_profile.c_str());

	return r;
}


