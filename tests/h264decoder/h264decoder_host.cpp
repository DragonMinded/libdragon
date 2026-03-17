/*
    h264decoder_host: host H.264 decoder validation tool for Libdragon
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/
/*

    This tool accepts a H.264 raw bitstream and decodes it using the PC version
    of libdragon H.264 decoder. Then, it reencodes it in MP4 format using ffmpeg.

    The goal of this tool is to test the H.264 decoder in the PC, for debugging
    purposes. If eg. this tool correctly decodes a video, but N64 fails to,
    the bug is in the N64 port of the decoder (eg: RSP ucode). Otherwise, it has
    to be a bug in the C portion of the decoder.

*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>

extern "C" {
#define LODEPNG_NO_COMPILE_CPP
#include "lodepng.h"
#include "h264bsd_decoder.h"
#include "h264bsd_storage.h"
#include "h264bsd_util.h"
}

namespace fs = std::filesystem;

struct Options {
    fs::path input_path;
    fs::path output_mp4;
    std::string ffmpeg_path = "ffmpeg";
    bool verbose = false;
    bool debug = false;
    uint32_t max_frames = 0; // 0 = unlimited
};

static void usage() {
    fprintf(stderr,
        "Usage: h264decoder_host [options] <input.h264>\n"
        "Options:\n"
        "  -o, --output <file.mp4>   Output MP4 path (default: <input>.mp4)\n"
        "      --max-frames <N>      Decode at most N frames\n"
        "      --debug               Keep PNG frames in <input>_frames/\n"
        "  -v, --verbose             Verbose logs and progress\n"
        "      --ffmpeg-path <path>  ffmpeg executable (default: ffmpeg)\n"
        "  -h, --help                Show this help message\n");
}

static void vlog(const Options &opt, const char *fmt, ...) {
    if (!opt.verbose) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static bool parse_u32(const char *s, uint32_t &out) {
    if (!s || !*s) return false;
    char *end = nullptr;
    unsigned long v = strtoul(s, &end, 10);
    if (*end != '\0' || v > 0xFFFFFFFFul) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

static bool parse_args(int argc, char **argv, Options &opt) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" || arg == "--output") {
            if (++i >= argc) return false;
            opt.output_mp4 = argv[i];
        } else if (arg == "--max-frames") {
            if (++i >= argc) return false;
            if (!parse_u32(argv[i], opt.max_frames) || opt.max_frames == 0) return false;
        } else if (arg == "--debug") {
            opt.debug = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opt.verbose = true;
        } else if (arg == "--ffmpeg-path") {
            if (++i >= argc) return false;
            opt.ffmpeg_path = argv[i];
        } else if (arg == "-h" || arg == "--help") {
            usage();
            exit(0);
        } else if (!arg.empty() && arg[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg.c_str());
            return false;
        } else {
            if (!opt.input_path.empty()) {
                fprintf(stderr, "Too many positional arguments\n");
                return false;
            }
            opt.input_path = arg;
        }
    }

    if (opt.input_path.empty()) return false;

    if (opt.output_mp4.empty()) {
        fs::path base = opt.input_path;
        base.replace_extension(".mp4");
        opt.output_mp4 = base;
    }

    return true;
}

static std::vector<uint8_t> read_file(const fs::path &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open input file");
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (size <= 0) throw std::runtime_error("Input file is empty");
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char *>(data.data()), size)) {
        throw std::runtime_error("Failed to read input file");
    }
    return data;
}

static inline uint8_t clip_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<uint8_t>(v);
}

static void yuv420_to_rgb24(const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
                            int width, int height, std::vector<uint8_t> &rgb) {
    rgb.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);
    for (int j = 0; j < height; ++j) {
        int cj = j >> 1;
        for (int i = 0; i < width; ++i) {
            int ci = i >> 1;
            int yv = y[j * width + i];
            int uv = cb[cj * (width >> 1) + ci] - 128;
            int vv = cr[cj * (width >> 1) + ci] - 128;

            int r = static_cast<int>(yv + 1.402 * vv);
            int g = static_cast<int>(yv - 0.344136 * uv - 0.714136 * vv);
            int b = static_cast<int>(yv + 1.772 * uv);

            size_t o = (static_cast<size_t>(j) * static_cast<size_t>(width) + static_cast<size_t>(i)) * 3u;
            rgb[o + 0] = clip_u8(r);
            rgb[o + 1] = clip_u8(g);
            rgb[o + 2] = clip_u8(b);
        }
    }
}

static std::string shell_quote(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static double extract_framerate(storage_t *storage, bool *is_from_vui) {
    if (storage->activeSps &&
        storage->activeSps->vuiParametersPresentFlag &&
        storage->activeSps->vuiParameters &&
        storage->activeSps->vuiParameters->timingInfoPresentFlag) {
        uint32_t timeScale = storage->activeSps->vuiParameters->timeScale;
        uint32_t numUnitsInTick = storage->activeSps->vuiParameters->numUnitsInTick;
        if (numUnitsInTick > 0) {
            if (is_from_vui) *is_from_vui = true;
            return static_cast<double>(timeScale) / (2.0 * static_cast<double>(numUnitsInTick));
        }
    }
    if (is_from_vui) *is_from_vui = false;
    return 20.0;
}

static int run_ffmpeg_pack(const Options &opt, const fs::path &frame_dir, bool use_png, double fps) {
    fs::path pattern = frame_dir / (use_png ? "frame_%06d.png" : "frame_%06d.ppm");
    std::ostringstream cmd;
    cmd << shell_quote(opt.ffmpeg_path);
    if (!opt.verbose) {
        cmd << " -hide_banner -loglevel error -nostats";
    }
    cmd << " -y -framerate " << fps
        << " -i " << shell_quote(pattern.string())
        << " -c:v libx264 -preset veryfast -crf 10 -pix_fmt yuv420p "
        << shell_quote(opt.output_mp4.string());
    vlog(opt, "Running ffmpeg: %s", cmd.str().c_str());
    return std::system(cmd.str().c_str());
}

static unsigned encode_png_fastish_file(const char *filename, const unsigned char *rgb, unsigned w, unsigned h) {
    LodePNGState state;
    lodepng_state_init(&state);

    // Keep real compression but strongly bias for speed.
    state.encoder.auto_convert = 0;
    state.encoder.filter_palette_zero = 0;
    state.encoder.filter_strategy = LFS_ZERO;
    state.encoder.zlibsettings.btype = 2;
    state.encoder.zlibsettings.use_lz77 = 0;
    state.encoder.zlibsettings.windowsize = 256;
    state.encoder.zlibsettings.nicematch = 16;
    state.encoder.zlibsettings.lazymatching = 0;

    state.info_raw = lodepng_color_mode_make(LCT_RGB, 8);
    state.info_png.color = lodepng_color_mode_make(LCT_RGB, 8);

    unsigned char *out = NULL;
    size_t out_size = 0;
    unsigned error = lodepng_encode(&out, &out_size, rgb, w, h, &state);
    if (!error) error = lodepng_save_file(out, out_size, filename);
    if (out) free(out);
    lodepng_state_cleanup(&state);
    return error;
}

static bool write_ppm_file(const char *filename, const unsigned char *rgb, unsigned w, unsigned h) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    if (fprintf(f, "P6\n%u %u\n255\n", w, h) < 0) {
        fclose(f);
        return false;
    }
    size_t size = (size_t)w * (size_t)h * 3u;
    bool ok = fwrite(rgb, 1, size, f) == size;
    ok = ok && (fclose(f) == 0);
    return ok;
}

static void progress_update_default(double pct, uint32_t frames) {
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    const int width = 40;
    int filled = (int)((pct / 100.0) * width + 0.5);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    char bar[41];
    for (int i = 0; i < width; i++) bar[i] = (i < filled) ? '#' : '-';
    bar[width] = '\0';
    fprintf(stdout, "\rProgress [%s] %6.1f%%  frames=%u", bar, pct, frames);
    fflush(stdout);
}

static void progress_update_verbose(double pct, uint32_t frames, size_t consumed, size_t total) {
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    fprintf(stdout, "Progress: %6.1f%%  frames=%u  bytes=%zu/%zu\n", pct, frames, consumed, total);
    fflush(stdout);
}

struct PngDirGuard {
    fs::path path;
    bool keep = false;
    ~PngDirGuard() {
        if (!keep && !path.empty()) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

static fs::path create_temp_png_dir() {
    char tmpl[] = "/tmp/h264decoder_host_XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) throw std::runtime_error("Cannot create temporary directory");
    return fs::path(dir);
}

int main(int argc, char **argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        usage();
        return 2;
    }

    try {
        std::vector<uint8_t> bitstream = read_file(opt.input_path);

        PngDirGuard png_guard;
        if (opt.debug) {
            png_guard.path = opt.output_mp4.parent_path() / (opt.input_path.stem().string() + "_frames");
            png_guard.keep = true;
        } else {
            png_guard.path = create_temp_png_dir();
            png_guard.keep = false;
        }
        fs::create_directories(png_guard.path);
        fs::path out_parent = opt.output_mp4.parent_path();
        if (!out_parent.empty()) {
            fs::create_directories(out_parent);
        }

        storage_t storage;
        h264bsdInitStorage(&storage);
        u32 init_ret = h264bsdInit(&storage, 0);
        if (init_ret != HANTRO_OK) {
            fprintf(stderr, "h264bsdInit failed: %u\n", init_ret);
            return 1;
        }

        const uint8_t *cur = bitstream.data();
        size_t remaining = bitstream.size();
        uint32_t frame_idx = 0;
        int width = 0, height = 0;
        std::vector<uint8_t> rgb;
        double input_fps = 0.0;
        bool fps_from_vui = false;
        double last_progress_printed = -1.0;

        auto stop_at_max = [&]() -> bool {
            return opt.max_frames > 0 && frame_idx >= opt.max_frames;
        };

        auto dump_ready_pictures = [&]() -> bool {
            for (;;) {
                if (stop_at_max()) return true;
                u32 picId = 0, isIdrPic = 0, numErrMbs = 0;
                uint8_t *pic_data = h264bsdNextOutputPicture(&storage, &picId, &isIdrPic, &numErrMbs);
                if (!pic_data) break;

                if (width == 0 || height == 0) {
                    width = static_cast<int>(h264bsdPicWidth(&storage)) * 16;
                    height = static_cast<int>(h264bsdPicHeight(&storage)) * 16;
                    input_fps = extract_framerate(&storage, &fps_from_vui);
                    vlog(opt, "Stream info: %dx%d @ %.6f fps (%s)", width, height, input_fps, fps_from_vui ? "from VUI" : "default");
                }
                if (width <= 0 || height <= 0) {
                    fprintf(stderr, "Decoder dimensions unavailable\n");
                    return false;
                }
                if (numErrMbs != 0) {
                    fprintf(stderr, "Decoded picture has concealed MBs: %u\n", numErrMbs);
                }

                const uint8_t *base = reinterpret_cast<const uint8_t *>(pic_data);
                const uint8_t *y = base;
                const uint8_t *cb = y + width * height;
                const uint8_t *cr = cb + (width * height) / 4;
                yuv420_to_rgb24(y, cb, cr, width, height, rgb);

                char name[64];
                snprintf(name, sizeof(name), opt.debug ? "frame_%06u.png" : "frame_%06u.ppm", frame_idx++);
                fs::path out_path = png_guard.path / name;
                if (opt.debug) {
                    unsigned enc = encode_png_fastish_file(out_path.string().c_str(), rgb.data(), (unsigned)width, (unsigned)height);
                    if (enc) {
                        fprintf(stderr, "PNG encode failed (%u): %s\n", enc, lodepng_error_text(enc));
                        return false;
                    }
                } else {
                    bool ok = write_ppm_file(out_path.string().c_str(), rgb.data(), (unsigned)width, (unsigned)height);
                    if (!ok) {
                        fprintf(stderr, "PPM write failed: %s\n", out_path.string().c_str());
                        return false;
                    }
                }
            }
            return true;
        };

        while (remaining > 0 && !stop_at_max()) {
            u32 num_read = 0;
            u32 status = h264bsdDecode(&storage, const_cast<uint8_t *>(cur), static_cast<u32>(remaining), frame_idx, &num_read);
            if (num_read == 0) {
                if (status != H264BSD_HDRS_RDY) {
                    fprintf(stderr, "Decoder consumed 0 bytes (stuck), status=%u\n", status);
                    h264bsdShutdown(&storage);
                    return 1;
                }
            } else {
                cur += num_read;
                remaining -= num_read;
            }

            if (status == H264BSD_ERROR || status == H264BSD_PARAM_SET_ERROR || status == H264BSD_MEMALLOC_ERROR) {
                fprintf(stderr, "h264bsdDecode failed: status=%u\n", status);
                h264bsdShutdown(&storage);
                return 1;
            }

            if (!dump_ready_pictures()) {
                h264bsdShutdown(&storage);
                return 1;
            }

            size_t consumed = bitstream.size() - remaining;
            double decode_pct = bitstream.empty() ? 100.0 : (100.0 * (double)consumed / (double)bitstream.size());
            if (decode_pct > 99.0 && !stop_at_max()) decode_pct = 99.0;
            if (opt.verbose) {
                if (decode_pct >= last_progress_printed + 2.0 || stop_at_max()) {
                    progress_update_verbose(decode_pct, frame_idx, consumed, bitstream.size());
                    last_progress_printed = decode_pct;
                }
            } else {
                if (decode_pct >= last_progress_printed + 0.5 || stop_at_max()) {
                    progress_update_default(decode_pct, frame_idx);
                    last_progress_printed = decode_pct;
                }
            }
        }

        if (!stop_at_max()) {
            h264bsdFlushBuffer(&storage);
            if (!dump_ready_pictures()) {
                h264bsdShutdown(&storage);
                return 1;
            }
        }

        h264bsdShutdown(&storage);

        if (frame_idx == 0) {
            fprintf(stderr, "No frames decoded\n");
            return 1;
        }
        if (input_fps <= 0.0) input_fps = 20.0;

        if (opt.verbose) {
            fprintf(stdout, "Progress: 100.0%%  frames=%u  decode complete, packing MP4...\n", frame_idx);
            fflush(stdout);
        } else {
            progress_update_default(100.0, frame_idx);
            fprintf(stdout, "\n");
            fflush(stdout);
        }

        int ff_rc = run_ffmpeg_pack(opt, png_guard.path, opt.debug, input_fps);
        if (ff_rc != 0) {
            fprintf(stderr, "ffmpeg failed with code %d\n", ff_rc);
            return 1;
        }

        vlog(opt, "Wrote MP4: %s", opt.output_mp4.string().c_str());
        if (opt.debug) {
            vlog(opt, "Kept PNG frames in: %s", png_guard.path.string().c_str());
        }
    } catch (const std::exception &e) {
        fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }

    return 0;
}
