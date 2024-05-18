#include <unordered_map>
#include <algorithm>
#include <array>

// Freetype
#include "freetype/FreeTypeAmalgam.h"

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

// Crop any row or column that is entirely empty
static void crop_bitmap(uint8_t *buffer, int width, int height, int *crop_x0, int *crop_y0, int *crop_width, int *crop_height)
{
    int x0 = 0, x1 = width, y0 = 0, y1 = height;
    for (int x=0; x<width; x++) {
        bool empty = true;
        for (int y=0; y<height; y++) {
            if (buffer[y * width + x] != 0) {
                empty = false;
                break;
            }
        }
        if (!empty) {
            x0 = x;
            break;
        }
    }
    for (int x=width-1; x>=0; x--) {
        bool empty = true;
        for (int y=0; y<height; y++) {
            if (buffer[y * width + x] != 0) {
                empty = false;
                break;
            }
        }
        if (!empty) {
            x1 = x+1;
            break;
        }
    }
    for (int y=0; y<height; y++) {
        bool empty = true;
        for (int x=0; x<width; x++) {
            if (buffer[y * width + x] != 0) {
                empty = false;
                break;
            }
        }
        if (!empty) {
            y0 = y;
            break;
        }
    }
    for (int y=height-1; y>=0; y--) {
        bool empty = true;
        for (int x=0; x<width; x++) {
            if (buffer[y * width + x] != 0) {
                empty = false;
                break;
            }
        }
        if (!empty) {
            y1 = y+1;
            break;
        }
    }

    *crop_x0 = x0;
    *crop_y0 = y0;
    *crop_width = x1 - x0;
    *crop_height = y1 - y0;
}


