
// Bring in tex_format_t definition
#include "surface.h"
#include "sprite.h"
#include <map>
#include <mutex>
#include "phf.h"
#include "phf.cpp"
#include "../common/thread_utils.h"

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

static uint32_t utf8_to_codepoint(const char *utf8, const char **endptr) {
    const uint8_t *s = (const uint8_t*)utf8;
    uint32_t c = *s++;
    if (c < 0x80) {
        *endptr = (const char*)s;
        return c;
    }
    if (c < 0xC0) {
        *endptr = (const char*)s;
        return 0xFFFD;
    }
    if (c < 0xE0) {
        c = ((c & 0x1F) << 6) | (*s++ & 0x3F);
        *endptr = (const char*)s;
        return c;
    }
    if (c < 0xF0) {
        c = ((c & 0x0F) << 12); c |= ((*s++ & 0x3F) << 6); c |= (*s++ & 0x3F);
        *endptr = (const char*)s;
        return c;
    }
    if (c < 0xF8) {
        c = ((c & 0x07) << 18); c |= ((*s++ & 0x3F) << 12); c |= ((*s++ & 0x3F) << 6); c |= (*s++ & 0x3F);
        *endptr = (const char*)s;
        return c;
    }
    *endptr = (const char*)s;
    return 0xFFFD;
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
        assert(w >= 0 && h >= 0);
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
                a = a > 0x7F;
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
            assert(x >= 0 && x < w);
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
        assert(y >= 0 && y < h);
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
        assert(w >= 0 && h >= 0);
        if (w == 0 && h == 0)
            return Image(fmt, 0, 0);
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

        bool solid = false;
        int x0 = w, y0 = h, x1 = 0, y1 = 0;
        for (int y = 0; y < h; y++) {
            Line line = (*this)[y];
            for (int x = 0; x < w; x++) {
                if (!line[x].is_transparent()) {
                    x0 = std::min(x0, x);
                    y0 = std::min(y0, y);
                    x1 = std::max(x1, x);
                    y1 = std::max(y1, y);
                    solid = true;
                }
            }
        }
        if (!solid) {
            *ox0 = *oy0 = 0;
            return Image(fmt, 0, 0);
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
    fonttype_t fonttype;
    tex_format_t bmp_outfmt = FMT_RGBA16;  // output format in case fonttype is FONT_TYPE_BITMAP
    phf phash;  // perfect hash table for sparse ranges
    std::vector<int16_t> phash_values;

    Font(std::string fn, fonttype_t ftype, int point_size, int ascent, int descent, int line_gap, int space_width)
    {
        outfn = fn;
        fonttype = ftype;
        fnt = (rdpq_font_t*)calloc(1, sizeof(rdpq_font_t));
        memcpy(fnt->magic, FONT_MAGIC, 3);
        fnt->version = 10;
        fnt->flags = fonttype;
        fnt->point_size = point_size;
        fnt->ascent = ascent;
        fnt->descent = descent;
        fnt->line_gap = line_gap;
        fnt->space_width = space_width;
        memset(&phash, 0, sizeof(phf));
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

    bool is_bitmap(void) {
        return fonttype == FONT_TYPE_BITMAP;
    }

    bool is_monochrome(void) {
        return fonttype == FONT_TYPE_MONO || fonttype == FONT_TYPE_MONO_OUTLINE;
    }
    bool has_outline(void) {
        return fonttype == FONT_TYPE_ALIASED_OUTLINE || fonttype == FONT_TYPE_MONO_OUTLINE;
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
    void make_sparse_ranges(void);

    void write(void);

private:
    std::vector<rect_pack::Sheet> pack_atlases(std::vector<Glyph>& glyphs, int merge_layers);
    void build_perfect_hash(std::vector<uint32_t>& keys, std::vector<int16_t>& values);
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

    int off_placeholders = ftell(out);
    w32(out, (uint32_t)0); // placeholder
    w32(out, (uint32_t)0); // placeholder
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

    // Write sparse table
    uint32_t offset_sparse = 0;
    if (!phash_values.empty()) {
        walign(out, 4);

        int offset_disp = ftell(out);
        for (int i=0; i<phash.r; i++)
            w16(out, phash.g[i]); // already verified it fits 16 bits in build_perfect_hash

        int offset_T = ftell(out);
        for (int i=0; i<phash_values.size(); i++)
            w16(out, phash_values[i]);

        walign(out, 4);
        offset_sparse = ftell(out);
        w32(out, phash.seed);
        w32(out, phash.r);
        w32(out, phash.m);
        w32(out, offset_disp);
        w32(out, offset_T);
    }

    // Write glyphs, aligned to 16 bytes. This makes sure
    // they cover exactly one data cacheline in R4300, so that
    // they each drawn glyph dirties exactly one line.
    walign(out, 16);
    uint32_t offset_glypes = ftell(out);
    for (int i=0; i<fnt->num_glyphs; i++)
    {
        w8(out, fnt->glyphs[i].xadvance);
        w8(out, fnt->glyphs[i].xoff);
        w8(out, fnt->glyphs[i].yoff);
        w8(out, fnt->glyphs[i].xoff2);
        w8(out, fnt->glyphs[i].yoff2);
        w8(out, fnt->glyphs[i].s);
        w8(out, fnt->glyphs[i].t);
        w8(out, (fnt->glyphs[i].natlas << 2) | (fnt->glyphs[i].ntile));
    }

    uint32_t offset_glyphs_kranges = 0;
    if (fnt->num_kerning > 1)
    {
        // Write glyph kerning ranges
        walign(out, 16);
        offset_glyphs_kranges = ftell(out);
        for (int i=0; i<fnt->num_glyphs; i++)
        {
            w16(out, fnt->glyphs_kranges[i].kerning_lo);
            w16(out, fnt->glyphs_kranges[i].kerning_hi);
        }
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
    w32(out, offset_sparse);
    w32(out, offset_glypes);
    w32(out, offset_glyphs_kranges);
    w32(out, offset_atlases);
    w32(out, offset_kernings);
    w32(out, offset_builtin_style);

    fseek(out, offset_end, SEEK_SET);

    fclose(out);
}

void Font::add_range(int first, int last)
{
    int range_size = last - first + 1;
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
    fnt->ranges[fnt->num_ranges].num_codepoints = range_size;
    fnt->ranges[fnt->num_ranges].first_glyph = fnt->num_glyphs;
    fnt->num_ranges++;
    fnt->glyphs =                (glyph_t*)realloc(fnt->glyphs,         (fnt->num_glyphs + range_size) * sizeof(glyph_t));
    fnt->glyphs_kranges = (glyph_krange_t*)realloc(fnt->glyphs_kranges, (fnt->num_glyphs + range_size) * sizeof(glyph_krange_t));
    memset(fnt->glyphs         + fnt->num_glyphs, 0, range_size * sizeof(glyph_t));
    memset(fnt->glyphs_kranges + fnt->num_glyphs, 0, range_size * sizeof(glyph_krange_t));
    fnt->num_glyphs += range_size;
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

    switch (fonttype) {
    case FONT_TYPE_MONO:
        if (img.fmt != FMT_I8) assert(!"glyph image must be I8 for monochrome fonts");
        // Now check that all the pixels are monochrome
        img.for_each_pixel([&](Image::Pixel&& px) {
            if (px.data[0] > 0 && px.data[0] < 0xF0)
                assert(!"monochrome glyph must not contains shades of gray");
        });
        break;
    case FONT_TYPE_ALIASED:
        if (img.fmt != FMT_I8) assert(!"glyph image must be I8 for aliased fonts");
        break;
    case FONT_TYPE_MONO_OUTLINE:
        // Outline fonts are IA16. Intensity goes between 0x00 for the outline
        // to 0xFF for the fill, while the alpha channel is the coverage of each pixel.
        // Outline monochromatic fonts have intensity fixed to 0xFF.
        if (img.fmt != FMT_IA16) assert(!"glyph image must be IA16 for outlined fonts");
        // Now check that all the pixels are monochrome
        img.for_each_pixel([&](Image::Pixel&& px) {
            if (px.data[0] != 0 && px.data[1] != 0x00 && px.data[1] != 0xFF)
                assert(!"monochrome glyph must not contains shades of gray");
        });
        break;
    case FONT_TYPE_ALIASED_OUTLINE:
        // Outline fonts are IA16. Intensity goes between 0x00 for the outline
        // to 0xFF for the fill, while the alpha channel is the coverage of each pixel.
        if (img.fmt != FMT_IA16) assert(!"glyph image must be IA16 for outlined fonts");
        break;
    case FONT_TYPE_BITMAP:
        if (img.fmt != FMT_RGBA16 && img.fmt != FMT_RGBA32 && img.fmt != FMT_CI8)
            assert(!"glyph image must be RGBA16/RGBA32/CI8 for bitmap fonts");
        break;
    default:
        assert(!"unsupported font type");
    }

    // Crop the image to the actual glyph size
    int x0=0, y0=0;
    img = img.crop_transparent(&x0, &y0);

    glyphs.push_back(Glyph(gidx, cp, std::move(img), xoff + x0, yoff + y0, xadv));
    return gidx;
}

// Pack the specified glyphs into optimized texture atlases
std::vector<rect_pack::Sheet> Font::pack_atlases(std::vector<Glyph>& glyphs, int merge_layers)
{
    // Pack the glyphs into a texture
    rect_pack::Settings settings;
    memset(&settings, 0, sizeof(settings));
    settings.method = rect_pack::Method::Best;
    settings.method = rect_pack::Method::Best;
    settings.max_width = 128;
    settings.max_height = 64;
    settings.border_padding = 0;
    settings.allow_rotate = false;
    
    int total_glyph_area = 0;
    std::vector<rect_pack::Size> sizes;
    std::vector<rect_pack::Sheet> sheets;
    for (int i=0; i<glyphs.size(); i++) {
        if (glyphs[i].img.w == 0 || glyphs[i].img.h == 0) continue;
        rect_pack::Size size;
        size.id = i;
        size.width = glyphs[i].img.w + settings.border_padding;
        size.height = glyphs[i].img.h + settings.border_padding;
        total_glyph_area += size.width * size.height;
        sizes.push_back(size);
    }

    tex_format_t cfmt; 
    switch (fonttype) {
    case FONT_TYPE_BITMAP:
        cfmt = bmp_outfmt;
        switch (bmp_outfmt) {
        case FMT_RGBA16:
            settings.max_width = 64;
            settings.max_height = 32;
            settings.align_width = TEX_FORMAT_BYTES2PIX(cfmt, 8);
            break;
        case FMT_RGBA32:
            settings.max_width = 32;
            settings.max_height = 32;
            // RGBA32 is 4bpp but it's split into two 16-bit planes in TMEM.
            // So the width alignment applies for each 16-bit plane.
            settings.align_width = TEX_FORMAT_BYTES2PIX(FMT_RGBA16, 8);
            break;
        case FMT_CI4:
            settings.max_width = 64;
            settings.max_height = 64;
            settings.align_width = TEX_FORMAT_BYTES2PIX(cfmt, 8);
            break;
        case FMT_CI8:
            settings.max_width = 64;
            settings.max_height = 32;
            settings.align_width = TEX_FORMAT_BYTES2PIX(cfmt, 8);
            break;
        default:
            assert(!"unsupported bitmap format");
        }
        break;
    case FONT_TYPE_ALIASED:
        // Aliased font: pack into I4 (max 128x64).
        settings.max_width = 128;
        settings.max_height = 64;
        cfmt = FMT_I4;
        settings.align_width = TEX_FORMAT_BYTES2PIX(cfmt, 8);
        break;
    case FONT_TYPE_ALIASED_OUTLINE:
        // Aliased+outlined font: pack into IA8 (max 64x64).
        settings.max_width = 64;
        settings.max_height = 64;
        cfmt = FMT_IA8;
        settings.align_width = TEX_FORMAT_BYTES2PIX(cfmt, 8);
        break;
    case FONT_TYPE_MONO:
    case FONT_TYPE_MONO_OUTLINE:
        settings.max_width = 64;
        settings.max_height = 64;
        cfmt = FMT_CI4;
        settings.align_width = TEX_FORMAT_BYTES2PIX(cfmt, 8);
        break;
    default:
        assert(!"unsupported font type");
    }

    enum { MAX_TMEM_ATLASES = 8 };
    bool fit_tmem = false;

    // Try packing for TMEM sizes. We first do a quick check whether the glyphs
    // do fit in the maximum number of atlases for TMEM (with zero waste), just to avoid
    // doing a huge computation in case there are many glyph, just to discard it.
    if (total_glyph_area <= merge_layers * MAX_TMEM_ATLASES * settings.max_width * settings.max_height) {

        // Do the packing with TMEM limits
        sheets = rect_pack::pack(settings, sizes);
        
        // Check whether the number of atlases is below the threshold to keep them in TMEM
        fit_tmem = sheets.size() <= MAX_TMEM_ATLASES * merge_layers;
    }

    if (!fit_tmem) {    
        // If we end up with too many atlases for a single range, it means that
        // loading atlases into TMEM isn't going to be efficient: we would statistically
        // need to switch between atlases too often. In this case, change strategy
        // and build huge atlases that will be kept in RDRAM and loaded one glyph
        // at a time instead
        // 8 is an arbitrary threshold for the heuristics of "too many atlases".
        if (flag_verbose) fprintf(stderr, "too many atlases for a single range, switching to RDRAM atlases\n");
        // We are limited to 256x256 because we keep s/t coordinates in 8-bit values
        settings.max_width = 256;
        settings.max_height = 256;
        // RDRAM atlases are not limited in width, they could have any size. We still
        // put 4 here to avoid a too slow optimization process for a modest saving.
        settings.align_width = 4;
        sheets = rect_pack::pack(settings, sizes);
    }

    if (flag_verbose) fprintf(stderr, "packed %zu glyphs into %zu sheets\n", sizes.size(), sheets.size());

    // We can save bytes on the last group of sheets by checking for many different
    // sizes. This is not something at which rect_pack excels at, so a bruteforce
    // approach is used here.
    int num_sheets = sheets.size();
    int last_group = (num_sheets-1) / merge_layers * merge_layers;

    // Move the last group of sheets to a temporary array. Calculate also the
    // current area for the last group (which is the area of the biggest
    // sheet in the group).
    int group_width = 0, group_height = 0;
    std::vector<rect_pack::Sheet> group_sheets;
    for (int i=last_group; i<num_sheets; i++) {
        auto& s = sheets.back();
        int area = s.width * s.height;
        if (area > group_width*group_height) {
            group_width = s.width;
            group_height = s.height;
        }
        group_sheets.push_back(s);
        sheets.pop_back();
    }

    // Try to optimize the last group (up to four sheets). Create an array
    // of input sizes for all the glyphs in the last group
    int min_area = 0;
    std::vector<rect_pack::Size> sizes2;
    for (auto& sheet : group_sheets) {
        for (auto &r : sheet.rects) {
            auto &g = glyphs[r.id];
            rect_pack::Size size;
            size.id = r.id;
            size.width = g.img.w + settings.border_padding;
            size.height = g.img.h + settings.border_padding;
            sizes2.push_back(size);
            min_area += size.width * size.height;
        }
    }
    min_area /= merge_layers;

    if (flag_verbose >= 1)
        fprintf(stderr, "repacking last %zu sheets: %d x %d (%d bytes)\n", group_sheets.size(), group_width, group_height, TEX_FORMAT_PIX2BYTES(cfmt, group_width * group_height));

    // Try to find a better packing for this sheet group. Set the maximum number
    // of sheets to the expected one so that we can early abort.
    std::atomic_int best_area = group_width * group_height;
    int best_h = 1024;
    settings.max_sheets = merge_layers;
    std::mutex best_lock;

    thParaLoop(256, [&](int h){
        if (++h < MAX(min_area/256, 8)) return;
        int w = MAX(ROUND_UP(min_area / h, settings.align_width), settings.align_width);
        for (; w <= 256; w += settings.align_width) {
            // Early exit if w is too big to win against best_area.
            // If h*w matches, to guarantee consistency of output, we want to keep the result
            // with the smallest h.
            if (h * w > best_area) break;

            // printf("    trying %dx%d\n", w, h);
            rect_pack::Settings s = settings;
            s.min_width = w;
            s.min_height = h;
            s.max_width = w;
            s.max_height = h;
            std::vector<rect_pack::Sheet> new_sheets = rect_pack::pack(s, sizes2);

            // Check if all glyphs fit this size, by counting how many of them were packed
            int packed_glyphs = 0;
            for (auto &sheet : new_sheets) packed_glyphs += sheet.rects.size();
            if (packed_glyphs == sizes2.size()) {
                std::lock_guard<std::mutex> g(best_lock);
                if (h * w < best_area || (h * w == best_area && h < best_h)) {
                    group_sheets = new_sheets;
                    best_h = h;
                    best_area = w*h;
                    if (flag_verbose >= 1)
                        printf("    found better packing: %d x %d (%d bytes)\n", w, h, TEX_FORMAT_PIX2BYTES(cfmt, w*h));
                }
                break;
            }
        }
    });

    // Append the best sheets to the calculated sheets
    sheets.insert(sheets.end(), group_sheets.begin(), group_sheets.end());

    return sheets;
}


void Font::make_atlases(void)
{
    if (glyphs.empty()) {
        if (flag_verbose) fprintf(stderr, "WARNING: no glyphs found in this range\n");
        return;
    }

    bool all_spaces = true;
    for (auto& g : glyphs) {
        // Zero-sized glyphs ("spaces") will not be included in the atlases
        // but we want to set their advance anyway in the output table.
        if (g.img.w == 0 && g.img.h == 0) {
            glyph_t *gout = &fnt->glyphs[g.gidx];
            gout->xadvance = g.xadv;
        } else {
            all_spaces = false;
        }
    }
    if (all_spaces) {
        // No glyphs to pack
        glyphs.clear();
        return;
    }

    if (num_atlases == 0) {
        switch (fonttype) {
        case FONT_TYPE_MONO:
            if (flag_verbose) fprintf(stderr, "monochrome glyphs detected (format: 1bpp)\n");
            break;
        case FONT_TYPE_MONO_OUTLINE:
            if (flag_verbose) fprintf(stderr, "monochrome+outlined glyphs detected (format: 2bpp)\n");
            break;
        case FONT_TYPE_ALIASED:
            if (flag_verbose) fprintf(stderr, "aliased glyphs detected (format: 4 bpp)\n");
            break;
        case FONT_TYPE_ALIASED_OUTLINE:
            if (flag_verbose) fprintf(stderr, "aliased+outlined glyphs detected (format: 8 bpp)\n");
            break;
        case FONT_TYPE_BITMAP:
            if (flag_verbose) fprintf(stderr, "bitmap glyphs detected (format: %s)\n", tex_format_name(bmp_outfmt));
            break;
        default:
            assert(!"unsupported font type");
            break;
        }
    }

    // Determine how many different layers the final atlases will be:
    //  Aliased font: single layer (either I4 or IA8, depending on outline)
    //  Mono, no outline: we can use 1bpp, so we can merge 4 layers
    //  Mono, outline: we can use 2bpp, so we can merge 2 layers
    int merge_layers = !is_monochrome() ? 1 : (has_outline() ? 2 : 4);

    // Pack the atlases
    auto sheets = pack_atlases(glyphs, merge_layers);

    // Create the actual textures
    std::vector<Image> atlases;
    for (int i=0; i<sheets.size(); i++) {
        rect_pack::Sheet& sheet = sheets[i];

        Image img(is_bitmap() ? FMT_RGBA16 : FMT_IA16, sheet.width, sheet.height);

        for (int j=0; j<sheet.rects.size(); j++) {
            rect_pack::Rect& rect = sheet.rects[j];
            Glyph& glyph = glyphs[rect.id];

            if (!rect.rotated) {
                img.copy(glyph.img, rect.x, rect.y);
            } else {
                img.copy_rotated(glyph.img, rect.x, rect.y);
            }

            glyph_t *gout = &fnt->glyphs[glyph.gidx];
            
            int natlas = i;
            if (merge_layers > 1) {
                assert(merge_layers <= 4); // 2 bits for glyph_t::ntile
                gout->ntile = i & (merge_layers-1);
                natlas /= merge_layers;
            }
            natlas += fnt->num_atlases; // offset by the atlases already added for other ranges
            assert(natlas < 64); // 6 bits for glyph_t::natlas
            gout->natlas = natlas;
            assert(rect.x < 256 && rect.y < 256);
            gout->s = rect.x; gout->t = rect.y;
            gout->xoff = glyph.xoff;
            gout->yoff = glyph.yoff;
            gout->xoff2 = gout->xoff + glyph.img.w;
            gout->yoff2 = gout->yoff + glyph.img.h;
            gout->xadvance = glyph.xadv;

            if (flag_verbose >= 2) {
                fprintf(stderr, "  glyph %s [U+%04X]: %d x %d, %d,%d %d,%d %.2f\n", 
                    codepoint_to_utf8(glyph.codepoint).c_str(), glyph.codepoint, 
                    glyph.img.w, glyph.img.h, gout->xoff, gout->yoff, gout->xoff2, gout->yoff2, glyph.xadv/64.f);
            }

            if(abs(gout->xoff) > 128 || abs(gout->yoff) > 128 || abs(gout->xoff2) > 128 || abs(gout->yoff2) > 128 ||
                gout->xadvance < 0 || gout->xadvance > 255)
            {
                fprintf(stderr, "ERROR: font too big, please reduce point size (%d)\n", fnt->point_size);
                exit(1);
            }
        }

        if (flag_verbose && merge_layers == 1)
            fprintf(stderr, "created atlas %d: %d x %d pixels (%zu glyphs)\n", i + fnt->num_atlases, sheet.width, sheet.height, sheet.rects.size());
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

    if (merge_layers > 1) {
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
                for (int j=0; j<merge_layers && i+j<atlases.size(); j++)
                    num_glyphs += sheets[i+j].rects.size();
                fprintf(stderr, "created atlas %d: %d x %d pixels (%d glyphs)\n", i/merge_layers + fnt->num_atlases, w, h, num_glyphs);
            }
            atlases2.push_back(std::move(img));
        }

        // Replace the atlases with the new ones
        atlases = std::move(atlases2);
    }

    // Add atlases to the font
    for (int i=0; i<atlases.size(); i++)
        add_atlas(atlases[i]);

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
    switch (fonttype) {
    case FONT_TYPE_MONO:
    case FONT_TYPE_MONO_OUTLINE:
        // We received a merged-layers CI8 image but values are 0..15 specifically
        // so that we can pack it as CI4. Let mksprite do it.
        assert(img.fmt == FMT_CI8); 
        cmd_addr[i++] = "--format"; cmd_addr[i++] = "CI4";
        break;
    case FONT_TYPE_ALIASED:
        assert(img.fmt == FMT_IA16); 
        cmd_addr[i++] = "--format"; cmd_addr[i++] = "I4";
        break;
    case FONT_TYPE_ALIASED_OUTLINE:
        assert(img.fmt == FMT_IA16); 
        cmd_addr[i++] = "--format"; cmd_addr[i++] = "IA8";
        break;
    case FONT_TYPE_BITMAP:
        // For bitmap fonts, keep the input format
        cmd_addr[i++] = "--format"; cmd_addr[i++] = tex_format_name(bmp_outfmt);
        break;
    default:
        assert(!"unsupported font type");
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
    uint8_t *png = NULL; size_t png_size; unsigned error;
    if (img.fmt == FMT_RGBA16) {
        Image img2 = img.convert(FMT_RGBA32);
        error = lodepng_encode(&png, &png_size, img2.pixels.data(), img.w, img.h, &state);
    } else {
        error = lodepng_encode(&png, &png_size, img.pixels.data(), img.w, img.h, &state);
    }
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
            fnt->glyphs_kranges[ink->glyph1].kerning_lo = i+1;
        fnt->glyphs_kranges[ink->glyph1].kerning_hi = i+1;
    }

    if (flag_verbose)
        fprintf(stderr, "added %zu kernings\n", kernings.size());

    kernings.clear();
}

// Calculate the smallest prime number bigger than x
static int calc_smallest_prime(int x) 
{
    for (int i=x; ; i++) {
        bool prime = true;
        for (int j=2; j*j<=i; j++) {
            if (i % j == 0) {
                prime = false;
                break;
            }
        }
        if (prime) return i;
    }
    return 0;
}

void Font::build_perfect_hash(std::vector<uint32_t>& keys, std::vector<int16_t>& values)
{
    assert(phash.r == 0); // only one perfect hash

    const int lambda = 4;
    const int alpha = 100; // load factor (100%)
    const uint32_t seed = 0x12345678;

    PHF::init<uint32_t, false>(&phash, &keys[0], keys.size(), lambda, alpha, seed);

    phash_values.resize(phash.m, -1);
    for (int i=0; i<keys.size(); i++) {
        uint32_t h = PHF::hash(&phash, keys[i]);
        assert(h < phash_values.size());
        assert(phash_values[h] == -1);      // check if it's a real perfect hash
        phash_values[h] = values[i];
    }

    // In font64 we will store the displacement table using 16 bit indices.
    // This seems to be more than enough even for huge fonts. In case this is
    // hit, probably the best solution is to decrease the load factor a bit,
    // which reduces d_max (eg: in a loop until d_max fits 16 bits again).
    assert(phash.d_max < 65536);

    if (flag_verbose) fprintf(stderr, "    perfect hash table: %zu glyphs, %u bytes (d_max:%zu)\n",
        keys.size(), (int)(phash.m * sizeof(int16_t) + phash.r * sizeof(uint16_t)), phash.d_max);
}

void Font::make_sparse_ranges(void)
{
    // By default, all ranges are dense. Check if we find ranges that waste more
    // than WASTED_MEMORY_THRESHOLD by being dense, and convert them to sparse.
    enum { WASTED_MEMORY_THRESHOLD = 1*1024 };

    std::vector<uint32_t> sparse_codepoints;
    std::vector<int16_t> sparse_indices;
    
    int widx = 0;
    for (int i=0; i<fnt->num_ranges; i++)
    {
        // Small ranges are never worth converting to sparse
        bool sparse = false;
        int num_codepoints = fnt->ranges[i].num_codepoints;
        if (num_codepoints * sizeof(glyph_t) >= WASTED_MEMORY_THRESHOLD) {
            // Check how many empty glyph slots are in this range
            int num_glyphs = 0;
            for (int j=0; j<num_codepoints; j++)
            {
                int gidx = fnt->ranges[i].first_glyph + j;
                if (fnt->glyphs[gidx].xadvance > 0)
                    num_glyphs++;
            }
            if ((num_codepoints - num_glyphs) * sizeof(glyph_t) >= WASTED_MEMORY_THRESHOLD) {
                sparse = true;
                if (flag_verbose) fprintf(stderr, "range %x-%x is sparse (%d glyphs out of %d)\n", 
                    fnt->ranges[i].first_codepoint, fnt->ranges[i].first_codepoint + num_codepoints - 1, 
                    num_glyphs, num_codepoints);
            }
        }

        int first_glyph = fnt->ranges[i].first_glyph;
        fnt->ranges[i].first_glyph = sparse ? -1 : widx;

        // Compact the range, removing all empty glyphs if sparse
        for (int j=0; j<num_codepoints; j++)
        {
            int ridx = first_glyph + j;
            if (fnt->glyphs[ridx].xadvance != 0 || !sparse) {
                if (sparse) {
                    sparse_codepoints.push_back(fnt->ranges[i].first_codepoint + j);
                    sparse_indices.push_back(widx);
                }
                fnt->glyphs[widx++] = fnt->glyphs[ridx];
            }
        }

    }

    if (sparse_codepoints.empty()) {
        assert(widx == fnt->num_glyphs);
        return;
    }

    // Some glyphs have been compacted. Update the number of glyphs in the font
    fnt->num_glyphs = widx;

    build_perfect_hash(sparse_codepoints, sparse_indices);
    
    if (flag_verbose) fprintf(stderr, "total glyphs: %d\n", fnt->num_glyphs);
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
    glyph_krange_t *gkr = &fnt->glyphs_kranges[ellipsis_glyph];
    float ellipsis_advance = g->xadvance;
    
    // Correct for kerning when repeating the ellipsis twice
    if (gkr->kerning_lo) {
        for (int i = gkr->kerning_lo; i <= gkr->kerning_hi; i++) {
            if (fnt->kerning[i].glyph2 == ellipsis_glyph) {
                ellipsis_advance += fnt->kerning[i].kerning * fnt->point_size / 127.0f;
                break;
            }
        }
    }

    fnt->ellipsis_advance = ellipsis_advance;
    fnt->ellipsis_width = ellipsis_advance * (ellipsis_repeats-1) + g->xoff2;
    fnt->ellipsis_reps = ellipsis_repeats;
    fnt->ellipsis_glyph = ellipsis_glyph;
}
