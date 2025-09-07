/*
    mkfont_ttf: convert TTF files to N64 font format
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
 */
#include <unordered_map>
#include <algorithm>
#include <array>
#include <map>

// Freetype
#include "freetype/FreeTypeAmalgam.h"

// PlutoSVG
#define PLUTOSVG_BUILD_STATIC
#define PLUTOVG_BUILD_STATIC
#include "plutosvg/plutosvg-ft.h"

static bool is_monochrome(FT_Face face)
{
    // Check if the font is monochrome at the current size, by rendering a
    // few ASCII glyphs and checking if all pixels in them are either fully
    // transparent or fully solid.
    for (int ch='a'; ch <= 'f'; ch++) {
        int idx = FT_Get_Char_Index(face, ch);
        if (idx == 0) continue;

        FT_Load_Glyph(face, idx, FT_LOAD_RENDER);

        FT_GlyphSlot slot = face->glyph;
        FT_Bitmap bmp = slot->bitmap;
        if (bmp.pixel_mode == FT_PIXEL_MODE_MONO)
            continue;

        for (int y=0; y<bmp.rows; y++) {
            for (int x=0; x<bmp.width; x++) {
                switch (bmp.pixel_mode) {
                    case FT_PIXEL_MODE_GRAY:
                        if (bmp.buffer[y * bmp.pitch + x] != 0 && bmp.buffer[y * bmp.pitch + x] != 0xFF)
                            return false;
                        break;
                    default:
                        assert(false && "Unsupported pixel mode");
                }
            }
        }
    }

    return true;
}

static bool is_colored_aliased(FT_Face face)
{
    assert(FT_HAS_COLOR(face));

    for (int ch='a'; ch <= 'f'; ch++) {
        int idx = FT_Get_Char_Index(face, ch);
        if (idx == 0) continue;

        FT_Load_Glyph(face, idx, FT_LOAD_RENDER | FT_LOAD_COLOR);

        FT_GlyphSlot slot = face->glyph;
        FT_Bitmap bmp = slot->bitmap;

        if (bmp.pixel_mode != FT_PIXEL_MODE_BGRA)
            continue;

        for (int y=0; y<bmp.rows; y++) {
            for (int x=0; x<bmp.width; x++) {
                if (bmp.buffer[y * bmp.pitch + x*4 + 3] != 0 && 
                    bmp.buffer[y * bmp.pitch + x*4 + 3] != 0xFF) {
                        return true;
                }
            }
        }
    }

    return false;
}

static bool is_combined_glyph(uint32_t cp)
{
    // Check if the codepoint is a combined glyph
    // (e.g. a diacritic that should be combined with the previous character)
    return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1DC0 && cp <= 0x1DFF) ||
           (cp >= 0x20D0 && cp <= 0x20FF) || (cp >= 0xFE20 && cp <= 0xFE2F);
}

static void apply_aspect_ratio_to_request(FT_Size_RequestRec *req)
{
    // Apply custom aspect ratio by modifying the width request
    // If aspect_ratio > 1.0, glyphs will be wider
    // If aspect_ratio < 1.0, glyphs will be narrower
    if (flag_ttf_aspect_ratio != 1.0) {
        if (req->type == FT_SIZE_REQUEST_TYPE_SCALES) {
            req->width = (FT_Fixed)(req->width * flag_ttf_aspect_ratio);
        } else {
            // For nominal sizes, we need to adjust the width proportionally
            req->width = (FT_F26Dot6)(req->height * flag_ttf_aspect_ratio);
        }
    }
}

