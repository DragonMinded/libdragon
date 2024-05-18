#include <map>
#include <memory>

#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS    // No need to parse PNG extra fields
#define LODEPNG_NO_COMPILE_CPP                 // No need to use C++ API
#include "../common/lodepng.h"
#include "../common/lodepng.c"

// Bring in tex_format_t definition
#include "surface.h"

std::vector<uint32_t> unicode_ranges{
    0x0000, 0x0020, 0x0080, 0x0100, 0x180, 0x250, 0x2b0, 0x300, 0x370, 0x400,
    0x500, 0x530, 0x590, 0x600, 0x700, 0x780, 0x900, 0x980, 0xa00, 0xa80,
    0xa00, 0xa80, 0xb00, 0xb80, 0xc00, 0xc80, 0xd00, 0xd80, 0xe00, 0xe80,
    0xf00, 0x1000, 0x10A0, 0x1100, 0x1200, 0x13A0, 0x1400, 0x1680, 0x16A0,
    0x1700, 0x1720, 0x1740, 0x1760, 0x1780, 0x1800, 0x1900, 0x1950, 0x19E0,
    0x1D00, 0x1E00, 0x1F00, 0x2000, 0x2070, 0x20A0, 0x20D0, 0x2100, 0x2150,
    0x2190, 0x2200, 0x2300, 0x2400, 0x2440, 0x2460, 0x2500, 0x2580, 0x25A0,
    0x2600, 0x2700, 0x27C0, 0x27F0, 0x2800, 0x2900, 0x2980, 0x2A00, 0x2B00,
    0x2E80, 0x2F00, 0x2FF0, 0x3000, 0x3040, 0x30A0, 0x3100, 0x3130, 0x3190,
    0x31A0, 0x31F0, 0x3200, 0x3300, 0x3400, 0x4DC0, 0x4E00, 0xA000, 0xA490,
    0xAC00, 0xD800, 0xDB80, 0xDC00, 0xE000, 0xF900, 0xFB00, 0xFB50, 0xFE00,
    0xFE20, 0xFE30, 0xFE50, 0xFE70, 0xFF00, 0xFFF0, 0x10000, 0x10080, 0x10100,
    0x10300, 0x10330, 0x10380, 0x10400, 0x10450, 0x10480, 0x10800, 0x1D000, 0x1D100,
    0x1D300, 0x1D400, 0x20000, 0x2F800, 0x2FA20
};

const char* tex_format_name(tex_format_t fmt) {
    switch ((int)fmt) {
    case FMT_NONE: return "AUTO";
    case FMT_RGBA32: return "RGBA32";
    case FMT_RGBA16: return "RGBA16";
    case FMT_CI8: return "CI8";
    case FMT_CI4: return "CI4";
    case FMT_I8: return "I8";
    case FMT_I4: return "I4";
    case FMT_IA16: return "IA16";
    case FMT_IA8: return "IA8";
    case FMT_IA4: return "IA4";
    default: assert(0); return ""; // should not happen
    }
}

tex_format_t tex_format_from_name(const char *name) {
    if (!strcasecmp(name, "RGBA32")) return FMT_RGBA32;
    if (!strcasecmp(name, "RGBA16")) return FMT_RGBA16;
    if (!strcasecmp(name, "IA16"))   return FMT_IA16;
    if (!strcasecmp(name, "CI8"))    return FMT_CI8;
    if (!strcasecmp(name, "I8"))     return FMT_I8;
    if (!strcasecmp(name, "IA8"))    return FMT_IA8;
    if (!strcasecmp(name, "CI4"))    return FMT_CI4;
    if (!strcasecmp(name, "I4"))     return FMT_I4;
    if (!strcasecmp(name, "IA4"))    return FMT_IA4;
    return FMT_NONE;
}

struct bmpage {
    std::unique_ptr<uint8_t[]> buf;       // pixels (RGBA, 32bpp)
    unsigned width;     // width in pixels
    unsigned height;    // height in pixels
};

struct bmchar {
    int id;
    int x, y;
    int width, height;
    int xoffset, yoffset;
    int xadvance;
    int page;
    int chnl;
    int unicode_range;   // Unicode range within which this character falls
};

struct bmctx {
    tex_format_t out_fmt;
    std::string basedir;
    rdpq_font_t *font;
    std::vector<bmpage> pages;
    std::vector<bmchar> glyphs;
    std::vector<n64font_kern> kernings;
    std::unordered_map<int, int> glyphmap; // ID -> index in glyphs

    bmctx() : out_fmt(FMT_NONE), font(NULL) {}
    ~bmctx() { if (font) n64font_free(font); }
};

