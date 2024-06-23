#include <libdragon.h>
#include <stdarg.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

typedef struct {
    const char *name;
    uint32_t first;
    uint32_t last;
    bool partial;
} font_block_t;

font_block_t* font_create_block_list(rdpq_font_t *font)
{
    int block_capacity = 4;
    int block_count = 0;
    font_block_t *blocks = calloc(block_capacity, sizeof(font_block_t));

    int idx = 0;
    uint32_t start, end;
    while (rdpq_font_get_glyph_ranges(font, idx++, &start, &end)) {        
        // Compact range by avoiding empty glyphs. Somtimes font define just spaces
        // in various variants (eg: CJK space) and don't define other chars there.
        // So we want to avoid mentioning those empty blocks.
        rdpq_font_gmetrics_t m;
        do {
            rdpq_font_get_glyph_metrics(font, start, &m);
        } while (m.x1-m.x0 == 0 && ++start <= end);
        if (start > end) continue;
    
        // Check which block(s) this range intersect to and add them to the block list
        for (int i = 0; unicode_blocks[i].name; i++) {
            if (start <= unicode_blocks[i].last && end >= unicode_blocks[i].first) {
                // Avoid adding a block if it was already the last one
                if (block_count > 0 && blocks[block_count-1].name == unicode_blocks[i].name) {
                    blocks[block_count-1].first = MIN(start, blocks[block_count-1].first);
                    blocks[block_count-1].last = MAX(end, blocks[block_count-1].last);
                    continue;
                }

                // Resize the block list if needed
                if (block_count == block_capacity) {
                    block_capacity *= 2;
                    blocks = realloc(blocks, block_capacity * sizeof(font_block_t));
                }
                // Check if the range is partial or full
                if (start <= unicode_blocks[i].first && end >= unicode_blocks[i].last) {
                    blocks[block_count].partial = false;
                } else {
                    blocks[block_count].partial = true;
                }
                blocks[block_count].name = unicode_blocks[i].name;
                blocks[block_count].first = MAX(start, unicode_blocks[i].first);
                blocks[block_count].last = MIN(end, unicode_blocks[i].last);
                block_count++;
            }
            // Since the block list is sorted, we can break early if the range is already past the block
            if (end < unicode_blocks[i].first) break;
        }
    }

    assert(block_count > 0);

    if (block_count == block_capacity) {
        block_capacity++;
        blocks = realloc(blocks, block_capacity * sizeof(font_block_t));
    }
    blocks[block_count].name = NULL;
    return blocks;
}

typedef struct {
    char *name;
    char *author;
    char *license;
    rdpq_font_t *font;
    font_block_t *block_list;
    uint8_t font_id;
} font_info_t;

static int sort_font_info(const void *a, const void *b)
{
    return strcasecmp(((font_info_t*)a)->name, ((font_info_t*)b)->name);
}

font_info_t *font_db;
int font_db_size;

static void load_font_database(void)
{
    dir_t dir = {0};
    
    if (dir_findfirst("rom:/", &dir) == 0) do {
        char full_fn[256];
        strcpy(full_fn, "rom:/");
        strcat(full_fn, dir.d_name);

        // if (!strstr(full_fn, "monogram.font64")) continue;

        char *ext = strstr(full_fn, ".font64");
        if (!ext) continue;
        rdpq_font_t *fnt = rdpq_font_load(full_fn);

        rdpq_font_style(fnt, 0, &(rdpq_fontstyle_t){
            .color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF),
            .outline_color = RGBA32(0x40, 0x40, 0x40, 0xFF),
        });
        rdpq_font_style(fnt, 1, &(rdpq_fontstyle_t){
            .color = RGBA32(0x8F, 0xC9, 0x3A, 0xFF),
            .outline_color = RGBA32(0x82, 0x73, 0x5C, 0xFF),
        });

        // Modify the extension into txt
        if (ext) {
            *ext = '\0';
            strcat(full_fn, ".txt");
        }

        font_info_t fi = { .font = fnt, .font_id = font_db_size+1 };
        rdpq_text_register_font(fi.font_id, fnt);

        // Parse the txt file as a sequence of "key: value" fields
        FILE *f = fopen(full_fn, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                char *key = strtok(line, ":");
                char *val = strtok(NULL, "\n");
                if (key && val) {
                    val = strspn(val, " \t") + val;
                    if (strcmp(key, "name") == 0) {
                        fi.name = strdup(val);
                    } else if (strcmp(key, "author") == 0) {
                        fi.author = strdup(val);
                    } else if (strcmp(key, "license") == 0) {
                        fi.license = strdup(val);
                    }
                }
            }
            fclose(f);
        }
        if (!fi.name) {
            char *ext = strstr(dir.d_name, ".font64");
            *ext = '\0';
            fi.name = strdup(dir.d_name);
        }

        // Create a list of Unicode blocks covered by the font
        fi.block_list = font_create_block_list(fnt);

        // Add the font to the database
        font_db = realloc(font_db, (font_db_size+1) * sizeof(font_info_t));
        font_db[font_db_size++] = fi;

    } while (dir_findnext("rom:/", &dir) == 0);

    qsort(font_db, font_db_size, sizeof(font_info_t), sort_font_info);
}

