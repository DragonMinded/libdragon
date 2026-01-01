/*
    videoconv64 subtitles module (SUB64)
    C++17 implementation (linter may not understand the project flags).
*/

#include "videoconv64.h"

// Use binout placeholders to avoid building the whole file in RAM.
#include "../common/binout.h"
#include "../common/assetcomp.h"
#include "../common/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>

using json = nlohmann::json;

enum Sub64OpCode : uint8_t {
    SUB64_OP_SHOW = 0x01,
    SUB64_OP_HIDE0 = 0x20,
    SUB64_OP_HIDE1 = 0x21,
    SUB64_OP_HIDE2 = 0x22,
    SUB64_OP_HIDE3 = 0x23,
    SUB64_OP_CLEAR = 0x04,
};

struct VttCue {
	int64_t start_ms = 0;
	int64_t end_ms = 0;
	std::string text;
};

static std::string trim(const std::string &s) {
	size_t a = 0;
	while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
	size_t b = s.size();
	while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r')) b--;
	return s.substr(a, b - a);
}

static bool starts_with(const std::string &s, const char *pfx) {
	size_t n = strlen(pfx);
	return s.size() >= n && memcmp(s.data(), pfx, n) == 0;
}

static bool parse_vtt_timestamp_ms(const std::string &ts, int64_t *out_ms) {
	std::string t = trim(ts);
	int hh = 0, mm = 0, ss = 0, ms = 0;
	int ncol = 0;
	for (char c: t) if (c == ':') ncol++;
	if (ncol == 1) {
		if (sscanf(t.c_str(), "%d:%d.%d", &mm, &ss, &ms) != 3) return false;
	} else if (ncol == 2) {
		if (sscanf(t.c_str(), "%d:%d:%d.%d", &hh, &mm, &ss, &ms) != 4) return false;
	} else {
		return false;
	}
	if (mm < 0 || ss < 0 || ms < 0) return false;
	*out_ms = ((int64_t)hh * 3600 + (int64_t)mm * 60 + (int64_t)ss) * 1000 + (int64_t)ms;
	return true;
}

static std::string vtt_to_rdpq_text(const std::string &text) {
	// - <i>/<b>/<u> -> $02/$03/$04
	// - closing tags -> $01
	// - <br> -> '\n'
	// Escape '$' and '^' for rdpq_text.
	std::string out;
	out.reserve(text.size());

	for (size_t i = 0; i < text.size(); ) {
		char c = text[i];
		if (c == '<') {
			size_t j = text.find('>', i + 1);
			if (j == std::string::npos) {
				out.push_back('<');
				i++;
				continue;
			}
			std::string tag = trim(text.substr(i + 1, j - (i + 1)));
			for (char &x : tag) x = (char)tolower((unsigned char)x);
			if (tag == "i") out += "$02";
			else if (tag == "/i") out += "$01";
			else if (tag == "b") out += "$03";
			else if (tag == "/b") out += "$01";
			else if (tag == "u") out += "$04";
			else if (tag == "/u") out += "$01";
			else if (tag == "br" || tag == "br/" || tag == "br /") out.push_back('\n');
			// else strip unknown tags
			i = j + 1;
			continue;
		}

		if (c == '$') out += "$$";
		else if (c == '^') out += "^^";
		else out.push_back(c);
		i++;
	}
	return out;
}

