#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <assert.h>
#include <vector>
#include <unordered_set>

#include "../../include/surface.h"
#include "../../src/rdpq/rdpq_font_internal.h"

// LodePNG
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS    // No need to parse PNG extra fields
#define LODEPNG_NO_COMPILE_CPP                 // No need to use C++ API
#include "../common/lodepng.h"
#include "../common/lodepng.c"

// Rect packing
#include "rect_pack.cpp"

// Compression library
#include "../common/assetcomp.h"

#include "../common/binout.c"
#include "../common/binout.h"
#include "../common/subprocess.h"
#include "../common/utils.h"
#include "../common/polyfill.h"

int flag_verbose = 0;
bool flag_debug = false;
bool flag_kerning = true;
int flag_ttf_point_size = 0;
std::vector<int> flag_ranges;
const char *n64_inst = NULL;
int flag_ellipsis_cp = 0x002E;
int flag_ellipsis_repeats = 3;
float flag_ttf_outline = 0;
bool flag_ttf_monochrome = false;
float flag_ttf_char_spacing = 0;
tex_format_t flag_bmfont_format = FMT_RGBA16;
std::unordered_set<uint32_t> flag_charset;

typedef struct {
    const char *name;
    uint32_t first;
    uint32_t last;
} unicode_block;

unicode_block unicode_blocks[] = {
    { "Basic Latin", 0x0, 0x7F },
    { "Latin-1 Supplement", 0x80, 0xFF },
    { "Latin Extended-A", 0x100, 0x17F },
    { "Latin Extended-B", 0x180, 0x24F },
    { "IPA Extensions", 0x250, 0x2AF },
    { "Spacing Modifier Letters", 0x2B0, 0x2FF },
    { "Combining Diacritical Marks", 0x300, 0x36F },
    { "Greek and Coptic", 0x370, 0x3FF },
    { "Cyrillic", 0x400, 0x4FF },
    { "Cyrillic Supplement", 0x500, 0x52F },
    { "Armenian", 0x530, 0x58F },
    { "Hebrew", 0x590, 0x5FF },
    { "Arabic", 0x600, 0x6FF },
    { "Syriac", 0x700, 0x74F },
    { "Thaana", 0x780, 0x7BF },
    { "Devanagari", 0x900, 0x97F },
    { "Bengali", 0x980, 0x9FF },
    { "Gurmukhi", 0xA00, 0xA7F },
    { "Gujarati", 0xA80, 0xAFF },
    { "Oriya", 0xB00, 0xB7F },
    { "Tamil", 0xB80, 0xBFF },
    { "Telugu", 0xC00, 0xC7F },
    { "Kannada", 0xC80, 0xCFF },
    { "Malayalam", 0xD00, 0xD7F },
    { "Sinhala", 0xD80, 0xDFF },
    { "Thai", 0xE00, 0xE7F },
    { "Lao", 0xE80, 0xEFF },
    { "Tibetan", 0xF00, 0xFFF },
    { "Myanmar", 0x1000, 0x109F },
    { "Georgian", 0x10A0, 0x10FF },
    { "Hangul Jamo", 0x1100, 0x11FF },
    { "Ethiopic", 0x1200, 0x137F },
    { "Cherokee", 0x13A0, 0x13FF },
    { "Unified Canadian Aboriginal Syllabics", 0x1400, 0x167F },
    { "Ogham", 0x1680, 0x169F },
    { "Runic", 0x16A0, 0x16FF },
    { "Tagalog", 0x1700, 0x171F },
    { "Hanunoo", 0x1720, 0x173F },
    { "Buhid", 0x1740, 0x175F },
    { "Tagbanwa", 0x1760, 0x177F },
    { "Khmer", 0x1780, 0x17FF },
    { "Mongolian", 0x1800, 0x18AF },
    { "Limbu", 0x1900, 0x194F },
    { "Tai Le", 0x1950, 0x197F },
    { "Khmer Symbols", 0x19E0, 0x19FF },
    { "Buginese", 0x1A00, 0x1A1F },
    { "Tai Tham", 0x1A20, 0x1AAF },
    { "Combining Diacritical Marks Extended", 0x1AB0, 0x1AFF },
    { "Balinese", 0x1B00, 0x1B7F },
    { "Sundanese", 0x1B80, 0x1BBF },
    { "Batak", 0x1BC0, 0x1BFF },
    { "Lepcha", 0x1C00, 0x1C4F },
    { "Ol Chiki", 0x1C50, 0x1C7F },
    { "Vedic Extensions", 0x1CD0, 0x1CFF },
    { "Phonetic Extensions", 0x1D00, 0x1D7F },
    { "Phonetic Extensions Supplement", 0x1D80, 0x1DBF },
    { "Combining Diacritical Marks Supplement", 0x1DC0, 0x1DFF },
    { "Latin Extended Additional", 0x1E00, 0x1EFF },
    { "Greek Extended", 0x1F00, 0x1FFF },
    { "General Punctuation", 0x2000, 0x206F },
    { "Superscripts and Subscripts", 0x2070, 0x209F },
    { "Currency Symbols", 0x20A0, 0x20CF },
    { "Combining Diacritical Marks for Symbols", 0x20D0, 0x20FF },
    { "Letterlike Symbols", 0x2100, 0x214F },
    { "Number Forms", 0x2150, 0x218F },
    { "Arrows", 0x2190, 0x21FF },
    { "Mathematical Operators", 0x2200, 0x22FF },
    { "Miscellaneous Technical", 0x2300, 0x23FF },
    { "Control Pictures", 0x2400, 0x243F },
    { "Optical Character Recognition", 0x2440, 0x245F },
    { "Enclosed Alphanumerics", 0x2460, 0x24FF },
    { "Box Drawing", 0x2500, 0x257F },
    { "Block Elements", 0x2580, 0x259F },
    { "Geometric Shapes", 0x25A0, 0x25FF },
    { "Miscellaneous Symbols", 0x2600, 0x26FF },
    { "Dingbats", 0x2700, 0x27BF },
    { "Miscellaneous Mathematical Symbols-A", 0x27C0, 0x27EF },
    { "Supplemental Arrows-A", 0x27F0, 0x27FF },
    { "Braille Patterns", 0x2800, 0x28FF },
    { "Supplemental Arrows-B", 0x2900, 0x297F },
    { "Miscellaneous Mathematical Symbols-B", 0x2980, 0x29FF },
    { "Supplemental Mathematical Operators", 0x2A00, 0x2AFF },
    { "Miscellaneous Symbols and Arrows", 0x2B00, 0x2BFF },
    { "Glagolitic", 0x2C00, 0x2C5F },
    { "Latin Extended-C", 0x2C60, 0x2C7F },
    { "Coptic", 0x2C80, 0x2CFF },
    { "Georgian Supplement", 0x2D00, 0x2D2F },
    { "Tifinagh", 0x2D30, 0x2D7F },
    { "Ethiopic Extended", 0x2D80, 0x2DDF },
    { "Cyrillic Extended-A", 0x2DE0, 0x2DFF },
    { "Supplemental Punctuation", 0x2E00, 0x2E7F },
    { "CJK Radicals Supplement", 0x2E80, 0x2EFF },
    { "Kangxi Radicals", 0x2F00, 0x2FDF },
    { "Ideographic Description Characters", 0x2FF0, 0x2FFF },
    { "CJK Symbols and Punctuation", 0x3000, 0x303F },
    { "Hiragana", 0x3040, 0x309F },
    { "Katakana", 0x30A0, 0x30FF },
    { "Bopomofo", 0x3100, 0x312F },
    { "Hangul Compatibility Jamo", 0x3130, 0x318F },
    { "Kanbun", 0x3190, 0x319F },
    { "Bopomofo Extended", 0x31A0, 0x31BF },
    { "CJK Strokes", 0x31C0, 0x31EF },
    { "Katakana Phonetic Extensions", 0x31F0, 0x31FF },
    { "Enclosed CJK Letters and Months", 0x3200, 0x32FF },
    { "CJK Compatibility", 0x3300, 0x33FF },
    { "CJK Unified Ideographs Extension A", 0x3400, 0x4DBF },
    { "Yijing Hexagram Symbols", 0x4DC0, 0x4DFF },
    { "CJK Unified Ideographs", 0x4E00, 0x9FFF },
    { "Yi Syllables", 0xA000, 0xA48F },
    { "Yi Radicals", 0xA490, 0xA4CF },
    { "Lisu", 0xA4D0, 0xA4FF },
    { "Vai", 0xA500, 0xA63F },
    { "Cyrillic Extended-B", 0xA640, 0xA69F },
    { "Bamum", 0xA6A0, 0xA6FF },
    { "Modifier Tone Letters", 0xA700, 0xA71F },
    { "Latin Extended-D", 0xA720, 0xA7FF },
    { "Syloti Nagri", 0xA800, 0xA82F },
    { "Common Indic Number Forms", 0xA830, 0xA83F },
    { "Phags-pa", 0xA840, 0xA87F },
    { "Saurashtra", 0xA880, 0xA8DF },
    { "Devanagari Extended", 0xA8E0, 0xA8FF },
    { "Kayah Li", 0xA900, 0xA92F },
    { "Rejang", 0xA930, 0xA95F },
    { "Hangul Jamo Extended-A", 0xA960, 0xA97F },
    { "Javanese", 0xA980, 0xA9DF },
    { "Myanmar Extended-B", 0xA9E0, 0xA9FF },
    { "Cham", 0xAA00, 0xAA5F },
    { "Myanmar Extended-A", 0xAA60, 0xAA7F },
    { "Tai Viet", 0xAA80, 0xAADF },
    { "Meetei Mayek Extensions", 0xAAE0, 0xAAFF },
    { "Ethiopic Extended-A", 0xAB00, 0xAB2F },
    { "Latin Extended-E", 0xAB30, 0xAB6F },
    { "Cherokee Supplement", 0xAB70, 0xABBF },
    { "Meetei Mayek", 0xABC0, 0xABFF },
    { "Hangul Syllables", 0xAC00, 0xD7AF },
    { "Hangul Jamo Extended-B", 0xD7B0, 0xD7FF },
    { "High Surrogates", 0xD800, 0xDB7F },
    { "High Private Use Surrogates", 0xDB80, 0xDBFF },
    { "Low Surrogates", 0xDC00, 0xDFFF },
    { "Private Use Area", 0xE000, 0xF8FF },
    { "CJK Compatibility Ideographs", 0xF900, 0xFAFF },
    { "Alphabetic Presentation Forms", 0xFB00, 0xFB4F },
    { "Arabic Presentation Forms-A", 0xFB50, 0xFDFF },
    { "Variation Selectors", 0xFE00, 0xFE0F },
    { "Vertical Forms", 0xFE10, 0xFE1F },
    { "Combining Half Marks", 0xFE20, 0xFE2F },
    { "CJK Compatibility Forms", 0xFE30, 0xFE4F },
    { "Small Form Variants", 0xFE50, 0xFE6F },
    { "Arabic Presentation Forms-B", 0xFE70, 0xFEFF },
    { "Halfwidth and Fullwidth Forms", 0xFF00, 0xFFEF },
    { "Specials", 0xFFF0, 0xFFFF },
    { "Linear B Syllabary", 0x10000, 0x1007F },
    { "Linear B Ideograms", 0x10080, 0x100FF },
    { "Aegean Numbers", 0x10100, 0x1013F },
    { "Ancient Greek Numbers", 0x10140, 0x1018F },
    { "Ancient Symbols", 0x10190, 0x101CF },
    { "Phaistos Disc", 0x101D0, 0x101FF },
    { "Lycian", 0x10280, 0x1029F },
    { "Carian", 0x102A0, 0x102DF },
    { "Coptic Epact Numbers", 0x102E0, 0x102FF },
    { "Old Italic", 0x10300, 0x1032F },
    { "Gothic", 0x10330, 0x1034F },
    { "Old Permic", 0x10350, 0x1037F },
    { "Ugaritic", 0x10380, 0x1039F },
    { "Old Persian", 0x103A0, 0x103DF },
    { "Deseret", 0x10400, 0x1044F },
    { "Shavian", 0x10450, 0x1047F },
    { "Osmanya", 0x10480, 0x104AF },
    { "Osage", 0x104B0, 0x104FF },
    { "Elbasan", 0x10500, 0x1052F },
    { "Caucasian Albanian", 0x10530, 0x1056F },
    { "Linear A", 0x10600, 0x1077F },
    { "Cypriot Syllabary", 0x10800, 0x1083F },
    { "Imperial Aramaic", 0x10840, 0x1085F },
    { "Palmyrene", 0x10860, 0x1087F },
    { "Nabataean", 0x10880, 0x108AF },
    { "Hatran", 0x108E0, 0x108FF },
    { "Phoenician", 0x10900, 0x1091F },
    { "Lydian", 0x10920, 0x1093F },
    { "Meroitic Hieroglyphs", 0x10980, 0x1099F },
    { "Meroitic Cursive", 0x109A0, 0x109FF },
    { "Kharoshthi", 0x10A00, 0x10A5F },
    { "Old South Arabian", 0x10A60, 0x10A7F },
    { "Old North Arabian", 0x10A80, 0x10A9F },
    { "Manichaean", 0x10AC0, 0x10AFF },
    { "Avestan", 0x10B00, 0x10B3F },
    { "Inscriptional Parthian", 0x10B40, 0x10B5F },
    { "Inscriptional Pahlavi", 0x10B60, 0x10B7F },
    { "Psalter Pahlavi", 0x10B80, 0x10BAF },
    { "Old Turkic", 0x10C00, 0x10C4F },
    { "Old Hungarian", 0x10C80, 0x10CFF },
    { "Hanifi Rohingya", 0x10D00, 0x10D3F },
    { "Rumi Numeral Symbols", 0x10E60, 0x10E7F },
    { "Yezidi", 0x10E80, 0x10EBF },
    { "Old Sogdian", 0x10F00, 0x10F2F },
    { "Sogdian", 0x10F30, 0x10F6F },
    { "Chorasmian", 0x10FB0, 0x10FDF },
    { "Elymaic", 0x10FE0, 0x10FFF },
    { "Brahmi", 0x11000, 0x1107F },
    { "Kaithi", 0x11080, 0x110CF },
    { "Sora Sompeng", 0x110D0, 0x110FF },
    { "Chakma", 0x11100, 0x1114F },
    { "Mahajani", 0x11150, 0x1117F },
    { "Sharada", 0x11180, 0x111DF },
    { "Sinhala Archaic Numbers", 0x111E0, 0x111FF },
    { "Khojki", 0x11200, 0x1124F },
    { "Multani", 0x11280, 0x112AF },
    { "Khudawadi", 0x112B0, 0x112FF },
    { "Grantha", 0x11300, 0x1137F },
    { "Newa", 0x11400, 0x1147F },
    { "Tirhuta", 0x11480, 0x114DF },
    { "Siddham", 0x11580, 0x115FF },
    { "Modi", 0x11600, 0x1165F },
    { "Mongolian Supplement", 0x11660, 0x1167F },
    { "Takri", 0x11680, 0x116CF },
    { "Ahom", 0x11700, 0x1173F },
    { "Dogra", 0x11800, 0x1184F },
    { "Warang Citi", 0x118A0, 0x118FF },
    { "Dives Akuru", 0x11900, 0x1195F },
    { "Nandinagari", 0x119A0, 0x119FF },
    { "Zanabazar Square", 0x11A00, 0x11A4F },
    { "Soyombo", 0x11A50, 0x11AAF },
    { "Pau Cin Hau", 0x11AC0, 0x11AFF },
    { "Bhaiksuki", 0x11C00, 0x11C6F },
    { "Marchen", 0x11C70, 0x11CBF },
    { "Masaram Gondi", 0x11D00, 0x11D5F },
    { "Gunjala Gondi", 0x11D60, 0x11DAF },
    { "Makasar", 0x11EE0, 0x11EFF },
    { "Tamil Supplement", 0x11FC0, 0x11FFF },
    { "Cuneiform", 0x12000, 0x123FF },
    { "Cuneiform Numbers and Punctuation", 0x12400, 0x1247F },
    { "Early Dynastic Cuneiform", 0x12480, 0x1254F },
    { "Egyptian Hieroglyphs", 0x13000, 0x1342F },
    { "Anatolian Hieroglyphs", 0x14400, 0x1467F },
    { "Bamum Supplement", 0x16800, 0x16A3F },
    { "Mro", 0x16A40, 0x16A6F },
    { "Tangsa", 0x16A70, 0x16ACF },
    { "Bassa Vah", 0x16AD0, 0x16AFF },
    { "Pahawh Hmong", 0x16B00, 0x16B8F },
    { "Medefaidrin", 0x16E40, 0x16E9F },
    { "Miao", 0x16F00, 0x16F9F },
    { "Ideographic Symbols and Punctuation", 0x16FE0, 0x16FFF },
    { "Tangut", 0x17000, 0x187FF },
    { "Tangut Components", 0x18800, 0x18AFF },
    { "Kana Supplement", 0x1B000, 0x1B0FF },
    { "Kana Extended-A", 0x1B100, 0x1B12F },
    { "Small Kana Extension", 0x1B130, 0x1B16F },
    { "Nushu", 0x1B170, 0x1B2FF },
    { "Duployan", 0x1BC00, 0x1BC9F },
    { "Shorthand Format Controls", 0x1BCA0, 0x1BCAF },
    { "Byzantine Musical Symbols", 0x1D000, 0x1D0FF },
    { "Musical Symbols", 0x1D100, 0x1D1FF },
    { "Ancient Greek Musical Notation", 0x1D200, 0x1D24F },
    { "Mayan Numerals", 0x1D2E0, 0x1D2FF },
    { "Tai Xuan Jing Symbols", 0x1D300, 0x1D35F },
    { "Counting Rod Numerals", 0x1D360, 0x1D37F },
    { "Mathematical Alphanumeric Symbols", 0x1D400, 0x1D7FF },
    { "Sutton SignWriting", 0x1D800, 0x1DAAF },
    { "Glagolitic Supplement", 0x1E000, 0x1E02F },
    { "Nyiakeng Puachue Hmong", 0x1E100, 0x1E14F },
    { "Wancho", 0x1E2C0, 0x1E2FF },
    { "Mende Kikakui", 0x1E800, 0x1E8DF },
    { "Adlam", 0x1E900, 0x1E95F },
    { "Indic Siyaq Numbers", 0x1EC70, 0x1ECBF },
    { "Ottoman Siyaq Numbers", 0x1ED00, 0x1ED4F },
    { "Arabic Mathematical Alphabetic Symbols", 0x1EE00, 0x1EEFF },
    { "Mahjong Tiles", 0x1F000, 0x1F02F },
    { "Domino Tiles", 0x1F030, 0x1F09F },
    { "Playing Cards", 0x1F0A0, 0x1F0FF },
    { "Enclosed Alphanumeric Supplement", 0x1F100, 0x1F1FF },
    { "Enclosed Ideographic Supplement", 0x1F200, 0x1F2FF },
    { "Miscellaneous Symbols and Pictographs", 0x1F300, 0x1F5FF },
    { "Emoticons", 0x1F600, 0x1F64F },
    { "Ornamental Dingbats", 0x1F650, 0x1F67F },
    { "Transport and Map Symbols", 0x1F680, 0x1F6FF },
    { "Alchemical Symbols", 0x1F700, 0x1F77F },
    { "Geometric Shapes Extended", 0x1F780, 0x1F7FF },
    { "Supplemental Arrows-C", 0x1F800, 0x1F8FF },
    { "Supplemental Symbols and Pictographs", 0x1F900, 0x1F9FF },
    { "Chess Symbols", 0x1FA00, 0x1FA6F },
    { "Symbols and Pictographs Extended-A", 0x1FA70, 0x1FAFF },
    { "Symbols for Legacy Computing", 0x1FB00, 0x1FBFF },
    { "CJK Unified Ideographs Extension B", 0x20000, 0x2A6DF },
    { "CJK Unified Ideographs Extension C", 0x2A700, 0x2B73F },
    { "CJK Unified Ideographs Extension D", 0x2B740, 0x2B81F },
    { "CJK Unified Ideographs Extension E", 0x2B820, 0x2CEAF },
    { "CJK Unified Ideographs Extension F", 0x2CEB0, 0x2EBEF },
    { "CJK Compatibility Ideographs Supplement", 0x2F800, 0x2FA1F },
    { "CJK Unified Ideographs Extension G", 0x30000, 0x3134F },
    { "CJK Unified Ideographs Extension H", 0x31350, 0x323AF },
    { "Tags", 0xE0000, 0xE007F },
    { "Variation Selectors Supplement", 0xE0100, 0xE01EF },
    { "Supplementary Private Use Area-A", 0xF0000, 0xFFFFF },
    { "Supplementary Private Use Area-B", 0x100000, 0x10FFFF },
    { NULL, 0, 0 }
};