bmctx *gctx;

const char* dirname(const char *full_path) {
    const char *sep = strrchr(full_path, '/');
    if (!sep) return ".";
    return strndup(full_path, sep - full_path);
}

char* unquote(char *str) {
    if (str[0] == '"') {
        str[strlen(str)-1] = '\0';
        return str+1;
    }
    return str;
}

// Parse a line from a BMFont file. Usage is similar to strok: pass the line
// only once, and then pass NULL. On the first call, it returns the first
// word which is the "command" (eg: "info", "page", "char", etc.). On the
// next calls, it will return alternatively keys and values as found on the line.
char* tokenize_bmfont_line(char *line_in)
{
    static bool last_key = false;
    static char *line = NULL;

    if (line_in) {
        last_key = false;
        line = line_in;
        char *cmd = line;
        line += strcspn(line, " \t\r\n"); *line++ = '\0';
        return cmd;
    }

    last_key = !last_key;
    if (last_key) {
        line += strspn(line, " \t\r\n");
        if (!*line) return NULL;
        char *key = line;
        line += strcspn(line, "="); *line++ = '\0';
        return key;
    } else {
        line += strspn(line, " \t\r\n");
        if (!*line) return NULL;
        char *value = line;
        if (*value == '\"') {
            value++; line++;
            line += strcspn(line, "\"");
        } else {
            line += strcspn(line, " \t\r\n");
        }
        *line++ = '\0';
        return value;
    }
}

bool bmfont_parse_info(char *key, char *value) {
    if (!key) return true;
    if (!strcmp(key, "size")) gctx->font->point_size = atoi(value);
    return true;
}

bool bmfont_parse_common(char *key, char *value) {
    if (!key) return true;
    if (!strcmp(key, "base")) gctx->font->ascent = atoi(value);
    if (!strcmp(key, "lineHeight")) gctx->font->line_gap = atoi(value);
    if (!strcmp(key, "pages")) {
        gctx->pages.resize(atoi(value));
        return true;
    }
    return true;
}

bool bmfont_parse_page(char *key, char *value) {
    static bmpage *curpage = NULL;
    if (!key) {
        curpage = NULL;
        return true;
    }
    if (!strcmp(key, "id")) {
        int pageid = atoi(value);
        if (pageid < 0 || pageid >= gctx->pages.size()) {
            fprintf(stderr, "invalid page id %d\n", pageid);
            return false;
        }
        curpage = &gctx->pages[pageid];
        return true;
    }    
    if (!strcmp(key, "file")) {
        char *pagefn = NULL;
        asprintf(&pagefn, "%s/%s", gctx->basedir.c_str(), unquote(value));
        uint8_t *buf = NULL;
        int err = lodepng_decode32_file(&buf, &curpage->width, &curpage->height, pagefn);
        if (err) {
            fprintf(stderr, "error loading page %s: %s\n", pagefn, lodepng_error_text(err));
            free(pagefn);
            return false;
        }
        curpage->buf.reset(buf);
        free(pagefn);
        return true;
    }
    return true;
}

bool bmfont_parse_chars(char *key, char *value) {
    if (!key) return true;
    if (!strcmp(key, "count")) {
        int numchars = atoi(value);
        gctx->glyphs.reserve(numchars);
        return true;
    }
    return true;
}

bool bmfont_parse_char(char *key, char *value) {
    if (!key) {
        bmchar ch;
        memset(&ch, 0, sizeof(ch));
        gctx->glyphs.push_back(ch);
        return true;
    }

    bmchar &ch = gctx->glyphs.back();
    if (!strcmp(key, "id")) ch.id = atoi(value);
    if (!strcmp(key, "x")) ch.x = atoi(value);
    if (!strcmp(key, "y")) ch.y = atoi(value);
    if (!strcmp(key, "width")) ch.width = atoi(value);
    if (!strcmp(key, "height")) ch.height = atoi(value);
    if (!strcmp(key, "xoffset")) ch.xoffset = atoi(value);
    if (!strcmp(key, "yoffset")) ch.yoffset = atoi(value);
    if (!strcmp(key, "xadvance")) ch.xadvance = atoi(value);
    if (!strcmp(key, "page")) ch.page = atoi(value);
    if (!strcmp(key, "chnl")) ch.chnl = atoi(value);
    return true;
}

bool bmfont_parse_kernings(char *key, char *value) {
    if (!key) return true;
    if (!strcmp(key, "count")) {
        int numkerns = atoi(value);
        if (numkerns > 0) gctx->kernings.reserve(numkerns);
        return true;
    }
    return true;
}