static std::vector<VttCue> parse_webvtt(const std::string &path) {
	std::ifstream f(path);
	if (!f) fatal("Failed to open VTT: %s", path.c_str());

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		lines.push_back(line);
	}

	std::vector<VttCue> cues;
	size_t i = 0;

	while (i < lines.size() && trim(lines[i]).empty()) i++;
	if (i < lines.size() && starts_with(trim(lines[i]), "WEBVTT")) i++;

	while (i < lines.size()) {
		while (i < lines.size() && trim(lines[i]).empty()) i++;
		if (i >= lines.size()) break;

		std::string l = trim(lines[i]);
		if (starts_with(l, "NOTE") || starts_with(l, "STYLE") || starts_with(l, "REGION")) {
			i++;
			while (i < lines.size() && !trim(lines[i]).empty()) i++;
			continue;
		}

		size_t timing_line = i;
		if (lines[i].find("-->") == std::string::npos && (i + 1) < lines.size() && lines[i+1].find("-->") != std::string::npos) {
			timing_line = i + 1;
		}
		if (timing_line >= lines.size()) break;

		std::string timing = trim(lines[timing_line]);
		size_t arrow = timing.find("-->");
		if (arrow == std::string::npos) { i = timing_line + 1; continue; }

		std::string a = trim(timing.substr(0, arrow));
		std::string b = trim(timing.substr(arrow + 3));
		size_t sp = b.find(' ');
		if (sp != std::string::npos) b = b.substr(0, sp);

		int64_t st = 0, en = 0;
		if (!parse_vtt_timestamp_ms(a, &st) || !parse_vtt_timestamp_ms(b, &en) || en <= st) {
			i = timing_line + 1;
			continue;
		}

		i = timing_line + 1;
		std::string payload;
		while (i < lines.size() && !trim(lines[i]).empty()) {
			if (!payload.empty()) payload.push_back('\n');
			payload += lines[i];
			i++;
		}

		VttCue c;
		c.start_ms = st;
		c.end_ms = en;
		c.text = vtt_to_rdpq_text(payload);
		cues.push_back(std::move(c));
	}
	return cues;
}

static int64_t ms_to_frame(int64_t ms, float fps) {
	return (int64_t)floor((double)ms * (double)fps / 1000.0 + 0.5);
}

static void wvaru(FILE *f, uint32_t v) {
	// VarUInt LEB128
	while (v >= 0x80) { w8(f, (v & 0x7F) | 0x80); v >>= 7; }
	w8(f, v & 0x7F);
}

static std::string sanitize_lang(const std::string &lang) {
	std::string out;
	for (char c : lang) {
		if (isalnum((unsigned char)c) || c == '_' || c == '-') out.push_back((char)tolower((unsigned char)c));
	}
	return out;
}

struct SubStreamInfo {
	int stream_index = -1;
	std::string language;
	std::string codec_name;
};

static bool subtitle_codec_is_text(const std::string &codec) {
	// ffmpeg can convert only text-based subtitle codecs to WebVTT.
	// Bitmap subtitles (eg: PGS/DVD/DVB) must be skipped.
	if (codec.empty()) return true; // best-effort: assume text
	const std::string c = codec;
	// Common bitmap subtitle codecs
	if (c == "hdmv_pgs_subtitle") return false;
	if (c == "dvd_subtitle") return false;
	if (c == "dvb_subtitle") return false;
	if (c == "xsub") return false;
	// Most other subtitle codecs in common containers are text.
	return true;
}

static std::vector<SubStreamInfo> ffprobe_list_subtitle_streams(void) {
	std::vector<std::string> cmd = {
		cfg.ffprobe_path,
		"-v", "error",
		"-select_streams", "s",
		"-show_entries", "stream=index,codec_name:stream_tags=language",
		"-of", "json",
		cfg.input_file,
	};
	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) return {};

	std::vector<SubStreamInfo> res;
	try {
		json j = json::parse(out);
		if (!j.contains("streams") || !j["streams"].is_array()) return res;
		for (const auto &s : j["streams"]) {
			SubStreamInfo si;
			si.stream_index = s.value("index", -1);
			si.codec_name = s.value("codec_name", std::string());
			if (s.contains("tags") && s["tags"].is_object()) {
				si.language = s["tags"].value("language", std::string());
			}
			res.push_back(std::move(si));
		}
	} catch (...) {
		return {};
	}
	return res;
}

static std::string ensure_webvtt_container_track(int stream_index, const std::string &tmp_dir, int idx) {
	std::string tmp = join_path(tmp_dir, "subtrack_" + std::to_string(idx) + ".vtt");
	std::vector<std::string> cmd = {
		cfg.ffmpeg_path,
		"-hide_banner", "-nostats", "-y", "-v", "error",
		"-i", cfg.input_file,
		"-map", "0:" + std::to_string(stream_index),
		"-f", "webvtt",
		tmp,
	};
	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) {
		// Most common failure here is bitmap subtitles (PGS/DVD/DVB). Caller may decide to skip.
		fatal("ffmpeg failed converting container subtitles to VTT (%d)", rc);
	}
	return tmp;
}