int main()
{
    debug_init_isviewer();
    debug_init_usblog();
    joypad_init();

    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    // rdpq_debug_start();

    sprite_t *star = sprite_load("rom:/star1.i8.sprite");
    int custom_text_len;
    char *custom_text = asset_load("rom:/customtext.txt", &custom_text_len);

    load_font_database();

    color_t COLOR_BKG_DARK = RGBA32(0x21,0x21,0x21,0xFF);
    color_t COLOR_BKG_LIGHT = RGBA32(0xA9,0xAF,0xD1,0xFF);
    color_t MENU_BKG = RGBA32(0x17,0x43,0x4E,0xFF);
    color_t MENU_END = RGBA32(0x5C,0x07,0x44,0xFF);
    const int MENU_WIDTH = 90;
    const int MENU_END_WIDTH = 12;
    const int MENU_FONT_SPACE = 20;
    int color_mode = 0;

    int cur_font_index = 0;
    float star_angle = 0.0f;

    float menu_xstart = 0, menu_xstart_target = 0;
    int menu_xstarti = 0;
    float menu_ystart = 120.0f;

    float ystart = 15;
    enum {
        PAGE_META,
        PAGE_TEXT,
        PAGE_ALLGLYPHS,

        NUM_PAGES,
    };
    int curpage = PAGE_META;

    while (1) {
        joypad_poll();
        joypad_buttons_t keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        if (keys.d_up) { cur_font_index = (cur_font_index + font_db_size - 1) % font_db_size; }
        if (keys.d_down) { cur_font_index = (cur_font_index + 1) % font_db_size; }
        if (keys.d_left) { curpage = (curpage + 1) % NUM_PAGES; }
        if (keys.d_right) { curpage = (curpage + NUM_PAGES - 1) % NUM_PAGES; }
        if (keys.z) { color_mode += 1; color_mode %= 4; }
        if (keys.c_left) { menu_xstart_target = -MENU_WIDTH-MENU_END_WIDTH+4; }
        if (keys.c_right) { menu_xstart_target = 0; }
        
        keys = joypad_get_buttons_held(JOYPAD_PORT_1);
        if (keys.c_up) { ystart += 2; }
        if (keys.c_down) { ystart -= 2; }

        int menu_ystart_target = 120 - cur_font_index*MENU_FONT_SPACE;
        if (menu_ystart != menu_ystart_target)
            menu_ystart = menu_ystart * 0.9 + menu_ystart_target * 0.1;
        if (menu_xstart != menu_xstart_target)
            menu_xstart = menu_xstart * 0.9 + menu_xstart_target * 0.1;
        menu_xstarti = (int)menu_xstart;

        star_angle += 0.1f;

        surface_t *disp = display_get();
        rdpq_attach(disp, NULL);

        rdpq_clear(color_mode & 1 ? COLOR_BKG_LIGHT : COLOR_BKG_DARK);

        rdpq_set_mode_fill(MENU_BKG);
        rdpq_fill_rectangle(menu_xstarti, 0, menu_xstarti+MENU_WIDTH, 240);
        for (int i=0; i<4; i++) {
            color_t c;
            c.r = MENU_BKG.r * (8-i) / 8;
            c.g = MENU_BKG.g * (8-i) / 8;
            c.b = MENU_BKG.b * (8-i) / 8;
            c.a = 0xFF;
            rdpq_set_fill_color(c);
            rdpq_fill_rectangle(menu_xstarti+MENU_WIDTH+i, 0, menu_xstarti+MENU_WIDTH+i+1, 240);
        }

        rdpq_set_fill_color(MENU_END);
        rdpq_fill_rectangle(menu_xstarti+MENU_WIDTH+4, 0, menu_xstarti+MENU_WIDTH+MENU_END_WIDTH, 240);
        for (int i=0; i<font_db_size; i++) {
            rdpq_text_print(&(rdpq_textparms_t){
                .width = MENU_WIDTH-20,
            }, font_db[i].font_id, menu_xstarti+15, (int)menu_ystart + i*MENU_FONT_SPACE, font_db[i].name);

            if (i == cur_font_index) {
                rdpq_set_mode_standard();
                rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
                rdpq_sprite_blit(star, menu_xstarti+10, (int)menu_ystart + i*MENU_FONT_SPACE - 5, &(rdpq_blitparms_t){
                    .scale_x = 0.15, .scale_y = 0.15, 
                    .cx = star->width/2, .cy = star->height/2,
                    .theta = star_angle,
                });
            }
        }

        font_info_t *fi = &font_db[cur_font_index];
        int x0 = menu_xstarti + MENU_WIDTH+MENU_END_WIDTH + 10;
        int y0 = ystart;
        rdpq_textparms_t tparms = {
            .width = 320-10-x0, .style_id = color_mode>>1,
            .tabstops = (int16_t[]){ 80, 120, 160, 200, 240, 280, 0 },
        };

        void print(int wrap_mode, const char *text, ...) {
            tparms.wrap = wrap_mode;
            va_list args;
            va_start(args, text);
            y0 += rdpq_text_vprintf(&tparms, fi->font_id, x0, y0, text, args).advance_y;
            va_end(args);
        }

        switch (curpage) {
        case PAGE_META: {
            print(WRAP_ELLIPSES, "Name:\t%s\n", fi->name);
            if (fi->author)  print(WRAP_ELLIPSES, "Author:\t%s\n", fi->author);
            if (fi->license) print(WRAP_ELLIPSES, "License:\t%s\n", fi->license);

            font_block_t *block = fi->block_list;
            print(WRAP_ELLIPSES, "Ranges:\t%s%s\n", block->name, block->partial ? "*" : "");
            ++block;
            while (block->name != NULL) {
                print(WRAP_ELLIPSES, "\t%s%s\n", block->name, block->partial ? "*" : "");
                ++block;
            }
            print(WRAP_CHAR, "\nabcdefghijklmnopqrstuvwxyz\n");
            print(WRAP_CHAR, "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");
            print(WRAP_CHAR, "0123456789\n\n");
            print(WRAP_WORD, "The quick brown fox jumps over the lazy dog.\n");
        } break;

        case PAGE_ALLGLYPHS: {
            static char buffer[4096];
            font_block_t *block = fi->block_list;
            while (block->name != NULL) {
                int bufidx = 0;

                // Put all the glyphs in the range in a buffer, in UTF-8 format
                for (uint32_t c = block->first; c <= block->last; c++) {
                    if (c < 0x80) {
                        if (c == '\n' || c == '\t' || c == '\r' || c == ' ') continue;
                        buffer[bufidx++] = c;
                        if (c == '^' || c == '$') buffer[bufidx++] = c;
                    } else if (c < 0x800) {
                        buffer[bufidx++] = 0xC0 | (c >> 6);
                        buffer[bufidx++] = 0x80 | (c & 0x3F);
                    } else if (c < 0x10000) {
                        buffer[bufidx++] = 0xE0 | (c >> 12);
                        buffer[bufidx++] = 0x80 | ((c >> 6) & 0x3F);
                        buffer[bufidx++] = 0x80 | (c & 0x3F);
                    } else {
                        buffer[bufidx++] = 0xF0 | (c >> 18);
                        buffer[bufidx++] = 0x80 | ((c >> 12) & 0x3F);
                        buffer[bufidx++] = 0x80 | ((c >> 6) & 0x3F);
                        buffer[bufidx++] = 0x80 | (c & 0x3F);
                    }
                }

                print(WRAP_WORD, "%s%s (U+%04X - U+%04X)\n", block->name, block->partial ? "*" : "", block->first, block->last);

                tparms.wrap = WRAP_CHAR;
                y0 += rdpq_text_printn(&tparms, fi->font_id, x0, y0, buffer, bufidx).advance_y;

                y0 += 20;
                ++block;
            }            
        }   break;

        case PAGE_TEXT: {
            rdpq_text_printn(&(rdpq_textparms_t){
                .width = 320-10-x0,
                .style_id = color_mode>>1,
                .wrap = WRAP_ELLIPSES,
            }, fi->font_id, x0, y0, custom_text, custom_text_len);

        } break;
        }

        rdpq_detach_show();
    }
}