bool bmfont_parse_kerning(char *key, char *value) {
    if (!key) {
        n64font_kern k{0,0,0};
        gctx->kernings.push_back(k);
        return true;
    }

    n64font_kern &k = gctx->kernings.back();
    if (!strcmp(key, "first")) k.glyph1 = atoi(value);
    if (!strcmp(key, "second")) k.glyph2 = atoi(value);
    if (!strcmp(key, "amount")) k.kerning = atoi(value);
    return true;
}

bool bmfont_parse_line(FILE *f, const char *infn) {
    static int curline; 
    static char *line_buf = NULL; static size_t line_buf_size = 0;

    if (!f) {
        curline = 0;
        return true;
    }

    ++curline;
    if (getline(&line_buf, &line_buf_size, f) == -1) {
        return false;
    }
        
    bool (*parser)(char*, char*) = NULL;

    char *cmd = tokenize_bmfont_line(line_buf);
    if (!strcmp(cmd, "info"))       parser = bmfont_parse_info;
    if (!strcmp(cmd, "common"))     parser = bmfont_parse_common;
    if (!strcmp(cmd, "page"))       parser = bmfont_parse_page;
    if (!strcmp(cmd, "chars"))      parser = bmfont_parse_chars;
    if (!strcmp(cmd, "char"))       parser = bmfont_parse_char;
    if (!strcmp(cmd, "kernings"))   parser = bmfont_parse_kernings;
    if (!strcmp(cmd, "kerning"))    parser = bmfont_parse_kerning;
    if (!parser) {
        fprintf(stderr, "unknown command %s (%s:%d)\n", cmd, infn, curline+1);
        return true;
    }

    if (parser) {
        char *key = NULL; char *value = NULL;
        parser(NULL, NULL);  // initialization
        while (1) {
            key = tokenize_bmfont_line(NULL);
            if (!key) break;
            value = tokenize_bmfont_line(NULL);
            if (!value) {
                fprintf(stderr, "syntax error: no value found for key %s (%s:%d)\n", key, infn, curline+1);
                return false;
            }
            if (!parser(key, value)) {
                return false;
            }
        }
    }
    return true;
}

void calc_ranges(void)
{
    std::map<int, std::pair<int, int>> ranges;
    for (auto& ch : gctx->glyphs) {
        int range = *(std::upper_bound(unicode_ranges.begin(), unicode_ranges.end(), ch.id)-1);
        ch.unicode_range = range;

        auto r = ranges.find(range);
        if (r != ranges.end()) {
            r->second.first = MIN(r->second.first, ch.id);
            r->second.second = MAX(r->second.second, ch.id);
        } else {
            ranges.insert({range, {ch.id, ch.id}});
        }
    }

    for (auto& r : ranges) {
        n64font_addrange(gctx->font, r.second.first, r.second.second);
    }
}

void repack_font(void)
{
    const int border_padding = 1;

    rect_pack::Settings settings;
    memset(&settings, 0, sizeof(settings));
    settings.method = rect_pack::Method::Best;
    switch (gctx->out_fmt) {
    case FMT_I4: settings.max_width = 128; settings.max_height = 64; break;
    default:
        fprintf(stderr, "ERROR: unsupported output format: %s\n", tex_format_name(gctx->out_fmt));
        return;
    }
    settings.border_padding = 1;
    settings.allow_rotate = false;

    std::vector<rect_pack::Size> sizes;
    for (int i = 0; i < gctx->glyphs.size(); ++i) {
        bmchar &ch = gctx->glyphs[i];
        rect_pack::Size sz;
        sz.id = i;
        sz.width = ch.width + border_padding;
        sz.height = ch.height + border_padding;
        sizes.push_back(sz);
    }

    // Repack the glyphs
    std::vector<rect_pack::Sheet> sheets = rect_pack::pack(settings, sizes);

    // Create the new pages
    std::vector<bmpage> newpages;

    for (int i = 0; i < sheets.size(); ++i) {
        rect_pack::Sheet &sheet = sheets[i];
        uint8_t *newbuf = new uint8_t[sheet.width * sheet.height * 4];
        memset(newbuf, 0, sheet.width * sheet.height * 4);
        for (int j = 0; j < sheet.rects.size(); ++j) {
            rect_pack::Rect &rect = sheet.rects[j];
            bmchar &ch = gctx->glyphs[rect.id];
            bmpage &page = gctx->pages[ch.page];
            for (int y = 0; y < ch.height; ++y) {
                for (int x = 0; x < ch.width; ++x) {
                    int srcidx = (ch.y + y) * page.width * 4 + (ch.x + x) * 4;
                    int dstidx = (rect.y + y) * sheet.width * 4 + (rect.x + x) * 4;
                    memcpy(&newbuf[dstidx], &page.buf[srcidx], 4);
                }
            }
            ch.x = rect.x;
            ch.y = rect.y;
            ch.page = i;
        }

        newpages.push_back({
            .buf = std::unique_ptr<uint8_t[]>(newbuf),
            .width = sheet.width,
            .height = sheet.height
        });
    }

    // Free the old pages
    gctx->pages = std::move(newpages);
}

