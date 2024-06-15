
// Bring in tex_format_t definition
#include "surface.h"
#include "sprite.h"

static std::string codepoint_to_utf8(uint32_t codepoint) {
    std::string utf8;
    if (codepoint <= 0x7F) {
        utf8.push_back(codepoint);
    } else if (codepoint <= 0x7FF) {
        utf8.push_back(0xC0 | (codepoint >> 6));
        utf8.push_back(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        utf8.push_back(0xE0 | (codepoint >> 12));
        utf8.push_back(0x80 | ((codepoint >> 6) & 0x3F));
        utf8.push_back(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        utf8.push_back(0xF0 | (codepoint >> 18));
        utf8.push_back(0x80 | ((codepoint >> 12) & 0x3F));
        utf8.push_back(0x80 | ((codepoint >> 6) & 0x3F));
        utf8.push_back(0x80 | (codepoint & 0x3F));
    }
    return utf8;
}

// An owned image bitmap, supporting multiple texture formats for dynamic conversions
struct Image {
    tex_format_t fmt;
    std::vector<uint8_t> pixels;
    std::vector<uint16_t> palette;
    int w, h;

    Image() {
        fmt = FMT_NONE;
        w = h = 0;
    }

    Image(tex_format_t fmt, int w, int h, uint8_t *px = NULL)
        : fmt(fmt), w(w), h(h)
    {
        pixels.resize(w * h * TEX_FORMAT_BITDEPTH(fmt)/8);
        if (px)
            memcpy(pixels.data(), px, pixels.size());
        else
            memset(pixels.data(), 0, pixels.size());
    }

    Image(const Image& img) : fmt(img.fmt), pixels(img.pixels), palette(img.palette), w(img.w), h(img.h) {}
    Image(const Image&& img) : fmt(img.fmt), pixels(std::move(img.pixels)), palette(std::move(img.palette)), w(img.w), h(img.h) {}
    Image& operator=(const Image& img) {
        fmt = img.fmt;
        pixels = img.pixels;
        w = img.w;
        h = img.h;
        return *this;
    }

    struct Pixel {
        tex_format_t fmt;
        uint8_t *data;
        uint16_t *palette;

        bool is_transparent() {
            switch (fmt) {
            case FMT_RGBA32:
                return data[3] == 0;
            case FMT_RGBA16:
                return (data[1] & 1) == 0;
            case FMT_IA16:
                return data[1] == 0;
            case FMT_I8:
            case FMT_CI8:
                return *data == 0;
            default:
                assert(!"unsupported format");
                return false;
            }
        }

        uint32_t to_rgba32() {
            switch (fmt) {
            case FMT_RGBA32:
                return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            case FMT_RGBA16: {
                uint16_t val = (data[0] << 8) | data[1];
                uint32_t r = (val >> 11) & 0x1F;
                uint32_t g = (val >> 6) & 0x1F;
                uint32_t b = (val >> 1) & 0x1F;
                uint32_t a = (val & 1) * 0xFF;
                r = (r << 3) | (r >> 2);
                g = (g << 3) | (g >> 2);
                b = (b << 3) | (b >> 2);
                return (r << 24) | (g << 16) | (b << 8) | a;
            }
            case FMT_I8: {
                uint32_t i = *data;
                return (i << 24) | (i << 16) | (i << 8) | i;
            }
            case FMT_IA16: {
                uint32_t i = data[0];
                uint32_t a = data[1];
                return (i << 24) | (i << 16) | (i << 8) | a;
            }
            case FMT_CI8: {
                if (!palette) {
                    uint32_t i = *data;
                    return (i << 24) | (i << 16) | (i << 8) | i;
                }
                uint16_t val = palette[*data];
                uint32_t r = (val >> 11) & 0x1F;
                uint32_t g = (val >> 6) & 0x1F;
                uint32_t b = (val >> 1) & 0x1F;
                uint32_t a = (val & 1) * 0xFF;
                r = (r << 3) | (r >> 2);
                g = (g << 3) | (g >> 2);
                b = (b << 3) | (b >> 2);
                return (r << 24) | (g << 16) | (b << 8) | a;
            }
            default:
                assert(!"unsupported format");
                return 0;
            }
        }

        void set_from_rgba32(uint32_t px) {
            uint32_t r = (px >> 24) & 0xFF;
            uint32_t g = (px >> 16) & 0xFF;
            uint32_t b = (px >> 8) & 0xFF;
            uint32_t a = (px >> 0) & 0xFF;
            switch (fmt) {
            case FMT_RGBA32: 
                data[0] = r;
                data[1] = g;
                data[2] = b;
                data[3] = a;
                break;
            case FMT_RGBA16: {
                r = (r >> 3) & 0x1F;
                g = (g >> 3) & 0x1F;
                b = (b >> 3) & 0x1F;
                uint16_t val = (r << 11) | (g << 6) | (b << 1) | a;
                data[0] = val >> 8;
                data[1] = val & 0xFF;
                break;
            }
            case FMT_I8: {
                data[0] = a;
            }   break;
            case FMT_IA16: {
                assert(r == g && g == b);
                uint16_t val = (r << 8) | a;
                data[0] = val >> 8;
                data[1] = val & 0xFF;
            }   break;
            case FMT_CI8: {
                assert(px < 256);
                data[0] = px;
            }   break;
            default:
                assert(!"unsupported format");
                break;
            }
        }

        Pixel& operator=(Pixel px) {
            if (px.fmt == fmt)
                memcpy(data, px.data, TEX_FORMAT_BITDEPTH(fmt)/8);
            else
                set_from_rgba32(px.to_rgba32());
            return *this;
        }

        Pixel& operator=(uint32_t px) {
            set_from_rgba32(px);
            return *this;
        }
    };

    struct Line {
        uint8_t *data;
        int w;
        tex_format_t fmt;
        uint16_t *palette;

        Image::Pixel operator[](int x) {
            return {fmt, data + TEX_FORMAT_PIX2BYTES(fmt, x), palette};
        }

        void copy(Line src, int x0) {
            assert(x0 + src.w <= w);
            if (src.fmt == fmt)
                memcpy(data + TEX_FORMAT_PIX2BYTES(fmt, x0), src.data, TEX_FORMAT_PIX2BYTES(fmt, src.w));
            else {
                for (int x = 0; x < src.w; x++)
                    (*this)[x0 + x] = src[x];
            }
        }
    };

    Line operator[](int y) {
        return {pixels.data() + TEX_FORMAT_PIX2BYTES(fmt, y * w), w, fmt, palette.data()};
    }

    void copy(Image img, int x0, int y0) {
        assert(x0 + img.w <= w && y0 + img.h <= h);
        for (int y = 0; y < img.h; y++)
            (*this)[y0 + y].copy(img[y], x0);
    }

    void copy_rotated(Image img, int x0, int y0) {
        assert(x0 + img.h <= w && y0 + img.w <= h);
        for (int y = 0; y < img.h; y++)
            for (int x = 0; x < img.w; x++)
                (*this)[y0 + x][x0 + y] = img[y][x];
    }

    Image convert(tex_format_t new_fmt) {
        Image img(new_fmt, w, h);
        for (int y = 0; y < h; y++) {
            Line src = (*this)[y];
            Line dst = img[y];
            for (int x = 0; x < w; x++)
                dst[x] = src[x];
        }
        return img;
    }

    Image crop(int x0, int y0, int w, int h) {
        Image img(fmt, w, h);
        for (int y = 0; y < h; y++) {
            Line src = (*this)[y0 + y];
            Line dst = img[y];
            for (int x = 0; x < w; x++)
                dst[x] = src[x0 + x];
        }
        return img;
    }

    Image crop_transparent(int *ox0, int *oy0) {
        if (w == 0 && h == 0) {
            *ox0 = *oy0 = 0;
            return *this;
        }

        int x0 = w, y0 = h, x1 = 0, y1 = 0;
        for (int y = 0; y < h; y++) {
            Line line = (*this)[y];
            for (int x = 0; x < w; x++) {
                if (!line[x].is_transparent()) {
                    x0 = std::min(x0, x);
                    y0 = std::min(y0, y);
                    x1 = std::max(x1, x);
                    y1 = std::max(y1, y);
                }
            }
        }
        *ox0 = x0;
        *oy0 = y0;
        return crop(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    }

    template <typename F>
    void for_each_pixel(F f) {
        for (int y = 0; y < h; y++) {
            Line line = (*this)[y];
            for (int x = 0; x < w; x++)
                f(line[x]);
        }
    }

    void write_png(const char *fn) {
        const auto &img = fmt == FMT_RGBA32 ? *this : convert(FMT_RGBA32);
        lodepng_encode32_file(fn, img.pixels.data(), img.w, img.h);
    }
};

// A Glyph to be added to the font
struct Glyph {
    int gidx;               // index in the glyph array in font64
    uint32_t codepoint;     // unicode codepoint
    Image img;              // Pixel image (see add_glyph for valid formats)
    int xoff, yoff;
    int xadv;

    Glyph(int idx, uint32_t cp, Image&& img, int xoff, int yoff, int xadv)
        : gidx(idx), codepoint(cp), img(img), xoff(xoff), yoff(yoff), xadv(xadv) {}
};

struct Font {
    struct Kerning { int glyph1, glyph2, kerning; };

    rdpq_font_t *fnt = nullptr;
    std::vector<Glyph> glyphs;
    std::vector<Kerning> kernings;
    int num_atlases = 0;
    std::string outfn;
    bool is_mono = true;
    bool has_outline = false;

    Font(std::string fn, int point_size, int ascent, int descent, int line_gap, int space_width, bool outline)
    {
        outfn = fn;
        fnt = (rdpq_font_t*)calloc(1, sizeof(rdpq_font_t));
        memcpy(fnt->magic, FONT_MAGIC, 3);
        fnt->version = 6;
        fnt->flags = FONT_TYPE_ALIASED;
        fnt->point_size = point_size;
        fnt->ascent = ascent;
        fnt->descent = descent;
        fnt->line_gap = line_gap;
        fnt->space_width = space_width;
        has_outline = outline;
    }

    ~Font()
    {
        for (int i=0;i<fnt->num_atlases;i++)
            free(fnt->atlases[i].sprite);
        free(fnt->atlases);
        free(fnt->glyphs);
        free(fnt->ranges);
        free(fnt);
    }

    int get_glyph_index(uint32_t cp);
    void write(FILE *out);

    void add_range(int first, int last);
    int add_glyph(uint32_t cp, Image&& img, int xoff, int yoff, int xadv);
    void add_atlas(Image& img);
    void add_kerning(int glyph1, int glyph2, int kerning);
    void add_ellipsis(int ellipsis_cp, int ellipsis_repeats);

    void make_atlases(void);
    void make_kernings(void);

    void write(void);
};


void Font::write()
{
    FILE *out = fopen(outfn.c_str(), "wb");
    if (!out) {
        fprintf(stderr, "Error: cannot open output file %s\n", outfn.c_str());
        exit(1);
    }

    // Write header
    w8(out, fnt->magic[0]);
    w8(out, fnt->magic[1]);
    w8(out, fnt->magic[2]);
    w8(out, fnt->version);
    w32(out, fnt->flags);
    w32(out, fnt->point_size);
    w32(out, fnt->ascent);
    w32(out, fnt->descent);
    w32(out, fnt->line_gap);
    w32(out, fnt->space_width);
    w16(out, fnt->ellipsis_width);
    w16(out, fnt->ellipsis_glyph);
    w16(out, fnt->ellipsis_reps);
    w16(out, fnt->ellipsis_advance);
    w32(out, fnt->num_ranges);
    w32(out, fnt->num_glyphs);
    w32(out, fnt->num_atlases);
    w32(out, fnt->num_kerning);
    // Write builtin style
    w32(out, 1);   // num styles
    uint32_t offset_builtin_style = ftell(out);
    w32(out, 0xFFFFFFFF); // color
    w32(out, 0x40404040); // outline
    w32(out, 0); // runtime pointer

    int off_placeholders = ftell(out);
    w32(out, (uint32_t)0); // placeholder
    w32(out, (uint32_t)0); // placeholder
    w32(out, (uint32_t)0); // placeholder
    w32(out, (uint32_t)0); // placeholder
    w32(out, (uint32_t)0); // placeholder

    // Write ranges
    uint32_t offset_ranges = ftell(out);
    for (int i=0; i<fnt->num_ranges; i++)
    {
        w32(out, fnt->ranges[i].first_codepoint);
        w32(out, fnt->ranges[i].num_codepoints);
        w32(out, fnt->ranges[i].first_glyph);
    }

    // Write glyphs, aligned to 16 bytes. This makes sure
    // they cover exactly one data cacheline in R4300, so that
    // they each drawn glyph dirties exactly one line.
    walign(out, 16);
    uint32_t offset_glypes = ftell(out);
    for (int i=0; i<fnt->num_glyphs; i++)
    {
        w16(out, fnt->glyphs[i].xadvance);
        w8(out, fnt->glyphs[i].xoff);
        w8(out, fnt->glyphs[i].yoff);
        w8(out, fnt->glyphs[i].xoff2);
        w8(out, fnt->glyphs[i].yoff2);
        w8(out, fnt->glyphs[i].s);
        w8(out, fnt->glyphs[i].t);
        w8(out, fnt->glyphs[i].natlas);
        w8(out, fnt->glyphs[i].ntile);
        for (int j=0;j<2;j++) w8(out, (uint8_t)0);
        w16(out, fnt->glyphs[i].kerning_lo);
        w16(out, fnt->glyphs[i].kerning_hi);
    }

    // Write atlases
    walign(out, 16);
    uint32_t offset_atlases = ftell(out);
    int* offset_atlases_sprites = (int*)alloca(sizeof(int) * fnt->num_atlases);
    for (int i=0; i<fnt->num_atlases; i++)
    {
        offset_atlases_sprites[i] = w32_placeholder(out);
        w32(out, fnt->atlases[i].size);
        w32(out, 0);
    }

    // Write kernings
    walign(out, 16);
    uint32_t offset_kernings = ftell(out);
    for (int i=0; i<fnt->num_kerning; i++)
    {
        w16(out, fnt->kerning[i].glyph2);
        w8(out, fnt->kerning[i].kerning);
    }

    // Write bytes
    for (int i=0; i<fnt->num_atlases; i++)
    {
        walign(out, 16); // align sprites to 16 bytes
        w32_at(out, offset_atlases_sprites[i], ftell(out));
        fwrite(fnt->atlases[i].sprite, fnt->atlases[i].size, 1, out);
    }
    uint32_t offset_end = ftell(out);

    // Write offsets
    fseek(out, off_placeholders, SEEK_SET);
    w32(out, offset_ranges);
    w32(out, offset_glypes);
    w32(out, offset_atlases);
    w32(out, offset_kernings);
    w32(out, offset_builtin_style);

    fseek(out, offset_end, SEEK_SET);

    fclose(out);
}

void Font::add_range(int first, int last)
{
    // Check that the range does not overlap an existing one
    for (int i=0;i<fnt->num_ranges;i++)
    {
        if (first >= fnt->ranges[i].first_codepoint && first < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
        {
            fprintf(stderr, "Error: range 0x%04x-0x%04x overlaps with existing range 0x%04x-0x%04x\n", first, last, fnt->ranges[i].first_codepoint, fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints - 1);
            exit(1);
        }
        if (last >= fnt->ranges[i].first_codepoint && last < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
        {
            fprintf(stderr, "Error: range 0x%04x-0x%04x overlaps with existing range 0x%04x-0x%04x\n", first, last, fnt->ranges[i].first_codepoint, fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints - 1);
            exit(1);
        }
    }
    fnt->ranges = (range_t*)realloc(fnt->ranges, (fnt->num_ranges + 1) * sizeof(range_t));
    fnt->ranges[fnt->num_ranges].first_codepoint = first;
    fnt->ranges[fnt->num_ranges].num_codepoints = last - first + 1;
    fnt->ranges[fnt->num_ranges].first_glyph = fnt->num_glyphs;
    fnt->num_ranges++;
    fnt->glyphs = (glyph_t*)realloc(fnt->glyphs, (fnt->num_glyphs + last - first + 1) * sizeof(glyph_t));
    memset(fnt->glyphs + fnt->num_glyphs, 0, (last - first + 1) * sizeof(glyph_t));
    fnt->num_glyphs += last - first + 1;
}

int Font::get_glyph_index(uint32_t cp)
{
    for (int i=0;i<fnt->num_ranges;i++) {
        if (cp >= fnt->ranges[i].first_codepoint && cp < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
            return fnt->ranges[i].first_glyph + cp - fnt->ranges[i].first_codepoint;
    }
    return -1;
}

int Font::add_glyph(uint32_t cp, Image&& img, int xoff, int yoff, int xadv)
{
    int gidx = get_glyph_index(cp);
    if (gidx < 0) {
        assert(!"codepoint not in range");
        return -1;
    }

    if (has_outline) {
        // Outline fonts are IA16. Intensity goes between 0x00 for the outline
        // to 0xFF for the fill, while the alpha channel is the coverage of each pixel.
        // Outline monochromatic fonts have intensity fixed to 0xFF.
        if (img.fmt != FMT_IA16) assert(!"glyph image must be IA16 for outlined fonts");
    } else {
        // Non-outline fonts can be monochromatic or aliased, and must be I8 in both cases.
        // The monochromatic property is deduced by the glyphs pixels so that the user
        // doesn't have to specify it to benefits from 1bpp size reduction.
        if (img.fmt != FMT_I8) assert(!"glyph image must be I8 for non-outlined fonts");
    }

    // Check if the font is still mono
    bool was_mono = is_mono;
    if (was_mono) {
        img.for_each_pixel([&](Image::Pixel&& px) {
            switch (img.fmt) {
            case FMT_I8:
                if (px.data[0] > 0 && px.data[0] < 0xF0)
                    is_mono = false;
                break;
            case FMT_IA16:
                if (px.data[0] != 0 && px.data[1] != 0x00 && px.data[1] != 0xFF)
                    is_mono = false;
                break;
            default:
                assert(!"unsupported format");
            }
        });
        if (was_mono != is_mono && num_atlases > 0) 
            assert(!"cannot mix mono and non-mono glyphs in the same font in different ranges");
    }

    // Crop the image to the actual glyph size
    int x0=0, y0=0;
    img = img.crop_transparent(&x0, &y0);

    glyphs.push_back(Glyph(gidx, cp, std::move(img), xoff + x0, yoff + y0, xadv));
    return gidx;
}

void Font::make_atlases(void)
{
    if (num_atlases == 0) {
        // First call, time to decide the format of the font
        fnt->flags &= ~FONT_FLAG_TYPE_MASK;
        if (is_mono) {
            if (has_outline) {
                if (flag_verbose) fprintf(stderr, "monochrome+outlined glyphs detected (format: 2bpp)\n");
                fnt->flags |= FONT_TYPE_MONO_OUTLINE;
            } else {
                if (flag_verbose) fprintf(stderr, "monochrome glyphs detected (format: 1bpp)\n");
                fnt->flags |= FONT_TYPE_MONO;
            }
        } else {
            if (has_outline) {
                if (flag_verbose) fprintf(stderr, "aliased+outlined glyphs detected (format: 8 bpp)\n");
                fnt->flags |= FONT_TYPE_ALIASED_OUTLINE;
            } else {
                if (flag_verbose) fprintf(stderr, "aliased glyphs detected (format: 4 bpp)\n");
                fnt->flags |= FONT_TYPE_ALIASED;
            }
        }
    }

    // Determine how many different layers the final atlases will be:
    //  Aliased font: single layer (either I4 or IA8, depending on outline)
    //  Mono, no outline: we can use 1bpp, so we can merge 4 layers
    //  Mono, outline: we can use 2bpp, so we can merge 2 layers
    int merge_layers = !is_mono ? 1 : (has_outline ? 2 : 4);

    // Pack the glyphs into a texture
    rect_pack::Settings settings;
    memset(&settings, 0, sizeof(settings));
    settings.method = rect_pack::Method::Best;
    settings.method = rect_pack::Method::Best;
    settings.max_width = 128;
    settings.max_height = 64;
    settings.border_padding = 0;
    settings.allow_rotate = false;
    
    std::vector<rect_pack::Size> sizes;
    std::vector<rect_pack::Sheet> sheets;
    for (int i=0; i<glyphs.size(); i++) {
        if (glyphs[i].img.w == 0 || glyphs[i].img.h == 0) continue;
        rect_pack::Size size;
        size.id = i;
        size.width = glyphs[i].img.w + settings.border_padding;
        size.height = glyphs[i].img.h + settings.border_padding;
        sizes.push_back(size);
    }

    if (!is_mono) {
        // Aliased font: pack into I4 (max 128x64).
        // Outline not supported yet.
        if (!has_outline) {
            settings.max_width = 128;
            settings.max_height = 64;
        } else {
            settings.max_width = 64;
            settings.max_height = 64;
        }
        sheets = rect_pack::pack(settings, sizes);
    } else {
        // Start by computing a pack with the CI4 maximum size (64x64)
        settings.min_width = 64;
        settings.max_width = 64;
        settings.max_height = 64;
        sheets = rect_pack::pack(settings, sizes);
        int num_sheets = sheets.size();
        int last_group = (num_sheets-1) / merge_layers * merge_layers;

        // Try to optimize the last group (up to four sheets). Create an array
        // of input sizes for all the glyphs in the last group
        std::vector<rect_pack::Size> sizes2;
        for (int i=last_group; i<num_sheets; i++) {
            rect_pack::Sheet& sheet = sheets[i];
            for (auto &r : sheet.rects)
                sizes2.push_back(sizes[r.id]);
        }

        // Move the last group of sheets to a temporary array
        int best_area = 64*64;
        std::vector<rect_pack::Sheet> best_sheets;
        for (int i=last_group; i<num_sheets; i++) {
            best_sheets.push_back(sheets.back());
            sheets.pop_back();
        }

        if (flag_verbose >= 2)
            fprintf(stderr, "packing last group of %zu sheets\n", best_sheets.size());

        // Try to find a better packing for the last group
        while (1) {
            bool changed = false;
            for (int h=16; h<=64; h++) {
                // Find texture sizes where the value is a multiple of 16. Since
                // They are going to be packed as CI4, this allows the stride to be
                // multiple of 8, which allows LOAD_BLOCK to be used at runtime.
                int w = (best_area-1) / h / 16 * 16;
                if (!w) break;

                settings.min_width = 0;
                settings.max_width = w;
                settings.max_height = h;
                std::vector<rect_pack::Sheet> new_sheets = rect_pack::pack(settings, sizes2);
                if (new_sheets.size() <= merge_layers) {
                    if (flag_verbose >= 2)
                        printf("    found better packing: %d x %d (%d)\n", w, h, w*h);
                    best_sheets = new_sheets;
                    best_area = w * h;
                    changed = true;
                    break;
                }
            } 
            if (!changed) break;
        }

        // Append the best sheets to the calculated sheets
        sheets.insert(sheets.end(), best_sheets.begin(), best_sheets.end());
    }

    // Create the actual textures
    std::vector<Image> atlases;
    for (int i=0; i<sheets.size(); i++) {
        rect_pack::Sheet& sheet = sheets[i];

        Image img(FMT_IA16, sheet.width, sheet.height);

        for (int j=0; j<sheet.rects.size(); j++) {
            rect_pack::Rect& rect = sheet.rects[j];
            Glyph& glyph = glyphs[rect.id];

            if (!rect.rotated) {
                img.copy(glyph.img, rect.x, rect.y);
            } else {
                img.copy_rotated(glyph.img, rect.x, rect.y);
            }

            glyph_t *gout = &fnt->glyphs[glyph.gidx];
            gout->natlas = i;
            if (is_mono) {
                gout->ntile = i & (merge_layers-1);
                gout->natlas /= merge_layers;
            }
            gout->s = rect.x; gout->t = rect.y;
            gout->xoff = glyph.xoff;
            gout->yoff = glyph.yoff;
            gout->xoff2 = gout->xoff + glyph.img.w - 1;
            gout->yoff2 = gout->yoff + glyph.img.h - 1;
            gout->xadvance = glyph.xadv;

            if (flag_verbose >= 2) {
                fprintf(stderr, "  glyph %s [U+%04X]: %d x %d, %d,%d %d,%d %.2f\n", 
                    codepoint_to_utf8(glyph.codepoint).c_str(), glyph.codepoint, 
                    glyph.img.w, glyph.img.h, gout->xoff, gout->yoff, gout->xoff2, gout->yoff2, glyph.xadv/64.f);
            }

            if(abs(gout->xoff) > 128 || abs(gout->yoff) > 128 || abs(gout->xoff2) > 128 || abs(gout->yoff2) > 128 ||
                abs(gout->xadvance) > 32768)
            {
                fprintf(stderr, "ERROR: font too big, please reduce point size (%d)\n", fnt->point_size);
                exit(1);
            }
        }

        if (flag_verbose && !is_mono)
            fprintf(stderr, "created atlas %d: %d x %d pixels (%zu glyphs)\n", i, sheet.width, sheet.height, sheet.rects.size());
        if (flag_debug) {
            char *imgfn = NULL;
            asprintf(&imgfn, "%s_%d.png", outfn.c_str(), num_atlases);
            if (img.fmt == FMT_CI8) {
                img.palette.resize(3);
                img.palette[0] = 0;
                img.palette[1] = (31<<11) | (31<<6) | (31<<1) | 1;
                img.palette[2] = (10<<11) | (10<<6) | (10<<1) | 1;
            }
            img.write_png(imgfn);
            if (flag_verbose)
                fprintf(stderr, "wrote debug image: %s\n", imgfn);
            free(imgfn);
        }

        atlases.push_back(std::move(img));
        num_atlases++;
    }

    if (is_mono) {
        assert(merge_layers == 2 || merge_layers == 4);
        std::vector<Image> atlases2;
        for (int i=0; i<atlases.size(); i+=merge_layers) {
            // Merge (up to) 4 images into a single atlas. Calculate the
            // size of this group.
            int w = 0, h = 0;
            for (int j=0; j<merge_layers && i+j<atlases.size(); j++) {
                w = std::max(w, atlases[i+j].w);
                h = std::max(h, atlases[i+j].h);
            }

            // Create a new image with the size of the group
            Image img(FMT_CI8, w, h);

            // Merge the four images as four bitplanes
            for (int j=0; j<merge_layers && i+j<atlases.size(); j++) {
                Image& img2 = atlases[i+j];
                for (int y=0; y<img2.h; y++) {
                    for (int x=0; x<img2.w; x++) {
                        if (merge_layers == 4) {
                            uint8_t px = img2[y][x].is_transparent() ? 0 : 1;
                            *img[y][x].data |= px << (3-j);
                        } else {
                            uint32_t rgba32 = img2[y][x].to_rgba32();
                            uint8_t a = rgba32 & 0xFF;
                            uint8_t i = (rgba32 >> 8) & 0xFF;
                            uint8_t px = 0;
                            if (a) { if (i > 0x80) px = 1; else px = 2; }
                            *img[y][x].data |= px << ((1-j)*2);
                        }
                    }
                }
            }

            // We will treat this image as a CI4 image, and we will use special palettes
            // to isolate each layer
            if (merge_layers == 4) {
                img.palette.resize(16*4);
                for (int i=0; i<4; i++) {
                    int mask = 1 << (3-i);
                    for (int j=0; j<16; j++)
                        img.palette[i*16+j] = (j & mask) ? 0xFFFF : 0;
                }
            } else {
                img.palette.resize(16*2);
                for (int i=0; i<2; i++) {
                    for (int j=0; j<16; j++) {
                        int px = i==0 ? j>>2 : j&3;
                        switch (px) {
                        // IA16 palette with either I=FF or A=FF to identify fill vs outline
                        case 1: img.palette[i*16+j] = 0xFFFF; break;
                        case 2: img.palette[i*16+j] = 0x00FF; break;
                        }
                    }
                }
            }

            if (flag_verbose) {
                int num_glyphs = 0;
                for (int j=0; j<4 && i+j<atlases.size(); j++)
                    num_glyphs += sheets[i+j].rects.size();
                fprintf(stderr, "created atlas %d: %d x %d pixels (%d glyphs)\n", i/4, w, h, num_glyphs);
            }
            atlases2.push_back(std::move(img));
        }

        // Replace the atlases with the new ones
        atlases = std::move(atlases2);
    }

    // Add atlases to the font
    for (int i=0; i<atlases.size(); i++)
        add_atlas(atlases[i]);

    // Search for 0-sized glyphs. Those were not included in the atlases, so
    // we just need to set their advances correctly
    for (auto& g : glyphs) {
        if (g.img.w == 0 || g.img.h == 0) {
            glyph_t *gout = &fnt->glyphs[g.gidx];
            gout->xadvance = g.xadv;
        }
    }

    // Clear the glyph array, as we have added these to the atlases already
    glyphs.clear();
}

void Font::add_atlas(Image& img)
{
    static char *mksprite = NULL;
    if (!mksprite) asprintf(&mksprite, "%s/bin/mksprite", n64_inst);

    // Prepare mksprite command line
    struct subprocess_s subp;
    const char *cmd_addr[16] = {0}; int i = 0;
    cmd_addr[i++] = mksprite;
    cmd_addr[i++] = "--format";
    switch (img.fmt) {
        case FMT_CI8: cmd_addr[i++] = "CI4"; break;
        case FMT_IA16: cmd_addr[i++] = has_outline ? "IA8" : "I4"; break;
        default: assert(!"unsupported format");
    }
    cmd_addr[i++] = "--compress";  // don't compress the individual sprite (the font itself will be compressed)
    cmd_addr[i++] = "0";
    if (flag_verbose >= 2)
        cmd_addr[i++] = "--verbose";
    
    // Start mksprite
    if (subprocess_create(cmd_addr, subprocess_option_no_window|subprocess_option_inherit_environment, &subp) != 0) {
        fprintf(stderr, "Error: cannot run: %s\n", mksprite);
        exit(1);
    }

    // Create a PNG image from the atlas
    LodePNGState state;
    lodepng_state_init(&state);
    state.encoder.auto_convert = false; // avoid automatic remapping of palette colors
    LodePNGColorType ct;
    switch (img.fmt) {
        case FMT_I8: ct = LCT_GREY; break;
        case FMT_CI8: ct = LCT_PALETTE; break;
        case FMT_IA16: ct = LCT_GREY_ALPHA; break;
        case FMT_RGBA16: ct = LCT_RGBA; break;
        case FMT_RGBA32: ct = LCT_RGBA; break;
        default: assert(!"unsupported format");
    }
    state.info_raw = lodepng_color_mode_make(ct, 8);
    state.info_png.color = lodepng_color_mode_make(ct, 8);
    if (ct == LCT_PALETTE) {
        for (int i=0; i<img.palette.size(); i++) {
            int r = (img.palette[i] >> 11) & 0x1F;
            int g = (img.palette[i] >> 6) & 0x1F;
            int b = (img.palette[i] >> 1) & 0x1F;
            int a = (img.palette[i] & 1) * 0xFF;
            r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);
            lodepng_palette_add(&state.info_raw, r, g, b, a);
            lodepng_palette_add(&state.info_png.color, r, g, b, a);
        }
    }

    uint8_t *png = NULL; size_t png_size;
    unsigned error = lodepng_encode(&png, &png_size, img.pixels.data(), img.w, img.h, &state);
    if (error) {
        fprintf(stderr, "ERROR: generating PNG file %s\n", lodepng_error_text(error));
        exit(1);
    }

    // Write PNG to standard input of mksprite
    FILE *mksprite_in = subprocess_stdin(&subp);
    fwrite(png, 1, png_size, mksprite_in);
    fclose(mksprite_in); subp.stdin_file = SUBPROCESS_NULL;

    // Deallocate lodepng state
    lodepng_state_cleanup(&state);
    if (png) lodepng_free(png);

    // Read sprite from stdout into memory
    FILE *mksprite_out = subprocess_stdout(&subp);
    uint8_t *sprite = NULL;
    int sprite_size = 0;
    while (1) {
        uint8_t buf[4096];
        int n = fread(buf, 1, sizeof(buf), mksprite_out);
        if (n == 0) break;
        sprite = (uint8_t*)realloc(sprite, sprite_size + n);
        memcpy(sprite + sprite_size, buf, n);
        sprite_size += n;
    }

    // Dump mksprite's stderr. Whatever is printed there (if anything) is useful to see
    FILE *mksprite_err = subprocess_stderr(&subp);
    int buf_off = 0;
    while (1) {
        char buf[4096];
        int n = fread(buf+buf_off, 1, sizeof(buf)-buf_off, mksprite_err);
        if (n == 0) break;
        // Print whole lines to stderr, prefixed by "[mksprite]" and leave leftovers in the buffer
        n += buf_off;
        int x = 0;
        for (i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                fputs("[mksprite] ", stderr);
                fwrite(buf+x, 1, i+1-x, stderr);
                x = i+1;
            }
        }
        if (i < n) memmove(buf, buf+i, n-i);
        buf_off = n-i;
    }

    // mksprite should be finished. Extract the return code and abort if failed
    int retcode;
    subprocess_join(&subp, &retcode);
    if (retcode != 0) {
        fprintf(stderr, "Error: mksprite failed with return code %d\n", retcode);
        exit(1);
    }
    subprocess_destroy(&subp);

    fnt->atlases = (atlas_t*)realloc(fnt->atlases, (fnt->num_atlases + 1) * sizeof(atlas_t));
    fnt->atlases[fnt->num_atlases].sprite = (sprite_t*)(void*)sprite;
    fnt->atlases[fnt->num_atlases].size = sprite_size;
    fnt->atlases[fnt->num_atlases].up = NULL;
    fnt->num_atlases++;
}

void Font::add_kerning(int glyph1, int glyph2, int kerning) {
    kernings.push_back(Kerning{glyph1, glyph2, kerning});
}

void Font::make_kernings()
{
    assert(fnt->glyphs); // first we need the glyphs

    // Sort kernings by g1 and then g2
    std::sort(kernings.begin(), kernings.end(), [](const Kerning& k1, const Kerning& k2){
        if (k1.glyph1 != k2.glyph1) return k1.glyph1 < k2.glyph1;
        return k1.glyph2 < k2.glyph2;
    });

    // Allocate output data structure
    fnt->num_kerning = kernings.size()+1;
    fnt->kerning = (kerning_t*)malloc(fnt->num_kerning * sizeof(kerning_t));
    memset(&fnt->kerning[0], 0, sizeof(kerning_t));

    for (int i=0; i<kernings.size(); i++) {
        // Copy kerning data into output
        Kerning* ink = &kernings[i];
        assert(ink->kerning >= (int)-fnt->point_size && ink->kerning <= (int)fnt->point_size);
        fnt->kerning[i+1].glyph2 = ink->glyph2;
        fnt->kerning[i+1].kerning = ink->kerning * 127 / (int)fnt->point_size;

        // Update lo/hi indices for current glyph.
        if (i==0 || ink->glyph1 != ink[-1].glyph1)
            fnt->glyphs[ink->glyph1].kerning_lo = i+1;
        fnt->glyphs[ink->glyph1].kerning_hi = i+1;
    }

    kernings.clear();
}

void Font::add_ellipsis(int ellipsis_cp, int ellipsis_repeats)
{
    int ellipsis_glyph = get_glyph_index(ellipsis_cp);
    if (ellipsis_glyph < 0) {
        fprintf(stderr, "Error: ellipsis codepoint 0x%04x not found in font\n", ellipsis_cp);
        exit(1);
    }

    // Calculate length of ellipsis string
    glyph_t *g = &fnt->glyphs[ellipsis_glyph];
    float ellipsis_width = g->xadvance * (1.0f / 64.0f);
    
    if (g->kerning_lo) {
        for (int i = g->kerning_lo; i <= g->kerning_hi; i++) {
            if (fnt->kerning[i].glyph2 == ellipsis_glyph) {
                ellipsis_width += fnt->kerning[i].kerning * fnt->point_size / 127.0f;
                break;
            }
        }
    }

    fnt->ellipsis_advance = ellipsis_width + 0.5f;
    ellipsis_width *= 2;
    ellipsis_width += g->xoff2;
    
    fnt->ellipsis_width = ellipsis_width + 0.5f;
    fnt->ellipsis_reps = ellipsis_repeats;
    fnt->ellipsis_glyph = ellipsis_glyph;
}
