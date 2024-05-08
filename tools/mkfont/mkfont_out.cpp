
void n64font_write(rdpq_font_t *fnt, FILE *out)
{
    // Write header
    w8(out, fnt->magic[0]);
    w8(out, fnt->magic[1]);
    w8(out, fnt->magic[2]);
    w8(out, fnt->version);
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
    w32(out, 1);   // num styles (not supported by mkfont yet)
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
        for (int j=0;j<3;j++) w8(out, (uint8_t)0);
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

    // Write styles
    walign(out, 16);
    uint32_t offset_styles = ftell(out);
    w32(out, 0xFFFFFFFF); // color
    w32(out, 0); // runtime pointer
    for (int i=0; i<255; i++)
    {
        w32(out, 0); // color
        w32(out, 0); // runtime pointer
    }

    // Write offsets
    fseek(out, off_placeholders, SEEK_SET);
    w32(out, offset_ranges);
    w32(out, offset_glypes);
    w32(out, offset_atlases);
    w32(out, offset_kernings);
    w32(out, offset_styles);

    fseek(out, offset_end, SEEK_SET);
}

void n64font_addrange(rdpq_font_t *fnt, int first, int last)
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

int n64font_glyph(rdpq_font_t *fnt, uint32_t cp)
{
    for (int i=0;i<fnt->num_ranges;i++)
    {
        if (cp >= fnt->ranges[i].first_codepoint && cp < fnt->ranges[i].first_codepoint + fnt->ranges[i].num_codepoints)
            return fnt->ranges[i].first_glyph + cp - fnt->ranges[i].first_codepoint;
    }
    return -1;
}

static void png_write_func(void *context, void *data, int size)
{
    FILE *f = (FILE*)context;
    fwrite(data, 1, size, f);
}

void n64font_addatlas(rdpq_font_t *fnt, uint8_t *buf, int width, int height, int stride, int bytes_per_pixel)
{
    static char *mksprite = NULL;
    if (!mksprite) asprintf(&mksprite, "%s/bin/mksprite", n64_inst);

    // Prepare mksprite command line
    struct subprocess_s subp;
    const char *cmd_addr[16] = {0}; int i = 0;
    cmd_addr[i++] = mksprite;
    cmd_addr[i++] = "--format";
    cmd_addr[i++] = "I4";
    cmd_addr[i++] = "--compress";  // don't compress the individual sprite (the font itself will be compressed)
    cmd_addr[i++] = "0";
    
    // Start mksprite
    if (subprocess_create(cmd_addr, subprocess_option_no_window|subprocess_option_inherit_environment, &subp) != 0) {
        fprintf(stderr, "Error: cannot run: %s\n", mksprite);
        exit(1);
    }

    // Write PNG to standard input of mksprite
    FILE *mksprite_in = subprocess_stdin(&subp);
    stbi_write_png_to_func(png_write_func, mksprite_in, width, height, bytes_per_pixel, buf, stride);
    fclose(mksprite_in); subp.stdin_file = SUBPROCESS_NULL;

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
    while (1) {
        char buf[4096];
        int n = fread(buf, 1, sizeof(buf), mksprite_err);
        if (n == 0) break;
        fwrite(buf, 1, n, stderr);
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

struct n64font_kern {
    int glyph1;
    int glyph2;
    int kerning;
};

void n64font_addkerning(rdpq_font_t *fnt, std::vector<n64font_kern>& kernings)
{
    assert(fnt->glyphs); // first we need the glyphs

    // Sort kernings by g1 and then g2
    std::sort(kernings.begin(), kernings.end(), [](const n64font_kern& k1, const n64font_kern& k2){
        if (k1.glyph1 != k2.glyph1) return k1.glyph1 < k2.glyph1;
        return k1.glyph2 < k2.glyph2;
    });

    // Allocate output data structure
    fnt->num_kerning = kernings.size()+1;
    fnt->kerning = (kerning_t*)malloc(fnt->num_kerning * sizeof(kerning_t));
    memset(&fnt->kerning[0], 0, sizeof(kerning_t));

    for (int i=0; i<kernings.size(); i++) {
        // Copy kerning data into output
        n64font_kern* ink = &kernings[i];
        assert(ink->kerning >= (int)-fnt->point_size && ink->kerning <= (int)fnt->point_size);
        fnt->kerning[i+1].glyph2 = ink->glyph2;
        fnt->kerning[i+1].kerning = ink->kerning * 127 / (int)fnt->point_size;

        // Update lo/hi indices for current glyph.
        if (i==0 || ink->glyph1 != ink[-1].glyph1)
            fnt->glyphs[ink->glyph1].kerning_lo = i+1;
        fnt->glyphs[ink->glyph1].kerning_hi = i+1;
    }
}

void n64font_add_ellipsis(rdpq_font_t *fnt, int ellipsis_cp, int ellipsis_repeats)
{
    int ellipsis_glyph = n64font_glyph(fnt, ellipsis_cp);
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

rdpq_font_t* n64font_alloc(int point_size, int ascent, int descent, int line_gap, int space_width)
{
    rdpq_font_t *fnt = (rdpq_font_t*)calloc(1, sizeof(rdpq_font_t));
    memcpy(fnt->magic, FONT_MAGIC, 3);
    fnt->version = 4;
    fnt->point_size = point_size;
    fnt->ascent = ascent;
    fnt->descent = descent;
    fnt->line_gap = line_gap;
    fnt->space_width = space_width;
    return fnt;
}

void n64font_free(rdpq_font_t *fnt)
{
    for (int i=0;i<fnt->num_atlases;i++)
        free(fnt->atlases[i].sprite);
    free(fnt->atlases);
    free(fnt->glyphs);
    free(fnt->ranges);
    free(fnt);
}