static std::string ensure_webvtt_file(const std::string &in_path, const std::string &tmp_dir, int idx) {
	std::string tmp = join_path(tmp_dir, "subfile_" + std::to_string(idx) + ".vtt");
	std::vector<std::string> cmd = {
		cfg.ffmpeg_path,
		"-hide_banner", "-nostats", "-y", "-v", "error",
		"-i", in_path,
		"-f", "webvtt",
		tmp,
	};
	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) fatal("ffmpeg failed converting subtitle file to VTT (%d)", rc);
	return tmp;
}

static std::string output_sub64_path(const std::string &lang, int out_idx, int total) {
	std::string base = strip_ext(base_name(cfg.input_file));
	std::string name;
	if (total <= 1) {
		// Backward compatible naming for a single subtitle.
		std::string sl = sanitize_lang(lang);
		name = sl.empty() ? (base + ".sub64") : (base + "." + sl + ".sub64");
	} else {
		// Multiple subtitles: numeric pattern basename.N.sub64
		name = base + "." + std::to_string(out_idx) + ".sub64";
	}
	if (cfg.output_dir.empty()) return name;
	return join_path(cfg.output_dir, name);
}

struct Sub64Metrics {
	int overflow_count = 0;
	int max_overlap = 0;
};

static void write_sub64(
	const std::string &out_path,
	const std::vector<VttCue> &cues,
	float fps,
	uint32_t canvas_w,
	uint32_t canvas_h,
	uint32_t sync_interval_frames,
	Sub64Metrics *m
) {
    // Don't emit an empty subtitle file.
    assert(!cues.empty());

    // Create a timeline of cue boundary events (either start or end of a cue),
    // in frame timestamp (so already quantized).
    // Sort it by frame and kind (end before start), so that for each frame with
    // multiple events, we will first process all ends before any starts.
    const int TL_START = 1, TL_END = 0;
    struct TL { int64_t frame; int kind; int id; }; // kind 0=end,1=start
	std::vector<TL> tl;
	tl.reserve(cues.size() * 2);
	for (int i = 0; i < (int)cues.size(); i++) {
		int64_t s = ms_to_frame(cues[i].start_ms, fps);
		int64_t e = ms_to_frame(cues[i].end_ms, fps);
		if (e <= s) continue;
		tl.push_back({ e, TL_END,   i });
		tl.push_back({ s, TL_START, i });
	}
	std::sort(tl.begin(), tl.end(), [](const TL &a, const TL &b) {
		if (a.frame != b.frame) return a.frame < b.frame;
		return a.kind < b.kind; // end before start
	});

	struct Action {
		int64_t frame;
		Sub64OpCode opcode;
		std::string text; // SHOW only
	};
	std::vector<Action> actions;
	std::vector<int> stack; // cue ids, most recent first
	int maxov = 0;  // max simultaneous visible subtitles
	int overflow = 0; // number of overflows (>4 subtitles visible)
	std::vector<int64_t> gaps; // Gaps: [gap0_start, gap0_end, gap1_start, gap1_end, ...]

	// If subtitles start after frame 0, add an initial gap start so the first SHOW closes it.
	if (!tl.empty() && tl.front().kind == 1 && tl.front().frame > 0) {
		gaps.push_back(0);
	}

	for (const auto &e : tl) {
		if (e.kind == TL_START) {
			if (gaps.size() % 2 == 1)
                gaps.push_back(e.frame); // gap end
			int would_be = (int)stack.size() + 1;
			maxov = std::max(maxov, would_be);
			if ((int)stack.size() >= 4) {
                // Detected overflows (too many subtitles visible at the same time).
                // Hide the oldest subtitle (index 3) by emitting an HIDE3 opcode.
				overflow++;
				verbose(1, "warning: subtitle overlap overflow at frame=%lld; hiding oldest", (long long)e.frame);
				actions.push_back({ e.frame, SUB64_OP_HIDE3, {} });
				stack.pop_back();
			}
			actions.push_back({ e.frame, SUB64_OP_SHOW, cues[e.id].text });
			stack.insert(stack.begin(), e.id);
		} else {
            // Emit an HIDE opcode for the stack position matching the cue id.
			auto it = std::find(stack.begin(), stack.end(), e.id);
			if (it == stack.end()) continue;
			int idx = (int)(it - stack.begin());
			actions.push_back({ e.frame, (Sub64OpCode)(SUB64_OP_HIDE0 + idx), {} });
			stack.erase(it);
            if (gaps.size() % 2 == 0 && stack.empty())
                gaps.push_back(e.frame); // gap start
		}
	}

	// Drop trailing open gap (start without end). We don't emit syncpoints after last subtitle event anyway.
	if (gaps.size() % 2 == 1) gaps.pop_back();

    // Save metrics
	if (m) { m->overflow_count = overflow; m->max_overlap = maxov; }

	// Precompute TXT0 offsets per each subtitle text (relative to TXT0 start).
	std::vector<uint32_t> txt_off_by_event(actions.size(), 0);
	uint32_t txt_total = 0;
	for (size_t i = 0; i < actions.size(); i++) {
		txt_off_by_event[i] = txt_total;
		if (actions[i].opcode == SUB64_OP_SHOW) {
			txt_total += (uint32_t)actions[i].text.size() + 1;
		}
	}

	int64_t max_frame = actions.empty() ? 0 : actions.back().frame;

	// Decide sync_frames. These are the frames that will be directly seekable
    // without decoding the subtitle stream. We pick a gap frame at approximately
    // sync_interval_frames intervals.
	std::vector<int64_t> sync_frames;
    sync_frames.push_back(0); // Frame 0 is always a syncpoint.
    for (int64_t t = 0; t <= max_frame; t += (int64_t)sync_interval_frames) {
        int gapidx = std::upper_bound(gaps.begin(), gaps.end(), t) - gaps.begin();
        if (gapidx % 2 == 1) gapidx--;
        int sf = gaps[gapidx];
        if (sf == sync_frames.back()) continue;
        sync_frames.push_back(sf);
	}

	FILE *f = fopen(out_path.c_str(), "wb");
	if (!f) fatal("Failed to write: %s", out_path.c_str());

	// Header (v0.6). Sections are laid out sequentially:
	//   [header][IDX0][DELT][OPC0][TXT0]
	// IDX0 count is stored in the header; all offsets in IDX0 are absolute (from file start).
	wa(f, "SUB64", 5);
    w8(f, 1);    // version
	w16(f, 0);   // flags
	wf32(f, (float)fps);
	w32(f, canvas_w);
	w32(f, canvas_h);
	w32(f, (uint32_t)sync_frames.size()); // idx0_count
	w16(f, 0);   // reserved

	// IDX0 section
	for (int i = 0; i < (int)sync_frames.size(); i++) {
		w32(f, (uint32_t)sync_frames[i]);
		// Absolute byte offsets from start of SUB64 payload.
		w32_placeholderf(f, "idx%d_delt", i);
		w32_placeholderf(f, "idx%d_opc", i);
		w32_placeholderf(f, "idx%d_txt", i);
	}

	// DELT section: write VarUInt(delta_frames) and patch IDX0 placeholders on the fly.
	int64_t last_fr = 0;
	int sync_i = 0;

	for (size_t ev = 0; ev < actions.size(); ev++) {
		// Patch any syncpoints that fall at/before this event (first event at/after sync_frame).
		while (sync_i < (int)sync_frames.size() && sync_frames[sync_i] <= actions[ev].frame) {
			// DELT absolute offset is known now (we are in the DELT section).
			// We want the absolute offset of the current DELT cursor, which is ftell(f).
			placeholder_set(f, "idx%d_delt", sync_i);
			sync_i++;
		}

		uint32_t delta = (actions[ev].frame >= last_fr) ? (uint32_t)(actions[ev].frame - last_fr) : 0;
		last_fr = actions[ev].frame;

		wvaru(f, delta);
	}
	// Same rationale as OPC0/TXT0: no EOF syncpoints.
	assert(sync_i == (int)sync_frames.size());

	// OPC section
	int sync_opc_i = 0;
	for (size_t ev = 0; ev < actions.size(); ev++) {
		// Patch opcode offsets at the first event at/after sync_frame.
		while (sync_opc_i < (int)sync_frames.size() && sync_frames[sync_opc_i] <= actions[ev].frame) {
			placeholder_set(f, "idx%d_opc", sync_opc_i);
			sync_opc_i++;
		}
		w8(f, actions[ev].opcode);
	}
	// By construction we don't emit syncpoints after the last event.
	// Therefore every IDX0 entry must have been patched within the loop above.
	assert(sync_opc_i == (int)sync_frames.size());

	// TXT section
	int sync_txt_i = 0;
	for (size_t ev = 0; ev < actions.size(); ev++) {
		// Patch text offsets at the first event at/after sync_frame (current TXT0 cursor).
		while (sync_txt_i < (int)sync_frames.size() && sync_frames[sync_txt_i] <= actions[ev].frame) {
			placeholder_set(f, "idx%d_txt", sync_txt_i);
			sync_txt_i++;
		}
		if (actions[ev].opcode != 0x01) continue;
		wa(f, actions[ev].text.data(), actions[ev].text.size());
		w8(f, 0);
	}
	// Same rationale as OPC0: no EOF syncpoints.
	assert(sync_txt_i == (int)sync_frames.size());

	fclose(f);

	// Clear placeholders (we can generate multiple subtitle files in one run).
	placeholder_clear();
}