int convert_ttf(const char *infn, const char *outfn, std::vector<int>& ranges)
{
    int err;

    // Initialize the font
    FT_Library ftlib;
    err = FT_Init_FreeType(&ftlib);
    if (err) {
        fprintf(stderr, "cannot initialize FreeType\n");
        return 1;
    }

    FT_Face face;
    err = FT_New_Face(ftlib, infn, 0, &face);
    if (err) {
        fprintf(stderr, "cannot open font file: %s\n", infn);
        return 1;
    }

    FT_Size_RequestRec req;
    memset(&req, 0, sizeof(req));
    
    int point_size;
    if (flag_ttf_point_size == 0) {
        // Set the size based on scales 1.0, so that it defaults to the "correct" size.
        // This is good for TTF retro fonts which are actually bitmap fonts in TTF disguise.
        req.type = FT_SIZE_REQUEST_TYPE_SCALES;
        req.width = 1 << 16;
        req.height =1 << 16;
        point_size = face->bbox.yMax - face->bbox.yMin;
    } else {
        req.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
        req.height = flag_ttf_point_size << 6;
        point_size = flag_ttf_point_size;
    }
    FT_Request_Size(face, &req);

    int ascent = face->size->metrics.ascender >> 6;
    int descent = face->size->metrics.descender >> 6;
    int line_gap = (face->size->metrics.height >> 6) - ascent + descent; 
    int space_width = face->size->metrics.max_advance >> 6;
    rdpq_font_t *font = n64font_alloc(point_size, ascent, descent, line_gap, space_width);

    // Create a map from font64 glyph indices to truetype indices
    std::unordered_map<int, int> gidx_to_ttfidx;

    // Go through all the ranges
    int nimg = 0;
    for (int r=0; r<ranges.size(); r+=2) {
        if (flag_verbose)
            fprintf(stderr, "processing codepoint range: %04X - %04X\n", ranges[r], ranges[r+1]);
        n64font_addrange(font, ranges[r], ranges[r+1]);

        struct Glyph {
            int codepoint;          // unicode codepoint
            int gidx;               // index in the glyph array in font64
            int width, height;
            int xoff, yoff;
            int xadv;
            struct {
                int x0, y0, width, height;
            } crop;
            std::vector<uint8_t> bitmap;
        };

        std::vector<Glyph> glyphs;

        // Extract the glyphs for these ranges
        for (int g=ranges[r]; g<=ranges[r+1]; g++) {
            int ttf_idx = FT_Get_Char_Index(face, g);
            if (ttf_idx == 0) {
                if (flag_verbose >= 2)
                    fprintf(stderr, "  glyph %s [U+%04X]: not found\n", codepoint_to_utf8(g).c_str(), g);
                continue;
            }

            err = FT_Load_Glyph(face, ttf_idx, FT_LOAD_RENDER);
            if (err) {
                fprintf(stderr, "cannot load glyph: %04X\n", g);
                exit(1);
            }

            FT_GlyphSlot slot = face->glyph;
            FT_Bitmap bmp = slot->bitmap;

            Glyph glyph{
                .codepoint = g,
                .gidx = n64font_glyph(font, glyph.codepoint),
                .width = bmp.width,
                .height = bmp.rows,
                .xoff = slot->bitmap_left,
                .yoff = -slot->bitmap_top,
                .xadv = slot->advance.x,
                .crop = {0, 0, bmp.width, bmp.rows},
                .bitmap = std::vector<uint8_t>(bmp.width * bmp.rows),
            };

            switch (bmp.pixel_mode) {
            case FT_PIXEL_MODE_MONO:
                for (int y=0; y<bmp.rows; y++) {
                    for (int x=0; x<bmp.width; x++) {
                        glyph.bitmap[y * bmp.width + x] = (bmp.buffer[y * bmp.pitch + x / 8] & (1 << (7 - x % 8))) ? 255 : 0;
                    }
                }
                break;
            case FT_PIXEL_MODE_GRAY:
                // Prequantize the greyscale to 4bpp, as we're going to output 4bpp anyway
                for (int y=0; y<bmp.rows; y++) {
                    for (int x=0; x<bmp.width; x++) {
                        glyph.bitmap[y * bmp.width + x] = bmp.buffer[y * bmp.pitch + x] & 0xF0;
                    }
                }
                // After quantization, crop the bitmap if possible to save a few pixels
                crop_bitmap(&glyph.bitmap[0], glyph.width, glyph.height,
                    &glyph.crop.x0, &glyph.crop.y0, &glyph.crop.width, &glyph.crop.height);
                break;
            default:
                fprintf(stderr, "internal error: unsupported freetype pixel mode: %d\n", bmp.pixel_mode);
                return 1;
            }

            glyphs.push_back(glyph);
            gidx_to_ttfidx[glyph.gidx] = ttf_idx;
        }

        // Pack the glyphs into a texture
        rect_pack::Settings settings;
        memset(&settings, 0, sizeof(settings));
        settings.method = rect_pack::Method::Best;
        settings.max_width = 128;
        settings.max_height = 64;
        settings.border_padding = 1;
        settings.allow_rotate = false;
        
        std::vector<rect_pack::Size> sizes;
        for (int i=0; i<glyphs.size(); i++) {
            rect_pack::Size size;
            size.id = i;
            size.width = glyphs[i].crop.width + settings.border_padding;
            size.height = glyphs[i].crop.height + settings.border_padding;
            sizes.push_back(size);
        }

        std::vector<rect_pack::Sheet> sheets = rect_pack::pack(settings, sizes);

        // Create the actual textures
        for (int i=0; i<sheets.size(); i++) {
            rect_pack::Sheet& sheet = sheets[i];
            uint8_t *pixels = (uint8_t*)malloc(sheet.width * sheet.height);
            memset(pixels, 0, sheet.width * sheet.height);

            for (int j=0; j<sheet.rects.size(); j++) {
                rect_pack::Rect& rect = sheet.rects[j];
                Glyph& glyph = glyphs[rect.id];

                if (!rect.rotated) {
                    for (int y=0; y<glyph.crop.height; y++) {
                        for (int x=0; x<glyph.crop.width; x++) {
                            pixels[(rect.y + y) * sheet.width + (rect.x + x)] = glyph.bitmap[(y + glyph.crop.y0) * glyph.width + (x + glyph.crop.x0)];
                        }
                    }
                } else {
                    for (int y=0; y<glyph.crop.width; y++) {
                        for (int x=0; x<glyph.crop.height; x++) {
                            pixels[(rect.y + y) * sheet.width + (rect.x + x)] = glyph.bitmap[(x + glyph.crop.x0) * glyph.width + (y + glyph.crop.y0)];
                        }
                    }
                }

                glyph_t *gout = &font->glyphs[glyph.gidx];
                gout->natlas = i;
                gout->s = rect.x; gout->t = rect.y;
                gout->xoff = glyph.xoff + glyph.crop.x0;
                gout->yoff = glyph.yoff + glyph.crop.y0;
                gout->xoff2 = gout->xoff + glyph.crop.width - 1;
                gout->yoff2 = gout->yoff + glyph.crop.height - 1;
                gout->xadvance = glyph.xadv;

                if (flag_verbose >= 2) {
                    fprintf(stderr, "  glyph %s [U+%04X]: %d x %d, %d,%d %d,%d %.2f\n", 
                        codepoint_to_utf8(glyph.codepoint).c_str(), glyph.codepoint, 
                        glyph.crop.width, glyph.crop.height, gout->xoff, gout->yoff, gout->xoff2, gout->yoff2, glyph.xadv/64.f);
                }

                if(abs(gout->xoff) > 128 || abs(gout->yoff) > 128 || abs(gout->xoff2) > 128 || abs(gout->yoff2) > 128 ||
                    abs(gout->xadvance) > 32768)
                {
                    fprintf(stderr, "ERROR: font too big, please reduce point size (%d)\n", point_size);
                    return 1;
                }
            }

            n64font_addatlas(font, pixels, sheet.width, sheet.height, sheet.width, 1);

            if (flag_verbose)
                fprintf(stderr, "created atlas %d: %d x %d pixels (%zu glyphs)\n", i, sheet.width, sheet.height, sheet.rects.size());

            if (flag_debug) {
                char *imgfn = NULL;
                asprintf(&imgfn, "%s_%d.png", outfn, nimg);
                stbi_write_png(imgfn, sheet.width, sheet.height, 1, pixels, sheet.width);
                if (flag_verbose)
                    fprintf(stderr, "wrote debug image: %s\n", imgfn);
                free(imgfn);
            }
            free(pixels);
            ++nimg;
        }
    }

    // Add kerning information, if enabled on command line and available in the font
    if (flag_kerning && FT_HAS_KERNING(face)) {
        const int ascii_range_start = 0x20;
        const int ascii_range_len = 0x80 - 0x20;

        if (flag_verbose)
            fprintf(stderr, "collecting kerning information\n");

        std::vector<n64font_kern> kernings;

        // Prepare the kerning table. Go through all ranges, and within each range, construct a N*N table
        // for all the pairs [glyph1, glyph2] for all glyphs in that range. This means that we don't
        // collect kerning for pairs of glyphs in different ranges, but that shouldn't really matter in real
        // use cases (eg: kerning between a cyrillic and a greek letter is probably not very useful).
        // In addition to this, always collect kerning against all ASCII characters, because those are common
        // enough to be useful with all the ranges.
        for (int r=0;r<font->num_ranges; r++) {
            range_t *range = &font->ranges[r];

            // Number of codepoints to iterate twice (N^2). These are the glyphs in the range
            // plus the ASCII range (unless the range *is* ASCII itself).
            int num_codepoints = range->num_codepoints;
            if (range->first_codepoint != ascii_range_start)
                num_codepoints += ascii_range_len;

            // Now do the N*N loop
            for (int i=0;i<num_codepoints;i++) {
                int gidx1 = (i >= range->num_codepoints) ? i-range->num_codepoints : range->first_glyph + i;
                int ttfidx1 = gidx_to_ttfidx[gidx1];

                for (int j=0;j<num_codepoints;j++) {
                    int gidx2 = (j >= range->num_codepoints) ? j-range->num_codepoints : range->first_glyph + j;
                    int ttfidx2 = gidx_to_ttfidx[gidx2];

                    FT_Vector kerning;
                    FT_Get_Kerning(face, ttfidx1, ttfidx2, FT_KERNING_DEFAULT, &kerning);
                    
                    if (kerning.x != 0) {
                        // Add the kerning entry. Scale the advance to fit 8 bit, assuming
                        // the kerning will never be bigger than the point size (and usually much
                        // smaller). This makes good use of the available precision.
                        kernings.push_back({
                            .glyph1 = gidx1, .glyph2 = gidx2,
                            .kerning = (kerning.x >> 6),
                        });
                        if (flag_verbose >= 2) {
                            int codepoint1 = (i >= range->num_codepoints) ? ascii_range_start + i - range->num_codepoints : range->first_codepoint + i;
                            int codepoint2 = (j >= range->num_codepoints) ? ascii_range_start + j - range->num_codepoints : range->first_codepoint + j;
                            fprintf(stderr, "  kerning %s -> %s: %ld\n", codepoint_to_utf8(codepoint1).c_str(), codepoint_to_utf8(codepoint2).c_str(), kerning.x >> 6);
                        }
                    }
                }
            }

            n64font_addkerning(font, kernings);
        }
    }

    if (flag_ellipsis_repeats > 0)
        n64font_add_ellipsis(font, flag_ellipsis_cp, flag_ellipsis_repeats);

    // Write output file
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "cannot open output file: %s\n", outfn);
        return 1;
    }
    n64font_write(font, out);
    fclose(out);

    n64font_free(font);

    FT_Done_Face(face);
    FT_Done_FreeType(ftlib);
    return 0;
}

