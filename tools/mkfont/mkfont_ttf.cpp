#include <unordered_map>
#include <algorithm>
#include <array>

// Freetype
#include "freetype/FreeTypeAmalgam.h"

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
    if (flag_verbose) printf("asc: %d dec: %d scalable:%d fixed:%d\n", ascent, descent, FT_IS_SCALABLE(face), FT_HAS_FIXED_SIZES(face));

    Font font(outfn, point_size, ascent, descent, line_gap, space_width);

    // Create a map from font64 glyph indices to truetype indices
    std::unordered_map<int, int> gidx_to_ttfidx;

    FT_Stroker stroker;
    FT_Stroker_New(ftlib, &stroker);
    FT_Stroker_Set(stroker, 1*64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

    // Go through all the ranges
    for (int r=0; r<ranges.size(); r+=2) {
        if (flag_verbose)
            fprintf(stderr, "processing codepoint range: %04X - %04X\n", ranges[r], ranges[r+1]);
        font.add_range(ranges[r], ranges[r+1]);

        // Extract the glyphs for these ranges
        for (int g=ranges[r]; g<=ranges[r+1]; g++) {
            int ttf_idx = FT_Get_Char_Index(face, g);
            if (ttf_idx == 0) {
                if (flag_verbose >= 2)
                    fprintf(stderr, "  glyph %s [U+%04X]: not found\n", codepoint_to_utf8(g).c_str(), g);
                continue;
            }

            err = FT_Load_Glyph(face, ttf_idx, FT_LOAD_RENDER | (flag_ttf_monochrome ? FT_LOAD_TARGET_MONO : 0));
            if (err) {
                fprintf(stderr, "cannot load glyph: %04X\n", g);
                exit(1);
            }

            FT_GlyphSlot slot = face->glyph;
            FT_Bitmap bmp = slot->bitmap;

            Image img(FMT_I8, bmp.width, bmp.rows);

            switch (bmp.pixel_mode) {
            case FT_PIXEL_MODE_MONO:
                for (int y=0; y<bmp.rows; y++) {
                    for (int x=0; x<bmp.width; x++) {
                        img[y][x] = (bmp.buffer[y * bmp.pitch + x / 8] & (1 << (7 - x % 8))) ? 255 : 0;
                    }
                }
                break;
            case FT_PIXEL_MODE_GRAY:
                for (int y=0; y<bmp.rows; y++) {
                    for (int x=0; x<bmp.width; x++) {
                        img[y][x] = bmp.buffer[y * bmp.pitch + x] & 0xF0;
                    }
                }
                break;
            default:
                fprintf(stderr, "internal error: unsupported freetype pixel mode: %d\n", bmp.pixel_mode);
                return 1;
            }

            // } else {

            //     FT_Glyph ftglyph1, ftglyph2;
            //     FT_Load_Glyph(face, ttf_idx, FT_LOAD_DEFAULT);
            //     FT_Get_Glyph(face->glyph, &ftglyph1);
            //     FT_Glyph_Copy(ftglyph1, &ftglyph2);

            //     FT_Glyph_To_Bitmap(&ftglyph1, FT_RENDER_MODE_NORMAL, nullptr, true);
            //     FT_BitmapGlyph bitmapGlyph1 = reinterpret_cast<FT_BitmapGlyph>(ftglyph1);

            //     FT_Glyph_StrokeBorder(&ftglyph2, stroker, false, true);
            //     FT_Glyph_To_Bitmap(&ftglyph2, FT_RENDER_MODE_NORMAL, nullptr, true);
            //     FT_BitmapGlyph bitmapGlyph2 = reinterpret_cast<FT_BitmapGlyph>(ftglyph2);

            //     bmp_width = 128;
            //     bmp_height = 128;

            //     bitmap.resize(128 * 128);
            //     uint8_t pixel32[128*128*4];  // rgba buffer
            //     memset(pixel32, 0, 128*128*4);

            //     // printf("bitmap1: %d x %d -- %d,%d\n", bitmapGlyph1->bitmap.width, bitmapGlyph1->bitmap.rows,
            //     //     bitmapGlyph1->left, bitmapGlyph1->top);
            //     // printf("bitmap2: %d x %d -- %d,%d\n", bitmapGlyph2->bitmap.width, bitmapGlyph2->bitmap.rows,
            //     //     bitmapGlyph2->left, bitmapGlyph2->top);

            //     const int outR = 0x0, outG = 0x0, outB = 0x0;
            //     const int fillR = 0xFF, fillG = 0xFF, fillB = 0xFF;

            //     // Copy the second bitmap to the rgba buffer with yellow color
            //     for (int y = 0; y < bitmapGlyph2->bitmap.rows; y++) {
            //         for (int x = 0; x < bitmapGlyph2->bitmap.width; x++) {
            //             uint8_t v = bitmapGlyph2->bitmap.buffer[y * bitmapGlyph2->bitmap.width + x];
            //             int i = (y + 70 - bitmapGlyph2->top) * 128 + x + 10 + bitmapGlyph2->left;
            //             pixel32[i * 4 + 0] = outR;
            //             pixel32[i * 4 + 1] = outG;
            //             pixel32[i * 4 + 2] = outB;
            //             pixel32[i * 4 + 3] = v;
            //         }
            //     }
            //     (void)bitmapGlyph2;

            //     // Copy the first bitmap to the rgba buffer with red color, blending
            //     // it over the yellow color
            //     for (int y = 0; y < bitmapGlyph1->bitmap.rows; y++) {
            //         for (int x = 0; x < bitmapGlyph1->bitmap.width; x++) {
            //             uint8_t v = bitmapGlyph1->bitmap.buffer[y * bitmapGlyph1->bitmap.width + x];
            //             int i = (y + 70 - bitmapGlyph1->top) * 128 + x + 10 + bitmapGlyph1->left;
            //             uint8_t *dst = &pixel32[i * 4];
            //             dst[0] = dst[0] + ((fillR - dst[0]) * v) / 255;
            //             dst[1] = dst[1] + ((fillG - dst[1]) * v) / 255;
            //             dst[2] = dst[2] + ((fillB - dst[2]) * v) / 255;
            //             dst[3] = dst[3] + v >= 128 ? 255 : 0;
            //         }
            //     }

            //     bmp_bpp = 4;
            //     bmp_left = 0;
            //     bmp_top = 0;
            //     bmp_adv = 0;
            // }

            int gidx = font.add_glyph(g, img, slot->bitmap_left, -slot->bitmap_top, slot->advance.x);
            gidx_to_ttfidx[gidx] = ttf_idx;
        }

        font.make_atlases();
    }

    // Add kerning information, if enabled on command line and available in the font
    if (flag_kerning && FT_HAS_KERNING(face)) {
        const int ascii_range_start = 0x20;
        const int ascii_range_len = 0x80 - 0x20;

        if (flag_verbose)
            fprintf(stderr, "collecting kerning information\n");

        // Prepare the kerning table. Go through all ranges, and within each range, construct a N*N table
        // for all the pairs [glyph1, glyph2] for all glyphs in that range. This means that we don't
        // collect kerning for pairs of glyphs in different ranges, but that shouldn't really matter in real
        // use cases (eg: kerning between a cyrillic and a greek letter is probably not very useful).
        // In addition to this, always collect kerning against all ASCII characters, because those are common
        // enough to be useful with all the ranges.
        for (int r=0;r<font.fnt->num_ranges; r++) {
            range_t *range = &font.fnt->ranges[r];

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
                        font.add_kerning(gidx1, gidx2, kerning.x >> 6);

                        if (flag_verbose >= 2) {
                            int codepoint1 = (i >= range->num_codepoints) ? ascii_range_start + i - range->num_codepoints : range->first_codepoint + i;
                            int codepoint2 = (j >= range->num_codepoints) ? ascii_range_start + j - range->num_codepoints : range->first_codepoint + j;
                            fprintf(stderr, "  kerning %s -> %s: %ld\n", codepoint_to_utf8(codepoint1).c_str(), codepoint_to_utf8(codepoint2).c_str(), kerning.x >> 6);
                        }
                    }
                }
            }
        }
        
        font.make_kernings();
    }

    if (flag_ellipsis_repeats > 0)
        font.add_ellipsis(flag_ellipsis_cp, flag_ellipsis_repeats);

    // Write output file
    font.write();

    FT_Done_Face(face);
    FT_Done_FreeType(ftlib);
    return 0;
}