static int font_set_default_size(FT_Face face)
{
    FT_Size_RequestRec req;
    memset(&req, 0, sizeof(req));

    // Set the size based on scales 1.0, so that it defaults to the "correct" size.
    // This is good for TTF retro fonts which are actually bitmap fonts in TTF disguise.
    // Notice that we try hard to find if this font is monochrome because that's what
    // the user expects for monochrome fonts.
    req.type = FT_SIZE_REQUEST_TYPE_SCALES;
    req.width = 1 << 16;
    req.height =1 << 16;
    apply_aspect_ratio_to_request(&req);
    FT_Request_Size(face, &req);

    if (is_monochrome(face))
        return face->bbox.yMax - face->bbox.yMin;

    // If the font is not monochrome at its default size, try harder. Some monochrome fonts
    // come with a default scaling size configured in the TTF, so check
    // all "reasonable" sizes to check if the font is monochrome at one of those:
    int point_size = 0;
    for (int sz = 3; sz <= 30; sz++) {
        memset(&req, 0, sizeof(req));
        req.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
        req.height = sz << 6;
        apply_aspect_ratio_to_request(&req);
        FT_Request_Size(face, &req);

        if (is_monochrome(face)) {
            point_size = sz;
            break;
        }
    }

    if (point_size)
        return point_size;

    // Ok we couldn't find a size at which this font is monochrome. We assume
    // it's an aliased font. Let's just a default size of 12 pixels that is a
    // good compromise for most fonts at 320x240.
    const int DEFAULT_SIZE = 12;
    memset(&req, 0, sizeof(req));
    req.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
    req.height = DEFAULT_SIZE << 6;
    apply_aspect_ratio_to_request(&req);
    FT_Request_Size(face, &req);
    return DEFAULT_SIZE;
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

    // Set PlutoSVG hooks for the SVG module
    if(FT_Property_Set(ftlib, "ot-svg", "svg-hooks", &plutosvg_ft_hooks)) {
        fprintf(stderr, "cannot set FreeType SVG hooks\n");
        return -1;
    }

    FT_Face face;
    err = FT_New_Face(ftlib, infn, 0, &face);
    if (err) {
        fprintf(stderr, "cannot open font file: %s\n", infn);
        return 1;
    }

    int point_size;
    if (flag_ttf_point_size == 0) {
        point_size = font_set_default_size(face);
    } else {
        // Use the point size requested by the user
        FT_Size_RequestRec req;
        memset(&req, 0, sizeof(req));
        req.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
        req.height = flag_ttf_point_size << 6;
        apply_aspect_ratio_to_request(&req);
        FT_Request_Size(face, &req);
        point_size = flag_ttf_point_size;
    }

    // If the font is monochrome, remember it. We use the same flag as-if 
    // monochrome rendering was requested from the command-line, as the effect is the same.
    if (!flag_ttf_monochrome && is_monochrome(face))
        flag_ttf_monochrome = true;

    // Decide the fonttype
    fonttype_t fonttype;
    if (!flag_ttf_outline) {
        if (FT_HAS_COLOR(face))
            fonttype = FONT_TYPE_BITMAP;
        else if (flag_ttf_monochrome)
            fonttype = FONT_TYPE_MONO;
        else
            fonttype = FONT_TYPE_ALIASED;
    } else {
        if (!FT_IS_SCALABLE(face)) {
            fprintf(stderr, "error: outline generation requires a scalable font\n");
            return 1;
        }
        if (FT_HAS_COLOR(face)) {
            fprintf(stderr, "error: outline generation not supported for color fonts\n");
            return 1;
        } else if (flag_ttf_monochrome)
            fonttype = FONT_TYPE_MONO_OUTLINE;
        else
            fonttype = FONT_TYPE_ALIASED_OUTLINE;
    }

    int ascent = face->size->metrics.ascender >> 6;
    int descent = face->size->metrics.descender >> 6;
    int line_gap = (face->size->metrics.height >> 6) - ascent + descent; 
    int space_width = face->size->metrics.max_advance >> 6;
    if (flag_verbose) printf("Metrics: ascent=%d, descent=%d, line_gap=%d, space_width=%d\n", ascent, descent, line_gap, space_width);
    Font font(outfn, fonttype, point_size, ascent, descent, line_gap, space_width);

    if (!FT_HAS_COLOR(face) && flag_bmfont_format != FMT_NONE) {
        fprintf(stderr, "error: --format specified, but the font is not colored: %s\n", infn);
        return 1;
    }
    if (FT_HAS_COLOR(face)) {
        if (flag_bmfont_format == FMT_NONE) {
            flag_bmfont_format = is_colored_aliased(face) ? FMT_RGBA32 : FMT_RGBA16;
        }
        font.bmp_outfmt = flag_bmfont_format;
    }

    // Create a map from font64 glyph indices to truetype indices
    std::unordered_map<int, int> gidx_to_ttfidx;

    // Create the stroker (in case it's needed later)
    FT_Stroker stroker;
    FT_Stroker_New(ftlib, &stroker);
    FT_Stroker_Set(stroker, flag_ttf_outline * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

    // If ranges is emtpy, it means we need to extract all the glyphs from the font
    if (ranges.empty()) {
        std::vector<uint32_t> unicode_ranges;
        for (auto &b : unicode_blocks)
            unicode_ranges.push_back(b.first);

        unsigned idx;
        std::map<int, std::pair<int, int>> range_map;
        // Go through all the glyphs in the font
        uint32_t cp = FT_Get_First_Char(face, &idx);
        while (idx) {
            if (!is_combined_glyph(cp)) {
                // Search which unicode range this glyph belongs to
                int range = *(std::upper_bound(unicode_ranges.begin(), unicode_ranges.end(), cp)-1);

                // Update the min/max codepoint for this range
                auto r = range_map.find(range);
                if (r != range_map.end()) {
                    r->second.first = MIN(r->second.first, cp);
                    r->second.second = MAX(r->second.second, cp);
                } else {
                    range_map.insert({range, {cp, cp}});
                }
            }
            cp = FT_Get_Next_Char(face, cp, &idx);
        }
        // Convert the map back to a the ranges vector
        for (auto r : range_map) {
            ranges.push_back(r.second.first);
            ranges.push_back(r.second.second);
        }
    }
    
    bool warned_combined = false;

    // Go through all the ranges
    for (int r=0; r<ranges.size(); r+=2) {
        if (flag_verbose)
            fprintf(stderr, "processing codepoint range: %04X - %04X\n", ranges[r], ranges[r+1]);
        font.add_range(ranges[r], ranges[r+1]);

        // Extract the glyphs for these ranges
        for (int g=ranges[r]; g<=ranges[r+1]; g++) {
            if (!flag_charset.empty() && flag_charset.find(g) == flag_charset.end())
                continue;
            if (is_combined_glyph(g)) {
                if (!flag_charset.empty() && !warned_combined) {
                    fprintf(stderr, "WARNING: charset file contains combined diacritical marks, which are not supported\n");
                    warned_combined = true;
                }
                continue;
            }

            int ttf_idx = FT_Get_Char_Index(face, g);
            if (ttf_idx == 0) {
                if (flag_verbose >= 3)
                    fprintf(stderr, "  glyph %s [U+%04X]: not found\n", codepoint_to_utf8(g).c_str(), g);
                continue;
            }

            // For color fonts, we need to try different loading strategies
            int load_flags = FT_LOAD_RENDER;
            if (flag_ttf_monochrome) load_flags |= FT_LOAD_TARGET_MONO;
            if (FT_HAS_COLOR(face))  load_flags |= FT_LOAD_COLOR;
            err = FT_Load_Glyph(face, ttf_idx, load_flags);
            if (err) {
                fprintf(stderr, "cannot load glyph: %04X\n", g);
                exit(1);
            }

            if (flag_ttf_outline == 0) {
                FT_GlyphSlot slot = face->glyph;
                FT_Bitmap bmp = slot->bitmap;
                
                assert(bmp.width >= 0 && bmp.rows >= 0);

                Image img;
                switch (bmp.pixel_mode) {
                case FT_PIXEL_MODE_BGRA:
                    // Color fonts
                    img = Image(FMT_RGBA32, bmp.width, bmp.rows);
                    for (int y=0; y<bmp.rows; y++) {
                        for (int x=0; x<bmp.width; x++) {
                            uint8_t *src = &bmp.buffer[y * bmp.pitch + x * 4];
                            uint8_t b = src[0], g = src[1], r = src[2], a = src[3];
                            if (a > 0) { r = (r * 255) / a; g = (g * 255) / a; b = (b * 255) / a; }
                            img[y][x] = (r << 24) | (g << 16) | (b << 8) | a;
                        }
                    }
                    break;
                case FT_PIXEL_MODE_MONO:
                    img = Image(FMT_I8, bmp.width, bmp.rows);
                    for (int y=0; y<bmp.rows; y++) {
                        for (int x=0; x<bmp.width; x++) {
                            img[y][x] = (bmp.buffer[y * bmp.pitch + x / 8] & (1 << (7 - x % 8))) ? 255 : 0;
                        }
                    }
                    // If for some reason this glyph was rendered monochrome (eg: spaces...)
                    // uniform it with the others
                    if (FT_HAS_COLOR(face)) img = img.convert(FMT_RGBA32);
                    break;
                case FT_PIXEL_MODE_GRAY:
                    img = Image(FMT_I8, bmp.width, bmp.rows);
                    for (int y=0; y<bmp.rows; y++) {
                        for (int x=0; x<bmp.width; x++) {
                            // For greyscale, mask out the lower 4 bits because we
                            // are going to save a I4 anyway.
                            img[y][x] = bmp.buffer[y * bmp.pitch + x] & 0xF0;
                        }
                    }
                    if (FT_HAS_COLOR(face)) img = img.convert(FMT_RGBA32);
                    break;
                default:
                    fprintf(stderr, "internal error: unsupported freetype pixel mode: %d\n", bmp.pixel_mode);
                    return 1;
                }

                int gidx = font.add_glyph(g, std::move(img), slot->bitmap_left, -slot->bitmap_top, slot->advance.x/64 + flag_ttf_char_spacing);
                gidx_to_ttfidx[gidx] = ttf_idx;

            } else {
                // Load the glyph without rendering it, we will do it ourselves.
                // Avoid bitmaps was we can't stroke them.
                FT_Glyph ftglyph1, ftglyph2;
                FT_Load_Glyph(face, ttf_idx, FT_LOAD_NO_BITMAP);

                // If the glyph was still returned in bitmap format, we cannot
                // stroke it, so just abort. User is required to select ranges that
                // can be outlined when outline is requested.
                if (face->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
                    fprintf(stderr, "error: outline generation requires a scalable font (glyph U+%04X is bitmap, at least at this size)\n", g);
                    return 1;
                }

                // Get the glyph, make a copy of it, and stroke the copy to create the outline
                FT_Get_Glyph(face->glyph, &ftglyph1);
                FT_Glyph_Copy(ftglyph1, &ftglyph2);
                FT_Glyph_StrokeBorder(&ftglyph2, stroker, false, true);

                // Render both the filled glyph and the stroked one. We request
                // MONO rendering if we are doing monochrome output.
                FT_Render_Mode rm = flag_ttf_monochrome ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL;
                FT_Glyph_To_Bitmap(&ftglyph1, rm, nullptr, true);
                FT_Glyph_To_Bitmap(&ftglyph2, rm, nullptr, true);
                FT_BitmapGlyph bitmapGlyph1 = reinterpret_cast<FT_BitmapGlyph>(ftglyph1);
                FT_BitmapGlyph bitmapGlyph2 = reinterpret_cast<FT_BitmapGlyph>(ftglyph2);

                // Calculate the union of the two bitmaps. Notice that the Y coordinates (top) is
                // reversed in freetype, so positive is up, which means that all calculations are inverted here.
                int img_top = std::max(bitmapGlyph1->top, bitmapGlyph2->top);
                int img_left = std::min(bitmapGlyph1->left, bitmapGlyph2->left);
                int img_bottom = std::min(bitmapGlyph1->top - (int)bitmapGlyph1->bitmap.rows, bitmapGlyph2->top - (int)bitmapGlyph2->bitmap.rows);
                int img_right = std::max(bitmapGlyph1->left + (int)bitmapGlyph1->bitmap.width, bitmapGlyph2->left + (int)bitmapGlyph2->bitmap.width);
                
                int img_width = img_right - img_left;
                int img_height = img_top - img_bottom;

                // Allow for empty images (eg: space). We need to call add_glyph for them anyway
                assert(img_width >= 0 && img_height >= 0);
                Image img = Image(FMT_IA16, img_width, img_height);

                // Copy the outline bitmap to the image
                for (int y = 0; y < bitmapGlyph2->bitmap.rows; y++) {
                    for (int x = 0; x < bitmapGlyph2->bitmap.width; x++) {
                        uint8_t v;
                        switch (bitmapGlyph2->bitmap.pixel_mode) {
                        case FT_PIXEL_MODE_MONO:
                            v = (bitmapGlyph2->bitmap.buffer[y * bitmapGlyph2->bitmap.pitch + x / 8] & (1 << (7 - x % 8))) ? 0xFF : 0;
                            break;
                        case FT_PIXEL_MODE_GRAY:
                            v = bitmapGlyph2->bitmap.buffer[y * bitmapGlyph2->bitmap.pitch + x];
                            break;
                        default:
                            assert(!"unsupported pixel mode in outline calculation");
                        }
                        if (v != 0) {
                            img[y + img_top - bitmapGlyph2->top][x - img_left + bitmapGlyph2->left] = v;
                        }
                    }
                }

                // Copy the fill bitmap to the image, over the outline, blending it in.
                for (int y = 0; y < bitmapGlyph1->bitmap.rows; y++) {
                    for (int x = 0; x < bitmapGlyph1->bitmap.width; x++) {
                        uint8_t v;
                        switch (bitmapGlyph1->bitmap.pixel_mode) {
                        case FT_PIXEL_MODE_MONO:
                            v = (bitmapGlyph1->bitmap.buffer[y * bitmapGlyph1->bitmap.pitch + x / 8] & (1 << (7 - x % 8))) ? 0xFF : 0;
                            break;
                        case FT_PIXEL_MODE_GRAY:
                            v = bitmapGlyph1->bitmap.buffer[y * bitmapGlyph1->bitmap.pitch + x];
                            break;
                        default:
                            assert(!"unsupported pixel mode in outline calculation");
                        }
                        if (v != 0) {
                            auto &&dst = img[y + img_top - bitmapGlyph1->top][x - img_left + bitmapGlyph1->left];
                            assert(dst.data[0] == 0);
                            dst.data[0] = v;
                            dst.data[1] = std::min(0xFF, dst.data[1] + v);
                        }
                    }
                }

                int gidx = font.add_glyph(g, std::move(img), img_left, -img_top, face->glyph->advance.x/64 + flag_ttf_char_spacing);
                gidx_to_ttfidx[gidx] = ttf_idx;
            }
        }

        if (font.glyphs.empty()) {
            fprintf(stderr, "WARNING: %s: no glyphs found in range %X-%X\n", infn, ranges[r], ranges[r+1]);
            continue;
        }

        // Create atlases for glyphs in this range
        font.make_atlases();
    }

    // Create the sparse range table
    font.make_sparse_ranges();

    // Add kerning information, if enabled on command line and available in the font
    if (flag_kerning && FT_HAS_KERNING(face)) {
        const int ascii_range_start = 0x20;
        const int ascii_range_len = 0x80 - 0x20;

        if (flag_verbose >= 2)
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
                int ttfidx1 = gidx_to_ttfidx.find(gidx1) != gidx_to_ttfidx.end() ? gidx_to_ttfidx[gidx1] : -1;
                if (ttfidx1 == -1) continue;

                for (int j=0;j<num_codepoints;j++) {
                    int gidx2 = (j >= range->num_codepoints) ? j-range->num_codepoints : range->first_glyph + j;
                    int ttfidx2 = gidx_to_ttfidx.find(gidx2) != gidx_to_ttfidx.end() ? gidx_to_ttfidx[gidx2] : -1;
                    if (ttfidx2 == -1) continue;

                    FT_Vector kerning;
                    FT_Get_Kerning(face, ttfidx1, ttfidx2, FT_KERNING_DEFAULT, &kerning);
                    
                    if (kerning.x != 0) {
                        // Add the kerning entry. Scale the advance to fit 8 bit, assuming
                        // the kerning will never be bigger than the point size (and usually much
                        // smaller). This makes good use of the available precision.
                        font.add_kerning(gidx1, gidx2, kerning.x >> 6);

                        if (flag_verbose >= 3) {
                            int codepoint1 = font.get_codepoint(gidx1);
                            int codepoint2 = font.get_codepoint(gidx2);
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