void print_args( char * name )
{
    fprintf(stderr, "mkfont -- Convert TTF/OTF/BMFont fonts into the font64 format for libdragon\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -o/--output <dir>         Specify output directory (default: .)\n");
    fprintf(stderr, "   -v/--verbose              Verbose output\n");
    fprintf(stderr, "   --no-kerning              Do not export kerning information\n");
    fprintf(stderr, "   --ellipsis <cp>,<reps>    Select glyph and repetitions to use for ellipsis (default: 2E,3) \n");
    fprintf(stderr, "   -c/--compress <level>     Compress output files (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "   -d/--debug                Dump also debug images\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "TTF/OTF specific flags:\n");
    fprintf(stderr, "   -s/--size <pt>            Point size of the font (default: whatever the font defaults to)\n");
    fprintf(stderr, "   --monochrome              Force monochrome output, with no aliasing (default: off)\n");
    fprintf(stderr, "   --outline <width>         Add outline to font, specifying its width in (fractional) pixels\n");
    fprintf(stderr, "   --char-spacing <width>    Add extra spacing between characters (default: 0)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   Glyph selection modes (choose one of the following):\n");
    fprintf(stderr, "   --charset <file>          Create a font that covers all and only the glyphs used in the\n");
    fprintf(stderr, "                             specified file (in UTF-8 format).\n");
    fprintf(stderr, "   -r/--range <start-stop>   Range of unicode codepoints to convert, as hex values (default: 20-7F)\n");
    fprintf(stderr, "                             Can be specified multiple times. Use \"--range all\" to extract all\n");
    fprintf(stderr, "                             glyphs in the font.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "BMFont specific flags:\n");
    fprintf(stderr, "   --format <format>         Specify the output texture format. Valid options are:\n");
    fprintf(stderr, "                             RGBA16, RGBA32, CI4, CI8 (default: RGBA16)\n");
    fprintf(stderr, "\n");
}

#include "mkfont_out.cpp"
#include "mkfont_ttf.cpp"
#include "mkfont_bmfont.cpp"

int main(int argc, char *argv[])
{
    char *infn = NULL, *outfn = NULL; const char *outdir = ".";
    bool error = false;
    int compression = DEFAULT_COMPRESSION;
    bool range_all = false;

    if (argc < 2) {
        print_args(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose++;
            } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
                flag_debug = true;
            } else if (!strcmp(argv[i], "--no-kerning")) {
                flag_kerning = false;
            } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--size")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &flag_ttf_point_size, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
            } else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--range")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                if (strcmp(argv[i], "all") == 0) {
                    range_all = true;
                    continue;
                }
                int r0, r1;
                char extra;
                if (sscanf(argv[i], "%x-%x%c", &r0, &r1, &extra) != 2) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                if (r0 > r1) {
                    fprintf(stderr, "invalid range: %x-%x\n", r0, r1);
                    return 1;
                }
                flag_ranges.push_back(r0);
                flag_ranges.push_back(r1);
            } else if (!strcmp(argv[i], "--charset")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                FILE *f = fopen(argv[i], "r");
                if (!f) {
                    fprintf(stderr, "cannot open charset file: %s\n", argv[i]);
                    return 1;
                }
                // Read one line at a time
                char *line = NULL; size_t len = 0;
                while (getline(&line, &len, f) != -1) {
                    const char *ptr = line;
                    while (*ptr != 0) {
                        uint32_t cp = utf8_to_codepoint(ptr, &ptr);
                        flag_charset.insert(cp);
                    }
                }
                free(line);
                fclose(f);

                fprintf(stderr, "charset of %zu glyphs loaded from file: %s\n", flag_charset.size(), argv[i]);

                // Always add the ASCII space. Sometimes people forgot to add it
                // in the charset because they assume whitespaces is implicit.
                // This is the only whitespace that needs to be present.
                flag_charset.insert(0x20);

                // Sort the charset into a vector
                std::vector<uint32_t> sorted_charset(flag_charset.begin(), flag_charset.end());
                std::sort(sorted_charset.begin(), sorted_charset.end());

                // Find the ranges
                if (!flag_ranges.empty()) {
                    fprintf(stderr, "WARNING: --charset flag overrides --range flag\n");
                    flag_ranges.clear();
                }
                
                // Iterate through the blocks
                int char_idx = 0;
                for (auto& block : unicode_blocks) {
                    // Check if the current char is within this block
                    while (char_idx < sorted_charset.size() && sorted_charset[char_idx] < block.first)
                        char_idx++;
                    if (char_idx >= sorted_charset.size())
                        break;
                    if (sorted_charset[char_idx] > block.last)
                        continue;
                    int min_ch = sorted_charset[char_idx];
                    while (char_idx < sorted_charset.size() && sorted_charset[char_idx] <= block.last)
                        char_idx++;
                    int max_ch = sorted_charset[char_idx-1];
                    flag_ranges.push_back(min_ch);
                    flag_ranges.push_back(max_ch);
                    fprintf(stderr, "  range added from charset: %s [%x-%x]\n", block.name, min_ch, max_ch);
                }
            } else if (!strcmp(argv[i], "--monochrome")) {
                flag_ttf_monochrome = true;
            } else if (!strcmp(argv[i], "--outline")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                float outline;
                char extra;
                if (sscanf(argv[i], "%f%c", &outline, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                flag_ttf_outline = outline;
            } else if (!strcmp(argv[i], "--ellipsis")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                int cp, repeats;
                char extra;
                if (sscanf(argv[i], "%x,%d%c", &cp, &repeats, &extra) != 2) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                flag_ellipsis_cp = cp;
                flag_ellipsis_repeats = repeats;
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                // Optional compression level
                if (i+1 < argc && argv[i+1][1] == 0) {
                    int level = argv[i+1][0] - '0';
                    if (level >= 0 && level <= 3) {
                        compression = level;
                        i++;
                    }
                    else {
                        fprintf(stderr, "invalid compression level: %s\n", argv[i+1]);
                        return 1;
                    }
                }
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else if (!strcmp(argv[i], "--char-spacing")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                float spacing;
                char extra;
                if (sscanf(argv[i], "%f%c", &spacing, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                flag_ttf_char_spacing = spacing;
            } else if (!strcmp(argv[i], "--format")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                if (strcmp(argv[i], "RGBA16") == 0) {
                    flag_bmfont_format = FMT_RGBA16;
                } else if (strcmp(argv[i], "RGBA32") == 0) {
                    flag_bmfont_format = FMT_RGBA32;
                } else if (strcmp(argv[i], "CI4") == 0) {
                    flag_bmfont_format = FMT_CI4;
                } else if (strcmp(argv[i], "CI8") == 0) {
                    flag_bmfont_format = FMT_CI8;
                } else {
                    fprintf(stderr, "invalid format: %s\n", argv[i]);
                    return 1;
                }
            } else {
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        infn = argv[i];
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;
        char* basename_noext = strdup(basename);
        char* ext = strrchr(basename_noext, '.');
        if (ext) *ext = '\0';

        if (range_all) {
            flag_ranges.clear();
        } else if (flag_ranges.empty()) {
            flag_ranges.push_back(0x20);
            flag_ranges.push_back(0x7F);
        }

        // Find n64 tool directory
        if (!n64_inst) {
            n64_inst = n64_tools_dir();
            if (!n64_inst) {
                fprintf(stderr, "Error: N64_INST environment variable not set\n");
                return 1;
            }
        }

        asprintf(&outfn, "%s/%s.font64", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        
        int ret;
        if (strcasestr(infn, ".ttf") || strcasestr(infn, ".otf")) {
            ret = convert_ttf(infn, outfn, flag_ranges);
        } else if (strcasestr(infn, ".fnt")) {
            ret = convert_bmfont(infn, outfn);
        } else {
            fprintf(stderr, "Error: unknown input file type: %s\n", infn);
            ret = 1;
        }

        if (ret != 0) {
            error = true;
        } else {
            if (compression) {
                struct stat st_decomp = {0}, st_comp = {0};
                stat(outfn, &st_decomp);
                asset_compress(outfn, outfn, compression, 0);
                stat(outfn, &st_comp);
                if (flag_verbose)
                    printf("compressed: %s (%d -> %d, ratio %.1f%%)\n", outfn,
                    (int)st_decomp.st_size, (int)st_comp.st_size, 100.0 * (float)st_comp.st_size / (float)(st_decomp.st_size == 0 ? 1 :st_decomp.st_size));
            } else {
                if (flag_verbose) {
                    struct stat st = {0};
                    stat(outfn, &st);
                    printf("written: %s (%d bytes)\n", outfn, (int)st.st_size);
                }
            }
        }
        free(outfn);
    }

    return error ? 1 : 0;
}
