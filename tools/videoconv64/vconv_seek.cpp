/*
    videoconv64 seek index generator
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include <stdio.h>
#include <string.h>

#include "../common/binout.c"
#include "../common/assetcomp.h"
#include "../common/json.hpp"

#include <vector>
#include <string>
#include <limits.h>

using json = nlohmann::json;

static std::string replace_ext(const std::string& path, const char *new_ext) {
	size_t slash = path.find_last_of("/\\");
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
		return path + new_ext;
	}
	return path.substr(0, dot) + new_ext;
}

// Check if the packet contains an IDR frame. This is necessary because ffprobe
// will only report whether a frame is an I-Frames or not, but for seeking we
// actually need IDR frames.
static bool h264_check_idr(
	const uint8_t *buf, size_t sz,
	uint32_t base_offset,
	uint32_t *out_idr_offset
) {
	// Scan Annex-B start codes: 00 00 01 or 00 00 00 01
	// If we find an IDR NAL (type 5), return the absolute file offset of its start code.
	if (sz < 4) return false;

	for (size_t i = 0; i + 4 <= sz; i++) {
		size_t sc_len = 0;
		if (buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x01) {
			sc_len = 3;
		} else if (i + 4 <= sz && buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x00 && buf[i+3] == 0x01) {
			sc_len = 4;
		} else {
			continue;
		}

		size_t hdr = i + sc_len;
		if (hdr >= sz) continue;
		uint8_t nal_hdr = buf[hdr];
		uint8_t nal_type = nal_hdr & 0x1F;
		if (nal_type == 5) {
			*out_idr_offset = base_offset + (uint32_t)i;
			return true;
		}
	}

	return false;
}

static bool json_get_int64(const json& obj, const char *key, int64_t *out) {
	if (!obj.contains(key)) return false;
	const json& v = obj[key];
	if (v.is_number_integer()) { *out = v.get<int64_t>(); return true; }
	if (v.is_string()) { *out = atoll(v.get<std::string>().c_str()); return true; }
	return false;
}

static std::vector<seek_point_t> build_seek_points_mpeg1(const json& frames) {
	std::vector<seek_point_t> pts;
	if (!frames.is_array()) return pts;

	for (size_t i = 0; i < frames.size(); i++) {
		const auto& fr = frames[i];
		uint32_t frame_idx = (uint32_t)i;
		const std::string pict = fr.value("pict_type", std::string());
		if (pict != "I") continue;

		if (!fr.contains("pkt_pos")) continue;

		int64_t pkt_pos = 0;
		if (!json_get_int64(fr, "pkt_pos", &pkt_pos)) continue;

		if (pkt_pos < 0 || pkt_pos > 0xFFFFFFFFLL) continue;

        pts.push_back({ (uint32_t)pkt_pos, frame_idx });
	}

	return pts;
}

static std::vector<seek_point_t> build_seek_points_h264(const std::string& video_path, const json& frames) {
	std::vector<seek_point_t> pts;
	if (!frames.is_array()) return pts;

	FILE *vf = fopen(video_path.c_str(), "rb");
	if (!vf) fatal("seek: cannot open output video: %s", video_path.c_str());

	for (size_t i = 0; i < frames.size(); i++) {
		const auto& fr = frames[i];
		uint32_t frame_idx = (uint32_t)i;
		// Candidate I-frame/keyframe; final selection requires IDR.
		const int key = fr.value("key_frame", 0);
		const std::string pict = fr.value("pict_type", std::string());
		if (!(key == 1 || pict == "I")) continue;

		if (!fr.contains("pkt_pos") || !fr.contains("pkt_size")) continue;

		int64_t pkt_pos = 0;
		int64_t pkt_size = 0;
		if (!json_get_int64(fr, "pkt_pos", &pkt_pos)) continue;
		if (!json_get_int64(fr, "pkt_size", &pkt_size)) continue;

		if (pkt_pos < 0 || pkt_pos > 0xFFFFFFFFLL) continue;
		if (pkt_size <= 0 || pkt_size > (16 * 1024 * 1024)) continue; // sanity cap

        std::vector<uint8_t> pkt_buf(pkt_size);
		if (fseek(vf, (long)(uint32_t)pkt_pos, SEEK_SET) != 0) continue;
		if (fread(pkt_buf.data(), 1, pkt_buf.size(), vf) != pkt_buf.size()) continue;

        uint32_t idr_off = 0;
        if (h264_check_idr(pkt_buf.data(), pkt_buf.size(), (uint32_t)pkt_pos, &idr_off)) {
            pts.push_back({ idr_off, frame_idx });
        }
	}

	fclose(vf);
	return pts;
}

static void write_seek_file(const std::string& seek_path, const std::vector<seek_point_t>& pts) {
	FILE *f = fopen(seek_path.c_str(), "wb");
	if (!f) fatal("seek: cannot open output: %s", seek_path.c_str());

	wa(f, "VSK", 3);
	w8(f, 2);   // version
	w32(f, (uint32_t)pts.size()); // count
	w32_placeholderf(f, "offsets");
	w32_placeholderf(f, "frames");

    // Compute the mean of the deltas.
	int32_t mean_do_s32 = 0;
	int32_t mean_df_s32 = 0;
	if (pts.size() >= 2) {
		int64_t sum_do = 0;
		int64_t sum_df = 0;
		for (int i = 1; i < pts.size(); i++) {
			// Seekpoints are expected to be increasing.
			if (pts[i].offset < pts[i-1].offset) {
				fatal("seek: non-monotonic offsets at i=%d (%u < %u)", i, pts[i].offset, pts[i-1].offset);
			}
			if (pts[i].frame < pts[i-1].frame) {
				fatal("seek: non-monotonic frames at i=%d (%u < %u)", i, pts[i].frame, pts[i-1].frame);
			}
			sum_do += (int64_t)pts[i].offset - (int64_t)pts[i-1].offset;
			sum_df += (int64_t)pts[i].frame  - (int64_t)pts[i-1].frame;
		}
		const int64_t n = (int64_t)pts.size() - 1;
		mean_do_s32 = sum_do / n;
		mean_df_s32 = sum_df / n;
	}

    // Emit the mean of the deltas.
	w32(f, mean_do_s32);
	w32(f, mean_df_s32);

	// We encode offsets and frame numbers as delta from the means of the deltas.
    // This is a very simple encoding for monotonic increasing data that can be
    // efficiently compressed with LZ-based algorithms (like assetcomp), and can
    // later be resolved at load-time in place with minimal overhead.
	placeholder_set(f, "offsets");
	w32(f, pts[0].offset);
	// Emit residual arrays: difference from the mean of the deltas.
	for (size_t i = 1; i < pts.size(); i++) {
		int32_t delta = pts[i].offset - pts[i-1].offset;
		int32_t resid = delta - mean_do_s32;
		w32(f, resid);
	}

	placeholder_set(f, "frames");
	w32(f, pts[0].frame);
	// Emit residual arrays: difference from the mean of the deltas.
	for (size_t i = 1; i < pts.size(); i++) {
		int32_t delta = pts[i].frame - pts[i-1].frame;
		int32_t resid = delta - mean_df_s32;
		w32(f, resid);
	}

	fclose(f);
}

std::vector<seek_point_t> vconv_generate_seek(const CodecInfo &ci, const std::string &video_path) {
	(void)ci;
	verbose(1, "Seek: analyzing %s", video_path.c_str());

	std::vector<std::string> cmd = {
		cfg.ffprobe_path,
		"-v", "error",
		"-select_streams", "v:0",
		"-show_frames",
		"-show_entries", "frame=key_frame,pict_type,pkt_pos,pkt_size",
		"-of", "json",
		video_path,
	};

	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) fatal("seek: ffprobe failed (%d)", rc);

	json j;
	try {
		j = json::parse(out);
	} catch (const std::exception& e) {
		fatal("seek: ffprobe JSON parse error: %s", e.what());
	}

	const json frames = j.value("frames", json::array());

	std::vector<seek_point_t> pts;
	if (cfg.codec == "h264") {
		pts = build_seek_points_h264(video_path, frames);
	} else if (cfg.codec == "mpeg1") {
		pts = build_seek_points_mpeg1(frames);
	} else {
		fatal("seek: unsupported codec: %s", cfg.codec.c_str());
	}

    // Write the seek file and compress it with default asset compression
	if (pts.size() > 0) {
		std::string seek_path = replace_ext(video_path, ".seek");
		write_seek_file(seek_path, pts);
		if (!asset_compress(seek_path.c_str(), seek_path.c_str(), DEFAULT_COMPRESSION, 256*1024)) {
			fatal("seek: compression failed for %s", seek_path.c_str());
		}
		verbose(1, "Seek: wrote %s (%d points)", seek_path.c_str(), (int)pts.size());
	}

	return pts;
}
