
typedef struct {
    uint8_t *buf;
    unsigned width;
    unsigned height;
} bmpage;

struct {
    char *basedir;
    rdpq_font_t *font;
    bmpage *pages;
    int numpages;
} bmctx;

char* dirname(const char *full_path) {
    char *sep = strrchr(full_path, '/');
    if (!sep) return ".";
    return strndup(full_path, sep - full_path + 1);
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
char* tokenize_bmfont_line(char *line)
{
    static bool last_key = false;
    if (line) {
        last_key = false;
        return strtok(line, " \t\r\n");
    }
    last_key = !last_key;
    return strtok(NULL, last_key ? "=" : " \t\r\n");
}

bool bmfont_parse_info(char *key, char *value) {
    if (!key) return true;
    if (!strcmp(key, "size")) bmctx.font->point_size = atoi(value);
    return true;
}

bool bmfont_parse_common(char *key, char *value) {
    if (!key) return true;
    if (!strcmp(key, "lineHeight")) return true;
    if (!strcmp(key, "pages")) {
        bmctx.numpages = atoi(value);
        bmctx.pages = calloc(bmctx.numpages, sizeof(bmpage));
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
        if (pageid < 0 || pageid >= bmctx.numpages) {
            fprintf(stderr, "invalid page id %d\n", pageid);
            return false;
        }
        curpage = &bmctx.pages[pageid];
        return true;
    }    
    if (!strcmp(key, "file")) {
        char *pagefn = NULL;
        asprintf(&pagefn, "%s/%s", bmctx.basedir, unquote(value));
        int err = lodepng_decode32_file(&curpage->buf, &curpage->width, &curpage->height, pagefn);
        if (err) {
            fprintf(stderr, "error loading page %s: %s\n", pagefn, lodepng_error_text(err));
            free(pagefn);
            return false;
        }
        free(pagefn);
        return true;
    }
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
    // if (!strcmp(cmd, "chars"))      parser = bmfont_parse_chars;
    // if (!strcmp(cmd, "char"))       parser = bmfont_parse_char;
    // if (!strcmp(cmd, "kernings"))   parser = bmfont_parse_kernings;
    // if (!strcmp(cmd, "kerning"))    parser = bmfont_parse_kerning;

    if (parser) {
        char *key = NULL; char *value = NULL;
        parser(NULL, NULL);  // initialization
        while (1) {
            key = tokenize_bmfont_line(NULL);
            if (!key) break;
            value = tokenize_bmfont_line(NULL);
            if (!value) {
                fprintf(stderr, "syntax error: no value found for key %s (%s:%d)\n", key, infn, curline);
                return false;
            }
            if (!parser(key, value)) {
                return false;
            }
        }
    }
    return true;
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

    memset(&bmctx, 0, sizeof(bmctx));
    bmctx.font = n64font_alloc(0,0,0,0,0);
    bmctx.basedir = dirname(infn);

    while (bmfont_parse_line(f, infn)) {}
    if (!feof(f)) {
        fprintf(stderr, "not done\n");
        ret = 1;
    }

    if (f) {
        fclose(f);
        f = NULL;
    }
    if (bmctx.basedir) free(bmctx.basedir);
    if (bmctx.pages) {
        for (int i = 0; i < bmctx.numpages; ++i) {
            if (bmctx.pages[i].buf)
                free(bmctx.pages[i].buf);
        }
        free(bmctx.pages);
    }
    return ret;
}