void calc_glyphs(void)
{
    for (auto &ch : gctx->glyphs) {
        int gidx = n64font_glyph(gctx->font, ch.id);
        glyph_t *g = &gctx->font->glyphs[gidx];       
        g->xadvance = ch.xadvance*64;
        g->xoff = ch.xoffset;
        g->yoff = ch.yoffset - gctx->font->ascent;
        g->xoff2 = g->xoff + ch.width;
        g->yoff2 = g->yoff + ch.height;
        g->s = ch.x;
        g->t = ch.y;
        g->natlas = ch.page;

        if (ch.id == 32) {
            gctx->font->space_width = ch.width;
        }

        gctx->glyphmap[ch.id] = gidx;
    }

    if (!gctx->font->space_width)
        gctx->font->space_width = gctx->font->point_size;
}

void calc_atlases(void)
{
    for (bmpage& page : gctx->pages) {
        switch (gctx->out_fmt) {
        case FMT_I4: case FMT_I8:
            // Extract only the alpha channel from the atlases
            for (int i = 0; i < page.width * page.height; ++i) {
                page.buf[i*4+0] = page.buf[i*4+3];
                page.buf[i*4+1] = page.buf[i*4+3];
                page.buf[i*4+2] = page.buf[i*4+3];
                page.buf[i*4+3] = 0xFF;
            }
            break;

        default:
            fprintf(stderr, "ERROR: unsupported output format: %s\n", tex_format_name(gctx->out_fmt));            
        }


        n64font_addatlas(gctx->font, page.buf.get(), page.width, page.height, page.width*4, 4);
    }
}

void calc_kernings(void)
{
    if (!flag_kerning) return;

    // Convert unicode IDs to glyph indices in the kernings array
    for (n64font_kern &k : gctx->kernings) {
        k.glyph1 = gctx->glyphmap[k.glyph1];
        k.glyph2 = gctx->glyphmap[k.glyph2];
    }

    n64font_addkerning(gctx->font, gctx->kernings);
}

int convert_bmfont(const char *infn, const char *outfn) 
{
    int ret = 0;

    FILE *f = fopen(infn, "rb");
    if (!f) {
        fprintf(stderr, "cannot open input file: %s\n", infn);
        return 1;
    }

    char magic[6] = {0};
    fread(magic, 1, 5, f);
    if (strcmp(magic, "info ") != 0) {
        fprintf(stderr, "invalid BMFont file: %s\n", infn);
        fprintf(stderr, "NOTE: only text format of FNT files is supported\n");
        return 1;
    }
    rewind(f);

    gctx = new bmctx;
    gctx->out_fmt = FMT_I4;
    gctx->font = n64font_alloc(0,0,0,0,0);
    gctx->basedir = dirname(infn);

    while (bmfont_parse_line(f, infn)) {}
    if (!feof(f)) {
        fprintf(stderr, "not done\n");
        ret = 1;
    }

    if (f) {
        fclose(f);
        f = NULL;
    }

    if (ret == 0) {
        gctx->font->descent = 0;
        gctx->font->line_gap = gctx->font->line_gap - gctx->font->ascent + gctx->font->descent;
        printf("ascender: %d, descender: %d, line gap: %d\n", gctx->font->ascent, gctx->font->descent, gctx->font->line_gap);

        // Map glyphs to the ranges
        calc_ranges();

        // Repack the font into new atlases that are N64 compatible
        repack_font();

        // Add glyphs to output font
        calc_glyphs();

        // Add the atlases
        calc_atlases();

        // Dump the pages in PNG format
        for (int i = 0; i < gctx->pages.size(); ++i) {
            bmpage &page = gctx->pages[i];
            char *pagefn = NULL;
            asprintf(&pagefn, "%s/%s_%d.png", gctx->basedir.c_str(), outfn, i);
            lodepng_encode32_file(pagefn, page.buf.get(), page.width, page.height);
            printf("page %d: %s\n", i, pagefn);
            free(pagefn);
        }

        // Add the kernings to output
        calc_kernings();
    }

    // Write output file
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "cannot open output file: %s\n", outfn);
        return 1;
    }
    n64font_write(gctx->font, out);
    fclose(out);

    delete gctx;
    return ret;
}
