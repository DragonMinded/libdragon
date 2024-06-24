#include <map>
#include <memory>

// Bring in tex_format_t definition
#include "surface.h"

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
    std::string basedir;
    Font *font;
    std::vector<Image> pages;
    std::vector<bmchar> glyphs;
    // std::vector<n64font_kern> kernings;
    std::unordered_map<int, int> glyphmap; // ID -> index in glyphs

    bmctx() : font(NULL) {}
    ~bmctx() { if (font) { delete font; font = NULL; } }
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
        line += strcspn(line, "="); 
        if (!*line) return NULL; // ignore keys without value
        *line++ = '\0';
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
    if (!strcmp(key, "size")) gctx->font->fnt->point_size = atoi(value);
    return true;
}

bool bmfont_parse_common(char *key, char *value) {
    if (!key) return true;
    if (!strcmp(key, "base")) gctx->font->fnt->ascent = atoi(value);
    if (!strcmp(key, "lineHeight")) gctx->font->fnt->line_gap = atoi(value);
    if (!strcmp(key, "pages")) {
        gctx->pages.resize(atoi(value));
        return true;
    }
    return true;
}

bool bmfont_parse_page(char *key, char *value) {
    static Image *curpage = NULL;
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
        uint8_t *buf = NULL; unsigned width, height;
        int err = lodepng_decode32_file(&buf, &width, &height, pagefn);
        if (err) {
            fprintf(stderr, "error loading page %s: %s\n", pagefn, lodepng_error_text(err));
            free(pagefn);
            return false;
        }
        *curpage = std::move(Image(FMT_RGBA32, width, height, buf));
        free(buf);
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
        if (numkerns > 0) gctx->font->kernings.reserve(numkerns);
        return true;
    }
    return true;
}

bool bmfont_parse_kerning(char *key, char *value) {
    if (!key) {
        gctx->font->kernings.push_back({0,0,0});
        return true;
    }

    auto &k = gctx->font->kernings.back();
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
        gctx->font->add_range(r.second.first, r.second.second);
    }
}

bool repack_font(void)
{
    switch (gctx->font->bmp_outfmt) {
        case FMT_RGBA32: case FMT_RGBA16:
            for (auto &page : gctx->pages) {
                if (page.fmt != gctx->font->bmp_outfmt)
                    page = std::move(page.convert(gctx->font->bmp_outfmt));
            }
            break;
        default:
            // assert(!"unsupported output format");
            break;
    }
    return true;
}

void calc_glyphs(void)
{    
    for (auto &ch : gctx->glyphs) {
        if (ch.id == 32) {
            gctx->font->fnt->space_width = ch.width;
        }

        // gctx->glyphmap[ch.id] = gidx;
        Image *page = &gctx->pages[ch.page];

        gctx->font->add_glyph(ch.id, 
            page->crop(ch.x, ch.y, ch.width, ch.height),
            ch.xoffset, ch.yoffset - gctx->font->fnt->ascent,
            ch.xadvance*64);
    }

    if (!gctx->font->fnt->space_width)
        gctx->font->fnt->space_width = gctx->font->fnt->point_size;
}

void calc_atlases(void)
{
    gctx->font->make_atlases();
}

void calc_kernings(void)
{
    if (!flag_kerning) return;

    // Convert unicode IDs to glyph indices in the kernings array
    for (auto &k : gctx->font->kernings) {
        k.glyph1 = gctx->glyphmap[k.glyph1];
        k.glyph2 = gctx->glyphmap[k.glyph2];
    }

    gctx->font->make_kernings();
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
    gctx->font = new Font(outfn, FONT_TYPE_BITMAP, 0, 0, 0, 0, 0);
    gctx->font->bmp_outfmt = flag_bmfont_format;
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
        gctx->font->fnt->descent = 0;
        gctx->font->fnt->line_gap = gctx->font->fnt->line_gap - gctx->font->fnt->ascent + gctx->font->fnt->descent;

        // Map glyphs to the ranges
        calc_ranges();

        // Repack the font into new atlases that are N64 compatible
        repack_font();

        // Add glyphs to output font
        calc_glyphs();

        // Add the atlases
        calc_atlases();

        // Add the kernings to output
        calc_kernings();

        // Add ellipsis glyph
        if (flag_ellipsis_repeats > 0)
            gctx->font->add_ellipsis(flag_ellipsis_cp, flag_ellipsis_repeats);
    }


    // Write output file
    gctx->font->write();

    delete gctx;
    return ret;
}
