/*
    videoconv64 encoding module (H.264)
	Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include <math.h>
#include <string.h>

static const uint8_t H264_LD_BUFFER_UUID[16] = {
	'L', 'I', 'B', 'D', 'R', 'A', 'G', 'O', 'N', 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t H264_X264_INFO_UUID[16] = {
	0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9, 0x48, 0xb7,
	0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xef,
};

static const size_t H264_PLAYER_BUF_SIZE = 64 * 1024;

struct H264Nal {
	size_t prefix = 0;
	uint8_t type = 0;
	std::vector<uint8_t> bytes;
};

static bool h264_find_start_code(const std::vector<uint8_t>& data, size_t from, size_t *pos, size_t *prefix) {
	for (size_t i = from; i + 3 <= data.size(); i++) {
		if (data[i] != 0x00 || data[i+1] != 0x00)
			continue;
		if (i + 2 < data.size() && data[i+2] == 0x01) {
			*pos = i;
			*prefix = 3;
			return true;
		}
		if (i + 3 < data.size() && data[i+2] == 0x00 && data[i+3] == 0x01) {
			*pos = i;
			*prefix = 4;
			return true;
		}
	}
	return false;
}

static bool h264_next_nal_span(const std::vector<uint8_t>& data, size_t from,
		size_t *start, size_t *prefix, size_t *end, uint8_t *type) {
	size_t nal_start, nal_prefix;
	if (!h264_find_start_code(data, from, &nal_start, &nal_prefix))
		return false;
	if (nal_start + nal_prefix >= data.size())
		return false;

	size_t next, next_prefix;
	if (!h264_find_start_code(data, nal_start + nal_prefix, &next, &next_prefix))
		next = data.size();

	*start = nal_start;
	*prefix = nal_prefix;
	*end = next;
	*type = data[nal_start + nal_prefix] & 0x1F;
	return true;
}

static bool h264_split_nals(const std::vector<uint8_t>& data, std::vector<H264Nal> *nals) {
	nals->clear();

	size_t start, prefix, end;
	uint8_t type;
	size_t scan = 0;
	while (h264_next_nal_span(data, scan, &start, &prefix, &end, &type)) {
		H264Nal nal;
		nal.prefix = prefix;
		nal.type = type;
		nal.bytes.insert(nal.bytes.end(), data.begin() + start, data.begin() + end);
		nals->push_back(nal);
		scan = end;
	}

	return !nals->empty();
}

static bool h264_read_file(const std::string& path, std::vector<uint8_t> *data) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
	long size = ftell(f);
	if (size < 0) { fclose(f); return false; }
	if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }

	data->resize((size_t)size);
	if (!data->empty() && fread(data->data(), 1, data->size(), f) != data->size()) {
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

static void h264_append_be32(std::vector<uint8_t> *out, uint32_t v) {
	out->push_back((uint8_t)(v >> 24));
	out->push_back((uint8_t)(v >> 16));
	out->push_back((uint8_t)(v >> 8));
	out->push_back((uint8_t)v);
}

static void h264_append_escaped_rbsp(std::vector<uint8_t> *out, const std::vector<uint8_t>& rbsp) {
	int zero_count = 0;
	for (uint8_t b : rbsp) {
		if (zero_count == 2 && b <= 0x03) {
			out->push_back(0x03);
			zero_count = 0;
		}
		out->push_back(b);
		if (b == 0)
			zero_count++;
		else
			zero_count = 0;
	}
}

static std::vector<uint8_t> h264_unescape_rbsp(const uint8_t *src, size_t len) {
	std::vector<uint8_t> rbsp;
	rbsp.reserve(len);
	int zero_count = 0;
	for (size_t i = 0; i < len; i++) {
		uint8_t b = src[i];
		if (zero_count == 2 && b == 0x03) {
			zero_count = 0;
			continue;
		}
		rbsp.push_back(b);
		if (b == 0)
			zero_count++;
		else
			zero_count = 0;
	}
	return rbsp;
}

static bool h264_is_x264_info_sei(const H264Nal& nal) {
	if (nal.type != 6)
		return false;

	size_t payload_off = nal.prefix + 1;
	if (payload_off >= nal.bytes.size())
		return false;

	std::vector<uint8_t> rbsp = h264_unescape_rbsp(nal.bytes.data() + payload_off, nal.bytes.size() - payload_off);
	size_t off = 0;
	while (off + 1 < rbsp.size()) {
		size_t payload_type = 0;
		while (off < rbsp.size() && rbsp[off] == 0xFF) {
			payload_type += 255;
			off++;
		}
		if (off >= rbsp.size())
			break;
		payload_type += rbsp[off++];

		size_t payload_size = 0;
		while (off < rbsp.size() && rbsp[off] == 0xFF) {
			payload_size += 255;
			off++;
		}
		if (off >= rbsp.size())
			break;
		payload_size += rbsp[off++];

		if (payload_size > rbsp.size() - off)
			break;
		if (payload_type == 5 && payload_size >= sizeof(H264_X264_INFO_UUID) &&
				memcmp(rbsp.data() + off, H264_X264_INFO_UUID, sizeof(H264_X264_INFO_UUID)) == 0) {
			return true;
		}
		off += payload_size;
	}

	return false;
}

static std::vector<uint8_t> h264_make_max_slice_sei(uint32_t max_slice_size) {
	std::vector<uint8_t> payload;
	payload.reserve(28);
	payload.insert(payload.end(), H264_LD_BUFFER_UUID, H264_LD_BUFFER_UUID + sizeof(H264_LD_BUFFER_UUID));
	payload.insert(payload.end(), { 'L', 'D', 'S', 'Z' });
	payload.push_back(1); // payload version
	h264_append_be32(&payload, max_slice_size);

	std::vector<uint8_t> rbsp;
	rbsp.reserve(2 + payload.size() + 1);
	rbsp.push_back(5); // user_data_unregistered
	rbsp.push_back((uint8_t)payload.size());
	rbsp.insert(rbsp.end(), payload.begin(), payload.end());
	rbsp.push_back(0x80); // rbsp_trailing_bits

	std::vector<uint8_t> nal;
	nal.reserve(4 + 1 + rbsp.size() + 8);
	nal.insert(nal.end(), { 0x00, 0x00, 0x00, 0x01, 0x06 });
	h264_append_escaped_rbsp(&nal, rbsp);
	return nal;
}

static bool h264_write_nals(const std::string& path, const std::vector<H264Nal>& nals) {
	std::string tmp = path + ".tmp";
	FILE *out = fopen(tmp.c_str(), "wb");
	if (!out) return false;

	bool ok = true;
	for (const H264Nal& nal : nals) {
		if (fwrite(nal.bytes.data(), 1, nal.bytes.size(), out) != nal.bytes.size()) {
			ok = false;
			break;
		}
	}

	if (fclose(out) != 0)
		ok = false;
	if (!ok) {
		remove(tmp.c_str());
		return false;
	}
	if (rename(tmp.c_str(), path.c_str()) != 0) {
		remove(tmp.c_str());
		return false;
	}
	return true;
}

static void h264_strip_x264_info_sei(std::vector<H264Nal> *nals) {
	std::vector<H264Nal> filtered;
	filtered.reserve(nals->size());
	for (const H264Nal& nal : *nals) {
		if (h264_is_x264_info_sei(nal)) {
			continue;
		}
		filtered.push_back(nal);
	}
	*nals = filtered;
}

static void h264_insert_nal(std::vector<H264Nal> *nals, size_t idx, const std::vector<uint8_t>& bytes) {
	H264Nal nal;
	nal.prefix = 4;
	nal.type = bytes.size() > nal.prefix ? bytes[nal.prefix] & 0x1F : 0;
	nal.bytes = bytes;
	nals->insert(nals->begin() + idx, nal);
}

static bool h264_embed_max_slice_metadata(const std::string& path) {
	std::vector<uint8_t> data;
	if (!h264_read_file(path, &data))
		return false;
	std::vector<H264Nal> nals;
	if (!h264_split_nals(data, &nals))
		return false;
	h264_strip_x264_info_sei(&nals);

	size_t max_slice_size = 0;
	size_t insert_idx = (size_t)-1;

	for (size_t i = 0; i < nals.size(); i++) {
		const H264Nal& nal = nals[i];
		size_t nal_size = nal.bytes.size();
		if (nal_size > max_slice_size)
			max_slice_size = nal_size;
		if (insert_idx == (size_t)-1 && (nal.type == 1 || nal.type == 5))
			insert_idx = i;
	}

	if (insert_idx == (size_t)-1 || max_slice_size == 0)
		return false;
	if (max_slice_size > UINT32_MAX || max_slice_size > H264_PLAYER_BUF_SIZE)
		return false;

	std::vector<uint8_t> sei = h264_make_max_slice_sei((uint32_t)max_slice_size);
	h264_insert_nal(&nals, insert_idx, sei);

	if (!h264_write_nals(path, nals))
		return false;

	verbose(1, "H.264: embedded max_slice_size=%u", (unsigned)max_slice_size);
	return true;
}

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
	std::string x264_params = "no-deblock=1:no-info=1:slices=4";
	x264_params += cfg.debug_weightp ? ":weightp=1" : ":weightp=0";
	verbose(1, "H.264 debug weightp: %s", cfg.debug_weightp ? "on" : "off");

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
		// Force 4 slices per frame for easier background decoding
		"-x264-params", x264_params,
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

	cmd.insert(cmd.end(), cfg.ffmpeg_opts.begin(), cfg.ffmpeg_opts.end());
	cmd.push_back(er.video_path);

	progress_state_t ps = { .pass_count = 1, .start_ms = now_ms(), .last_draw_ms = 0 };
	int rc = run_ffmpeg_with_progress(cmd, ar.meta.duration, 0, ps);
	// Do not print a newline here: next phases (eg: Audio) should reuse the same line.
	// A single final newline is printed by main() when the whole pipeline is done.
	if (cfg.verbose == 0 && cfg.progress) { progressbar_clear(); }
	if (rc != 0) fatal("ffmpeg failed (rc=%d)", rc);

	if (!h264_embed_max_slice_metadata(er.video_path)) {
		fatal("failed to postprocess H.264 stream (max slice metadata SEI)");
	}
	return er;
}


