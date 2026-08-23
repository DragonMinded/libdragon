/*
    videoconv64: convert video files to formats used by the Libdragon SDK
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/

#include "videoconv64.h"

#include "../common/subprocess.h"
#include "../common/json.hpp"

#include <time.h>
#include <thread>
#include <mutex>
#include <deque>
#include <algorithm>
#include <climits>
#include <strings.h>

// Shared parsing helpers for --seek
#include "../common/seekfile.cpp"

using json = nlohmann::json;

Config cfg;

static const CodecInfo CODECS[] = {
	// NOTE: alignment is codec-specific and must be applied dynamically.
	{ "mpeg1", ".m1v", 32, 16 },
	// Placeholder for later implementation: we still expose the choice and requirements.
	{ "h264",  ".h264", 32, 16 },
};

static std::mutex log_mutex;

static std::mutex artifact_mutex;
static std::vector<std::string> artifacts;

void artifact_register(const std::string& path) {
	std::lock_guard<std::mutex> lock(artifact_mutex);
	artifacts.push_back(path);
}

void artifact_commit_all(void) {
	std::lock_guard<std::mutex> lock(artifact_mutex);
	artifacts.clear();
}

static void artifact_delete_all(void) {
	std::lock_guard<std::mutex> lock(artifact_mutex);
	for (const auto& p : artifacts)
		remove(p.c_str());
	artifacts.clear();
}

__attribute__((format(printf, 2, 3)))
void verbose(int level, const char *str, ...) {
	if (cfg.verbose < level) return;
	std::lock_guard<std::mutex> lock(log_mutex);
	va_list va;
	va_start(va, str);
	vfprintf(stderr, str, va);
	fprintf(stderr, "\n");
	va_end(va);
}

__attribute__((noreturn, format(printf, 1, 2)))
void fatal(const char *str, ...) {
	artifact_delete_all();
	std::lock_guard<std::mutex> lock(log_mutex);
	va_list va;
	va_start(va, str);
	vfprintf(stderr, str, va);
	fprintf(stderr, "\n");
	va_end(va);
	exit(1);
}

static void usage(void) {
	printf("videoconv64 -- Video conversion tool for libdragon\n");
	printf("\n");
	printf("Usage:\n");
	printf("   videoconv64 [flags] <input_file> [audio_or_subtitle_file ...]\n");
	printf("\n");
	printf("Options:\n");
	printf("   -o, --output <dir>          Specify output directory (default: same as input)\n");
	printf("   -v, --verbose               Verbose mode (repeatable)\n");
	printf("   -h, --help                  Show this help message\n");
	printf("\n");
	printf("Video options:\n");
	printf("   -c, --codec <mpeg1|h264>    Video codec to use (default: mpeg1)\n");
	printf("   -w, --width <N>             Target width (default: 320)\n");
	printf("                               Height is auto-calculated maintaining aspect ratio\n");
	printf("                               and enforcing codec-specific alignment.\n");
	printf("   -r, --fps <N>               Force a specific framerate (default: auto)\n");
	printf("   -q, --quality <0..100>      Synthetic quality scale (default: 80)\n");
	printf("   -Q, --quick                 Quick encoding (speed up processing as much as possible)\n");
	printf("       --seek <SEC|FILE>       Enable seeking support:\n");
	printf("                               - if SEC is a float, request a keyframe every SEC seconds\n");
	printf("                               - if FILE, read a list of seekpoints (one per line):\n");
	printf("                                 * integer frame indices, or\n");
	printf("                                 * timestamps in [hh:]mm:ss[.mmm] format\n");
	printf("\n");
	printf("Audio options:\n");
	printf("       --audio-compress <N>    Pass through to audioconv64: --wav-compress <N>\n");
	printf("       --no-audio              Disable audio extraction\n");
	printf("\n");
	printf("Advanced options:\n");
	printf("       --no-progress            Disable progress output\n");
	printf("       --deinterlace <auto|on|off>\n");
	printf("                                Control deinterlacing (default: auto)\n");
	printf("       --profile <auto|cartoon|film|noisy|none>\n");
	printf("                                Content profile for optimized filtering (default: auto)\n");
	printf("       --quant-matrix <n64|std> Quantization matrix to use (default: n64)\n");
	printf("       --audio-parms <R,C>      Audio params: RATE,CHANNELS (default: 32000,1)\n");
	printf("       --ffmpeg-opts <args>     Append raw ffmpeg args (repeatable, applied near output;\n");
	printf("                                space-separated tokens, quotes supported)\n");
	printf("       --ffmpeg-path <path>     Path to ffmpeg executable (default: ffmpeg)\n");
	printf("       --ffprobe-path <path>    Path to ffprobe executable (default: ffprobe)\n");
	printf("\n");
}

typedef enum {
	EXTRA_KIND_AUDIO = 1,
	EXTRA_KIND_SUBTITLE = 2,
} extra_kind_t;

static extra_kind_t classify_extra_file_with_ffprobe(const std::string &path) {
	// Files after the first positional must be standalone audio or subtitle files.
	// Reject any file that contains video, or that looks like a container (eg: mkv/webm).
	std::vector<std::string> cmd = {
		cfg.ffprobe_path,
		"-v", "error",
		"-show_entries", "format=format_name:stream=codec_type",
		"-of", "json",
		path,
	};
	std::string out;
	int rc = run_process(cmd, out);
	if (rc != 0) fatal("ffprobe failed for extra file: %s", path.c_str());

	json j;
	try {
		j = json::parse(out);
	} catch (const std::exception &e) {
		fatal("ffprobe JSON parse error for %s: %s", path.c_str(), e.what());
	}

	const json streams = j.value("streams", json::array());
	if (!streams.is_array() || streams.empty()) {
		fatal("Extra file not recognized by ffprobe (no streams): %s", path.c_str());
	}
	if (streams.size() != 1) {
		fatal("Extra file must have exactly 1 stream (not a container): %s", path.c_str());
	}

	const std::string ct = streams[0].value("codec_type", std::string());
	if (ct == "video") {
		fatal("Extra file must not be a video: %s", path.c_str());
	}
	if (ct == "audio") return EXTRA_KIND_AUDIO;
	if (ct == "subtitle") return EXTRA_KIND_SUBTITLE;

	fatal("Extra file has unsupported stream type '%s': %s", ct.c_str(), path.c_str());
}

static const CodecInfo *codec_info_from_name(const std::string& name) {
	for (size_t i = 0; i < sizeof(CODECS)/sizeof(CODECS[0]); i++)
		if (name == CODECS[i].name) return &CODECS[i];
	return nullptr;
}

int run_process_pipe(
	const std::vector<std::string>& argv,
	std::string *out,
	const std::function<void(const std::string&)> &cb
) {
	if (cfg.verbose >= 2) {
		verbose(2, "[exec] %s", format_cmdline_for_log(argv).c_str());
	}

	const char *tag = argv.empty() ? "proc" : argv[0].c_str();
	std::string prefix = std::string("[") + tag + "] ";

	std::vector<const char*> cargv;
	cargv.reserve(argv.size() + 1);
	for (size_t i = 0; i < argv.size(); i++)
		cargv.push_back(argv[i].c_str());
	cargv.push_back(NULL);

	struct subprocess_s proc;
	int options = subprocess_option_search_user_path | subprocess_option_enable_async | subprocess_option_combined_stdout_stderr;
	if (subprocess_create(cargv.data(), options, &proc) != 0) {
		return -1;
	}

	if (out) out->clear();
	std::string carry;
	char buf[2048];

	auto flush_line = [&](const std::string& line) {
		if (cfg.verbose >= 3) verbose(3, "%s%s", prefix.c_str(), line.c_str());
		if (cb) cb(line);
	};

	while (subprocess_alive(&proc)) {
		unsigned n = subprocess_read_stdout(&proc, buf, sizeof(buf));
		if (n == 0) {
			sleep_ms(50);
			continue;
		}
		if (out) out->append(buf, n);
		carry.append(buf, n);
		for (;;) {
			size_t nl = carry.find('\n');
			if (nl == std::string::npos) break;
			std::string line = carry.substr(0, nl);
			if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
			carry.erase(0, nl + 1);
			flush_line(line);
		}
	}

	// Drain any remaining output after the process exited.
	while (true) {
		unsigned n = subprocess_read_stdout(&proc, buf, sizeof(buf));
		if (n == 0) break;
		if (out) out->append(buf, n);
		carry.append(buf, n);
		for (;;) {
			size_t nl = carry.find('\n');
			if (nl == std::string::npos) break;
			std::string line = carry.substr(0, nl);
			if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
			carry.erase(0, nl + 1);
			flush_line(line);
		}
	}

	if (!carry.empty()) {
		if (!carry.empty() && carry[carry.size()-1] == '\r') carry.erase(carry.size()-1);
		flush_line(carry);
		carry.clear();
	}

	int rc = 0;
	if (subprocess_join(&proc, &rc) != 0) rc = -1;
	subprocess_destroy(&proc);
	return rc;
}

int run_process(const std::vector<std::string>& argv, std::string &out) {
	std::deque<std::string> tail_lines;
	const char *tag = argv.empty() ? "proc" : argv[0].c_str();
	std::string prefix = std::string("[") + tag + "] ";

	int rc = run_process_pipe(argv, &out, [&](const std::string& line) {
		const size_t MAX_TAIL_LINES = 80;
		if (tail_lines.size() >= MAX_TAIL_LINES) tail_lines.pop_front();
		tail_lines.push_back(line);
	});

	if (rc != 0 && cfg.verbose >= 1) {
		verbose(1, "[exec] %s", format_cmdline_for_log(argv).c_str());
		verbose(1, "[exit] rc=%d", rc);
		for (const auto& l : tail_lines)
			verbose(1, "%s%s", prefix.c_str(), l.c_str());
	} else {
		if (cfg.verbose >= 2) verbose(2, "[exit] rc=%d", rc);
	}

	return rc;
}

void check_tool_available(const std::string& tool_path, const char *tool_name) {
	std::string out;
	int rc = run_process({ tool_path, "-version" }, out);
	if (rc != 0) {
		fatal("%s not found or not executable: %s", tool_name, tool_path.c_str());
	}
	verbose(2, "%s OK: %s", tool_name, tool_path.c_str());
}

int main(int argc, char **argv) {
	winconsole_utf8();

	if (argc < 2) {
		usage();
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help") {
			usage();
			return 0;
		} else if (arg == "-v" || arg == "--verbose") {
			cfg.verbose++;
		} else if (arg == "-o" || arg == "--output") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.output_dir = argv[i];
		} else if (arg == "-c" || arg == "--codec") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.codec = argv[i];
			if (!codec_info_from_name(cfg.codec)) fatal("Unsupported codec: %s", cfg.codec.c_str());
		} else if (arg == "-w" || arg == "--width") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.width = atoi(argv[i]);
			if (cfg.width <= 0 || cfg.width >= 640) fatal("Invalid width: %d (expected 1..639)", cfg.width);
		} else if (arg == "-r" || arg == "--fps") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.fps = atof(argv[i]);
			if (cfg.fps < 1.0 || cfg.fps > 60.0) fatal("Invalid fps: %.3f (expected 1..60)", cfg.fps);
		} else if (arg == "--profile") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.profile = argv[i];
			if (!(cfg.profile == "auto" || cfg.profile == "cartoon" || cfg.profile == "film" || cfg.profile == "noisy" || cfg.profile == "none"))
				fatal("Invalid profile: %s", cfg.profile.c_str());
		} else if (arg == "-Q" || arg == "--quick") {
			cfg.quick = true;
		} else if (arg == "--debug-weightp") {
			cfg.debug_weightp = true;
		} else if (arg == "--seek") {
			if (++i >= argc) fatal("Missing argument for %s (expected seconds or file path)", arg.c_str());
			const char *param = argv[i];
			cfg.seek = true;
			double sec = 0.0;
			if (parse_double_strict(param, &sec) && sec > 0.0) {
				cfg.seek_interval_sec = sec;
			} else {
				cfg.seek_frames_file = param;
			}
		} else if (arg == "--no-progress") {
			cfg.progress = false;
		} else if (arg == "-q" || arg == "--quality") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.quality = atoi(argv[i]);
			if (cfg.quality < 0 || cfg.quality > 100)
				fatal("Invalid quality: %d (expected 0..100)", cfg.quality);
		} else if (arg == "--deinterlace") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.deinterlace = argv[i];
			if (!(cfg.deinterlace == "auto" || cfg.deinterlace == "on" || cfg.deinterlace == "off"))
				fatal("Invalid deinterlace: %s", cfg.deinterlace.c_str());
		} else if (arg == "--no-audio") {
			cfg.audio = false;
		} else if (arg == "--audio-compress") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.audio_compress = argv[i]; // no validation, audioconv64 will handle it
		} else if (arg == "--audio-parms") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			int rate = 0, ch = 0;
			if (sscanf(argv[i], "%d,%d", &rate, &ch) != 2) {
				fatal("Invalid --audio-parms (expected RATE,CHANNELS): %s", argv[i]);
			}
			cfg.audio_rate = rate;
			cfg.audio_channels = ch;
		} else if (arg == "--quant-matrix") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.quant_matrix = argv[i];
		} else if (arg == "--ffmpeg-opts") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			{
				std::vector<std::string> toks = split_shell_args(argv[i]);
				cfg.ffmpeg_opts.insert(cfg.ffmpeg_opts.end(), toks.begin(), toks.end());
			}
		} else if (arg == "--ffmpeg-path") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.ffmpeg_path = argv[i];
		} else if (arg == "--ffprobe-path") {
			if (++i >= argc) fatal("Missing argument for %s", arg.c_str());
			cfg.ffprobe_path = argv[i];
		} else if (!arg.empty() && arg[0] == '-') {
			fatal("Unknown option: %s", arg.c_str());
		} else {
			if (cfg.input_file.empty()) {
				cfg.input_file = arg;
			} else {
				// Defer classification (audio vs subtitles) until after we validated ffprobe exists.
				cfg.extra_files.push_back(arg);
			}
		}
	}

	if (cfg.input_file.empty()) fatal("No input file specified");

	const CodecInfo *ci = codec_info_from_name(cfg.codec);
	if (!ci) fatal("Internal error: codec missing");

	// Ensure tools exist and can run before proceeding any further.
	check_tool_available(cfg.ffmpeg_path, "ffmpeg");
	check_tool_available(cfg.ffprobe_path, "ffprobe");

	// Classify extra positional files via ffprobe.
	for (const auto &p : cfg.extra_files) {
		extra_kind_t k = classify_extra_file_with_ffprobe(p);
		if (k == EXTRA_KIND_AUDIO) cfg.audio_files.push_back(p);
		else if (k == EXTRA_KIND_SUBTITLE) cfg.subtitle_files.push_back(p);
	}
	cfg.extra_files.clear();

	// Start audio conversion early (runs in background while we analyze/encode video).
	// We can't do that if seek file generation is requested, as we need to wait for
	// video encoding to complete first to know the seek offsets.
	std::thread audio_thread;
	if (cfg.audio && !cfg.seek) {
		// Audio is running in parallel with video encoding; the visible progress is video-based,
		// but let the user know both are active.
		progressbar_set_mode(PROGRESS_MODE_VIDEO_AUDIO);
		audio_thread = std::thread([&]() {
			(void)vconv_audio_bridge({}, 0.0, false);
		});
	} else {
		// Video-only phase (either no audio or audio deferred).
		progressbar_set_mode(PROGRESS_MODE_VIDEO);
	}

	// Step 2: analysis (ffprobe/idet/signalstats) lives in vconv_analyze.cpp
	AnalysisResult ar = vconv_analyze(*ci);

	// Parse seek frames file now that we know the effective FPS (for timestamp conversion).
	if (cfg.seek && !cfg.seek_frames_file.empty()) {
		// Seek file semantics:
		// - Integers are INPUT frame indices.
		// - Timestamps are converted to INPUT frame indices using input FPS.
		// We then remap to OUTPUT frame indices if output FPS is overridden.
		const double fps_in = ar.meta.fps;
		const double fps_out = (cfg.fps > 0.0) ? cfg.fps : ar.out_fps;
		cfg.seek_frames = load_seek_frames_file(cfg.seek_frames_file, fps_in);

		// Remap input-frame indices -> output-frame indices if needed.
		if (fps_out != fps_in) {
			for (int &f : cfg.seek_frames) {
				double t = (double)f / fps_in;
				f = (int)round(t * fps_out);
			}

			// Normalize seek frames array again in case the remapping made
			// two seekpoints identical.
			cfg.seek_frames.erase(std::unique(cfg.seek_frames.begin(), cfg.seek_frames.end()), cfg.seek_frames.end());
		}
	}

	// Subtitles conversion can run in parallel with video encoding (and audio).
	// Start it after analysis so we already know output geometry/FPS for the SUB64 header.
	std::thread subtitles_thread([ar]() {
		vconv_process_subtitles(ar);
	});

	verbose(1, "Input: %s", cfg.input_file.c_str());
	verbose(1, "Output dir: %s", cfg.output_dir.empty() ? "(same as input)" : cfg.output_dir.c_str());
	verbose(1, "Codec: %s (ext=%s, align=%dx%d)", ci->name, ci->default_ext, ci->align_w, ci->align_h);
	verbose(1, "Width target: %d", cfg.width);
	verbose(1, "Mode: %s", cfg.quick ? "quick" : "quality (2-pass)");

	// Encoding
	EncodeResult er;
	if (cfg.codec == "mpeg1") {
		er = vconv_encode_mpeg1(*ci, ar);
	} else if (cfg.codec == "h264") {
		er = vconv_encode_h264(*ci, ar);
	} else {
		fatal("Codec not implemented yet: %s", cfg.codec.c_str());
	}
	verbose(1, "Output video: %s", er.video_path.c_str());

	if (cfg.seek) {
		std::vector<seek_point_t> pts = vconv_generate_seek(*ci, er.video_path);
		if (cfg.audio) {
			// If audio conversion was deferred due to seek generation, do it now.
			double fps = ar.out_fps;
			// Audio runs after video; show progress labeled as Audio.
			progressbar_set_mode(PROGRESS_MODE_AUDIO);
			audio_thread = std::thread([pts = std::move(pts), fps]() mutable {
				(void)vconv_audio_bridge(std::move(pts), fps, true);
			});
		}
	}
	
	// Sync subtitles thread (errors will abort via fatal()).
	if (subtitles_thread.joinable()) subtitles_thread.join();

	// Sync audio thread (errors will abort via fatal()).
	if (audio_thread.joinable()) audio_thread.join();

	// Everything went fine: the produced files can now be kept.
	artifact_commit_all();

	// If we used the interactive progress bar (stderr single-line updates),
	// end with a single newline so the shell prompt/logs start on a fresh line.
	if (cfg.verbose == 0 && cfg.progress) {
		progressbar_clear();
		fprintf(stderr, "\n");
	}
	return 0;
}