void vconv_process_subtitles(const AnalysisResult &ar) {
	std::vector<SubStreamInfo> tracks = ffprobe_list_subtitle_streams();
	if (tracks.empty() && cfg.subtitle_files.empty()) return;

	float fps = (float)ar.out_fps;
	if (!(fps > 0.0f)) fatal("Subtitles: invalid output fps");
	uint32_t sync_interval_frames = (uint32_t)floor((double)fps * 300.0 + 0.5);

	std::string tmpd = temp_dir();
	int produced = 0;

	// Count how many subtitles we will actually process (container text tracks + CLI files).
	int total_out = 0;
	for (const auto &t : tracks) {
		if (t.stream_index < 0) continue;
		if (!subtitle_codec_is_text(t.codec_name)) continue;
		total_out++;
	}
	total_out += (int)cfg.subtitle_files.size();

	auto process_one = [&](const char *log_prefix, const std::string &lang, auto &&make_vtt_path) {
		if (cfg.verbose >= 1) verbose(1, "%s", log_prefix);
		std::string vtt = make_vtt_path();
		std::vector<VttCue> cues = parse_webvtt(vtt);
		Sub64Metrics m;
		std::string outp = output_sub64_path(lang, produced, total_out);
		write_sub64(outp, cues, fps, (uint32_t)ar.out_width, (uint32_t)ar.out_height, sync_interval_frames, &m);
		if (!asset_compress(outp.c_str(), outp.c_str(), DEFAULT_COMPRESSION, 256*1024)) {
			fatal("subtitles: compression failed for %s", outp.c_str());
		}
		verbose(1, "Output subtitle: %s (cues=%zu, overflow=%d, max_overlap=%d)", outp.c_str(), cues.size(), m.overflow_count, m.max_overlap);
		produced++;
	};

	// Container tracks
	for (const auto &t : tracks) {
		if (t.stream_index < 0) continue;
		if (!subtitle_codec_is_text(t.codec_name)) {
			verbose(1, "Subtitle track: stream=%d codec=%s -> skipping (bitmap subtitles not supported)",
				t.stream_index, t.codec_name.empty() ? "(unknown)" : t.codec_name.c_str());
			continue;
		}
		std::string msg = "Subtitle track: stream=" + std::to_string(t.stream_index) + " lang=" + (t.language.empty() ? "(none)" : t.language);
		process_one(msg.c_str(), t.language, [&]() {
			return ensure_webvtt_container_track(t.stream_index, tmpd, produced);
		});
	}

	// CLI subtitle files
	for (const auto &sf : cfg.subtitle_files) {
		std::string msg = "Subtitle file: " + sf;
		process_one(msg.c_str(), std::string(), [&]() {
			return ensure_webvtt_file(sf, tmpd, produced);
		});
	}
}


