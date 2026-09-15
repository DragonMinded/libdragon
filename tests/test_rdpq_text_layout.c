/*
 * Layout tests for rdpq_paragraph.c / rdpq_text.c (registry, printf buffers).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Builtin font load uses rdpq_font_load_buf(), which requires rdpq_init() for atlas upload. */
#define RDPQ_TEXT_FONT_CTX() \
	RDPQ_INIT(); \
	rdpq_font_t *mono = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO); \
	ASSERT(mono != NULL, "builtin mono"); \
	rdpq_text_register_font(1, mono); \
	DEFER(rdpq_text_unregister_font(1); rdpq_font_free(mono);)

static int cmp_paragraph_reading_order(const void *a, const void *b)
{
	const rdpq_paragraph_char_t *ca = a, *cb = b;
	int ya = (int)roundf((float)ca->y);
	int yb = (int)roundf((float)cb->y);
	if (ya != yb)
		return ya - yb;
	if (ca->x < cb->x)
		return -1;
	if (ca->x > cb->x)
		return 1;
	return (int)ca->glyph - (int)cb->glyph;
}

static void layout_dump_glyphs_reading(const rdpq_paragraph_t *p, char *out, size_t cap)
{
	if (p->nchars <= 0) {
		if (cap)
			out[0] = '\0';
		return;
	}
	rdpq_paragraph_char_t *tmp = alloca(sizeof(rdpq_paragraph_char_t) * (size_t)p->nchars);
	memcpy(tmp, p->chars, sizeof(rdpq_paragraph_char_t) * (size_t)p->nchars);
	qsort(tmp, (size_t)p->nchars, sizeof(rdpq_paragraph_char_t), cmp_paragraph_reading_order);

	size_t o = 0;
	for (int i = 0; i < p->nchars && o + 1 < cap; i++) {
		const rdpq_paragraph_char_t *ch = &tmp[i];
		int n = snprintf(out + o, cap - o, "g=%d,x=%d,y=%d,f=%u,s=%u|",
		    (int)ch->glyph, (int)roundf((float)ch->x), (int)roundf((float)ch->y),
		    (unsigned)ch->font_id, (unsigned)ch->style_id);
		if (n < 0 || (size_t)n >= cap - o)
			break;
		o += (size_t)n;
	}
	if (o < cap)
		out[o] = '\0';
	else if (cap)
		out[cap - 1] = '\0';
}

/** UTF-8: collect glyphs for non-whitespace (skip space/tab/newline). */
static int utf8_plain_glyph_expect(const rdpq_font_t *fnt, const char *s, int *glyphs_out, int max_g)
{
	int n = 0;
	while (*s && n < max_g) {
		uint32_t cp;
		unsigned char c = (unsigned char)*s;
		if (c < 0x80) {
			cp = c;
			s++;
		} else if ((c & 0xE0) == 0xC0 && s[1]) {
			cp = ((uint32_t)(c & 0x1Fu) << 6) | (uint32_t)(s[1] & 0x3Fu);
			s += 2;
		} else if ((c & 0xF0) == 0xE0 && s[1] && s[2]) {
			cp = ((uint32_t)(c & 0x0Fu) << 12) | ((uint32_t)(s[1] & 0x3Fu) << 6)
			    | (uint32_t)(s[2] & 0x3Fu);
			s += 3;
		} else
			return -1;

		if (cp == '\n')
			continue;
		if (cp == ' ' || cp == '\t')
			continue;
		int g = rdpq_font_get_glyph_index(fnt, cp);
		if (g < 0)
			return -1;
		glyphs_out[n++] = g;
	}
	return n;
}

static void sort_copy_reading(const rdpq_paragraph_t *p, rdpq_paragraph_char_t *tmp)
{
	memcpy(tmp, p->chars, sizeof(rdpq_paragraph_char_t) * (size_t)p->nchars);
	qsort(tmp, (size_t)p->nchars, sizeof(rdpq_paragraph_char_t), cmp_paragraph_reading_order);
}

/** First @a n glyphs in reading order must match @a full_sorted (regression: typewriter + WRAP_WORD). */
static void assert_typewriter_prefix_matches_full(TestContext *ctx,
    const rdpq_paragraph_char_t *full_sorted, const rdpq_paragraph_t *partial, int n)
{
	ASSERT_EQUAL_SIGNED(partial->nchars, n, "partial nchars");
	rdpq_paragraph_char_t *ps = alloca(sizeof(rdpq_paragraph_char_t) * (size_t)n);
	sort_copy_reading(partial, ps);
	for (int i = 0; i < n; i++) {
		ASSERT_EQUAL_SIGNED((int)ps[i].glyph, (int)full_sorted[i].glyph, "glyph");
		ASSERT_EQUAL_SIGNED((int)roundf((float)ps[i].x), (int)roundf((float)full_sorted[i].x), "x");
		ASSERT_EQUAL_SIGNED((int)roundf((float)ps[i].y), (int)roundf((float)full_sorted[i].y), "y");
		ASSERT_EQUAL_UNSIGNED(ps[i].font_id, full_sorted[i].font_id, "font");
		ASSERT_EQUAL_UNSIGNED(ps[i].style_id, full_sorted[i].style_id, "style");
	}
}

static void typewriter_max_chars_progression_case(TestContext *ctx, const char *text,
    rdpq_textparms_t parms_base, int min_total, int min_lines)
{
	int nbytes = (int)strlen(text);
	rdpq_textparms_t parms_full = parms_base;
	parms_full.max_chars = 0;
	int nb_full = nbytes;
	rdpq_paragraph_t *full = rdpq_paragraph_build(&parms_full, 1, text, &nb_full);
	if (!full) {
		ASSERT(full, "full build");
		return;
	}
	int ncref = full->nchars, nlref = full->nlines;
	if (ncref < min_total || nlref < min_lines) {
		rdpq_paragraph_free(full);
		ASSERT(ncref >= min_total && nlref >= min_lines, "reference layout");
		return;
	}

	int total = full->nchars;
	rdpq_paragraph_char_t *full_sorted = alloca(sizeof(rdpq_paragraph_char_t) * (size_t)total);
	sort_copy_reading(full, full_sorted);

	for (int n = 1; n <= total; n++) {
		rdpq_textparms_t parms = parms_base;
		parms.max_chars = n;
		int nb = nbytes;
		rdpq_paragraph_t *p = rdpq_paragraph_build(&parms, 1, text, &nb);
		if (!p) {
			rdpq_paragraph_free(full);
			ASSERT(p, "partial build");
			return;
		}
		assert_typewriter_prefix_matches_full(ctx, full_sorted, p, n);
		rdpq_paragraph_free(p);
		if (ctx->result == TEST_FAILED) {
			rdpq_paragraph_free(full);
			return;
		}
	}
	rdpq_paragraph_free(full);
}

static void assert_layout_plain_glyphs(TestContext *ctx, const rdpq_paragraph_t *p,
    const rdpq_font_t *fnt, uint8_t expect_font, uint8_t expect_style, const char *plain_utf8)
{
	int exp[256];
	int ne = utf8_plain_glyph_expect(fnt, plain_utf8, exp, 256);
	ASSERT(ne >= 0, "glyph expect");
	ASSERT_EQUAL_SIGNED(p->nchars, ne, "nchars");
	if (ne <= 0)
		return;
	rdpq_paragraph_char_t *tmp = alloca(sizeof(rdpq_paragraph_char_t) * (size_t)ne);
	sort_copy_reading(p, tmp);
	for (int i = 0; i < ne; i++) {
		ASSERT_EQUAL_SIGNED((int)tmp[i].glyph, exp[i], "glyph id");
		ASSERT_EQUAL_UNSIGNED(tmp[i].font_id, expect_font, "font id");
		ASSERT_EQUAL_UNSIGNED(tmp[i].style_id, expect_style, "style id");
	}
}

/* Empty paragraph is SKIP: builder touches chars[-1] when nchars==0 (UB). */
void test_rdpq_text_metrics_empty(TestContext *ctx)
{
	SKIP("empty UTF-8: rdpq_paragraph_builder_end indexes chars[-1] when nchars==0; "
	     "cannot layout zero glyphs safely");
}

/* Simple ASCII: nbytes, single line, expected glyphs, stable reading-order dump. */
void test_rdpq_text_metrics_simple_ascii(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { 0 };
	const char *text = "abc";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(nbytes, (int)strlen(text), "nbytes");
	ASSERT_EQUAL_SIGNED(layout->nlines, 1, "nlines");
	assert_layout_plain_glyphs(ctx, layout, mono, 1, 0, text);

	char d1[512], d2[512];
	layout_dump_glyphs_reading(layout, d1, sizeof d1);
	layout_dump_glyphs_reading(layout, d2, sizeof d2);
	ASSERT_EQUAL_STR(d1, d2, "dump stable");
}

/* Newlines in input: glyph count, multiple visual lines, vertical order of a vs b. */
void test_rdpq_text_newline_only(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { 0 };
	const char *text = "a\n\nb";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(nbytes, (int)strlen(text), "nbytes");
	ASSERT_EQUAL_SIGNED(layout->nchars, 2, "two glyphs");
	ASSERT_EQUAL_SIGNED(layout->nlines, 3, "three visual lines");
	assert_layout_plain_glyphs(ctx, layout, mono, 1, 0, "ab");

	rdpq_paragraph_char_t tmp[4];
	sort_copy_reading(layout, tmp);
	ASSERT(tmp[1].y > tmp[0].y, "b baseline below a");
}

/* $0N escape: switch font (glyphs from IDs 1 and 2 in "a$02b"). */
void test_rdpq_text_escape_dollar_font(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();
	rdpq_font_t *var = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
	ASSERT(var != NULL, "var");
	rdpq_text_register_font(2, var);
	DEFER(rdpq_text_unregister_font(2); rdpq_font_free(var););

	rdpq_textparms_t parms = { 0 };
	const char *text = "a$02b";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 2, "nchars");
	rdpq_paragraph_char_t tmp[8];
	sort_copy_reading(layout, tmp);
	int ga = rdpq_font_get_glyph_index(mono, 'a');
	int gb = rdpq_font_get_glyph_index(var, 'b');
	ASSERT_EQUAL_SIGNED(tmp[0].glyph, ga, "a glyph");
	ASSERT_EQUAL_UNSIGNED(tmp[0].font_id, 1u, "font1");
	ASSERT_EQUAL_SIGNED(tmp[1].glyph, gb, "b glyph");
	ASSERT_EQUAL_UNSIGNED(tmp[1].font_id, 2u, "font2");
}

/* ^0N escape: adjacent glyphs get different style ids. */
void test_rdpq_text_escape_caret_style(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { 0 };
	const char *text = "a^01b";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 2, "nchars");
	rdpq_paragraph_char_t tmp[8];
	sort_copy_reading(layout, tmp);
	ASSERT_EQUAL_UNSIGNED(tmp[0].style_id, 0u, "a style");
	ASSERT_EQUAL_UNSIGNED(tmp[1].style_id, 1u, "b style");
}

/* $$ and ^^ emit literal $ and ^ in the layout. */
void test_rdpq_text_escape_literal_dollar_caret(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { 0 };
	const char *text = "a$$b^^c";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	assert_layout_plain_glyphs(ctx, layout, mono, 1, 0, "a$b^c");
}

/* indent: first line starts farther right on X than the second line. */
void test_rdpq_text_indent_first_line(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .indent = 17 };
	const char *text = "a\nb";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 2, "nchars");
	rdpq_paragraph_char_t tmp[4];
	sort_copy_reading(layout, tmp);
	ASSERT(tmp[0].y < tmp[1].y, "lines");
	ASSERT(tmp[0].x > tmp[1].x, "first line indented farther right");
}

/* WRAP_CHAR: break at fixed width; multiple lines; same glyphs as full ASCII text. */
void test_rdpq_text_wrap_char_basic(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 40, .wrap = WRAP_CHAR };
	const char *text = "abcdefghijklmnopqrst";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT(layout->nlines >= 2, "wrapped to multiple lines");
	assert_layout_plain_glyphs(ctx, layout, mono, 1, 0, text);
}

/* WRAP_WORD: break at spaces; words concatenated in plain-glyph expectations (no spaces). */
void test_rdpq_text_wrap_word_basic(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 56, .wrap = WRAP_WORD };
	const char *text = "abc def ghi";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT(layout->nlines >= 2, "word wrap");
	assert_layout_plain_glyphs(ctx, layout, mono, 1, 0, "abcdefghi");
}

/* Long token without spaces: WRAP_WORD truncates or ellipsizes (fewer glyphs than input). */
void test_rdpq_text_wrap_word_long_token(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 32, .wrap = WRAP_WORD };
	const char *text = "abcdefghijklmno";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT(layout->nchars < (int)strlen(text), "truncated long token");
	ASSERT(layout->nchars >= 1, "some glyph");
}

/* WRAP_NONE: no wrap/reflow; text is truncated when it exceeds width. */
void test_rdpq_text_wrap_none_truncate(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 24, .wrap = WRAP_NONE };
	const char *text = "abcdefghijklmnopqrst";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT(layout->nchars < (int)strlen(text), "truncate");
}

/* WRAP_ELLIPSES: ellipsis glyphs appear when text does not fit the width. */
void test_rdpq_text_wrap_ellipses(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 48, .wrap = WRAP_ELLIPSES };
	const char *text = "abcdefghijklmnopqrstuvwxyz";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	int ell_g = rdpq_font_get_glyph_index(mono, '.');
	ASSERT(ell_g >= 0, "ellipsis glyph");
	int n_ell = 0;
	for (int i = 0; i < layout->nchars; i++) {
		if (layout->chars[i].glyph == ell_g)
			n_ell++;
	}
	ASSERT(n_ell >= 1, "ellipsis glyphs present");
}

/* char_spacing increases horizontal gap between two letters vs default zero. */
void test_rdpq_text_char_spacing(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms0 = { 0 };
	rdpq_textparms_t parms1 = { .char_spacing = 4 };
	const char *text = "ab";
	int nb0 = (int)strlen(text);
	int nb1 = (int)strlen(text);
	rdpq_paragraph_t *l0 = rdpq_paragraph_build(&parms0, 1, text, &nb0);
	rdpq_paragraph_t *l1 = rdpq_paragraph_build(&parms1, 1, text, &nb1);
	ASSERT(l0 && l1, "build");
	DEFER(rdpq_paragraph_free(l0);
	      rdpq_paragraph_free(l1););

	rdpq_paragraph_char_t t0[4], t1[4];
	sort_copy_reading(l0, t0);
	sort_copy_reading(l1, t1);
	ASSERT(t1[1].x > t0[1].x, "char_spacing widens second glyph");
}

/* line_spacing increases delta-Y between two lines separated by newline. */
void test_rdpq_text_line_spacing(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms0 = { 0 };
	rdpq_textparms_t parms1 = { .line_spacing = 6 };
	const char *text = "a\nb";
	int nb0 = (int)strlen(text);
	int nb1 = (int)strlen(text);
	rdpq_paragraph_t *l0 = rdpq_paragraph_build(&parms0, 1, text, &nb0);
	rdpq_paragraph_t *l1 = rdpq_paragraph_build(&parms1, 1, text, &nb1);
	ASSERT(l0 && l1, "build");
	DEFER(rdpq_paragraph_free(l0);
	      rdpq_paragraph_free(l1););

	rdpq_paragraph_char_t t0[4], t1[4];
	sort_copy_reading(l0, t0);
	sort_copy_reading(l1, t1);
	ASSERT(t1[1].y > t0[1].y, "line_spacing increases delta-Y");
}

/* Default tab stops: X advance after \\t is clearly larger than for adjacent chars without tab. */
void test_rdpq_text_tab_default_32(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { 0 };
	const char *text = "a\tb";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 2, "nchars");
	rdpq_paragraph_char_t tmp[4];
	sort_copy_reading(layout, tmp);
	ASSERT(tmp[1].x > tmp[0].x + 4, "tab advances x");
}

/* Custom tabstops: second glyph X matches the chosen stop column. */
void test_rdpq_text_tab_custom_stops(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	int16_t stops[] = { 50, 100, 0 };
	rdpq_textparms_t parms = { .tabstops = stops };
	const char *text = "a\tb";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	rdpq_paragraph_char_t tmp[4];
	sort_copy_reading(layout, tmp);
	ASSERT(tmp[1].x > tmp[0].x + 20, "custom tabstop");
}

/* max_chars (typewriter): stops at next space after emitting up to the glyph budget. */
void test_rdpq_text_max_chars_stop_at_space(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .max_chars = 2 };
	const char *text = "ab cd ef";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 2, "two glyphs");
	ASSERT_EQUAL_SIGNED(nbytes, 2, "2 chars displayed");
}

void test_rdpq_text_nbytes_wrap_word_plaintext(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 40, .height = 40, .wrap = WRAP_WORD };
	const char *text = "abcdef abcdef 123456 123456";

	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, (int)strlen(text), "first word doesn't fit -> ellipsis: full line consumed");

	nbytes = (int)strlen(text);
	parms.width = 45;
	layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, 21, "consume spaces after wrapped words");

	nbytes = (int)strlen(text);
	parms.width = 85;
	layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););
	ASSERT_EQUAL_SIGNED(nbytes, (int)strlen(text), "full string fits in available lines");
}

void test_rdpq_text_nbytes_wrap_word_with_escapes(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_font_style(mono, 1, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF),
    });
	rdpq_font_style(mono, 2, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0x00, 0x00, 0xFF),
    });

	rdpq_textparms_t parms = { .width = 40, .height = 40, .wrap = WRAP_WORD };
	const char *text = "^01abcdef ^02abcdef ^01123456 ^02123456";

	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, (int)strlen(text), "first word doesn't fit -> ellipsis: full line consumed");

	nbytes = (int)strlen(text);
	parms.width = 45;
	layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, 30, "consume escapes and spaces consistently");

	nbytes = (int)strlen(text);
	parms.width = 85;
	layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););
	ASSERT_EQUAL_SIGNED(nbytes, (int)strlen(text), "full string fits in available lines");
}

void test_rdpq_text_nbytes_wrap_char_with_escapes(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_font_style(mono, 1, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF),
    });
	rdpq_font_style(mono, 2, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0x00, 0x00, 0xFF),
    });

	rdpq_textparms_t parms = { .width = 25, .height = 40, .wrap = WRAP_CHAR };
	const char *text = "^01abcdef ^02abcdef ^01123456 ^02123456";

	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, 16, "3 lines * 3 chars/line, escapes counted");

	nbytes = (int)strlen(text);
	parms.width = 30;
	layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, 18, "3 lines * 4 chars/line");

	nbytes = (int)strlen(text);
	parms.width = 35;
	layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););
	ASSERT_EQUAL_SIGNED(nbytes, 24, "3 lines * 5 chars/line");
}

void test_rdpq_text_nbytes_ellipsis(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_font_style(mono, 1, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF),
    });
	rdpq_font_style(mono, 2, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0x00, 0x00, 0xFF),
    });

	rdpq_textparms_t parms = { .width = 65, .height = 30, .wrap = WRAP_WORD };
	const char *text = "^01abcdef\n^02abcdefabcdef\n^01123456 ^02123456";

	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, 26, "ellipsis consumes to end of overflowing line");

	nbytes = (int)strlen(text);
	parms.height = 40;
	layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););
	ASSERT_EQUAL_SIGNED(nbytes, 36, "next line starts after consumed ellipsis line");
}

/* WRAP_WORD: when stopping on a full page, consume trailing newline delimiter too. */
void test_rdpq_text_nbytes_wrap_word_consumes_newline(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 40, .height = 20, .wrap = WRAP_WORD };
	const char *text = "abcdef\nghijkl";
	int nbytes = (int)strlen(text);

	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(nbytes, 7, "first page consumes first word plus newline");
}

/* WRAP_WORD + consecutive escapes: nbytes must stop before ^xx^yy when page is full. */
void test_rdpq_text_nbytes_wrap_word_consecutive_escapes(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_font_style(mono, 1, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF),
    });
	rdpq_font_style(mono, 2, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFF, 0x00, 0x00, 0xFF),
    });

	rdpq_textparms_t parms = { .width = 20, .height = 20, .wrap = WRAP_WORD };
	const char *text = "a ^01^02bbbbbbbb c";
	const int escape_off = (int)(strstr(text, "^01^02") - text);

	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes, escape_off, "full page must not consume consecutive escapes");
	ASSERT(text[nbytes] == '^', "next page starts at first escape");

	rdpq_textparms_t parms2 = parms;
	parms2.height = 120;
	int nbytes2 = (int)strlen(text) - nbytes;
	layout = rdpq_paragraph_build(&parms2, 1, text + nbytes, &nbytes2);
	ASSERT(layout != NULL, "build page 2");
	rdpq_paragraph_free(layout);
	ASSERT_EQUAL_SIGNED(nbytes2, (int)strlen(text) - nbytes, "page 2 consumes all remaining bytes");
}

/* max_chars on a word without spaces: final nchars clamped to max_chars. */
void test_rdpq_text_max_chars_clamp_end(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .max_chars = 3 };
	const char *text = "aaaa";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 3, "clamped to max_chars");
}

/* max_chars + WRAP_WORD across lines: typewriter does not break word wrap (#739). */
void test_rdpq_text_max_chars_with_word_wrap(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 56, .wrap = WRAP_WORD, .max_chars = 6 };
	const char *text = "abc def ghi";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 6, "typewriter stops after six letters (abc+def)");
	/* First six glyphs may sit on one line even when full "abc def ghi" wraps with no max_chars. */
	assert_layout_plain_glyphs(ctx, layout, mono, 1, 0, "abcdef");
}

/* For each max_chars, first N glyphs match unlimited layout (WORD wrap regression #739). */
void test_rdpq_text_typewriter(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	/* Full layout may truncate the long m-run (few total glyphs); min_lines still from short+long word. */
	typewriter_max_chars_progression_case(ctx, "aa mmmmmmmmmmmm",
	    (rdpq_textparms_t){ .width = 20, .wrap = WRAP_WORD }, 2, 2);
	if (ctx->result == TEST_FAILED)
		return;

	typewriter_max_chars_progression_case(ctx, "abc def ghi",
	    (rdpq_textparms_t){ .width = 56, .wrap = WRAP_WORD }, 9, 2);
}

/* WRAP_ELLIPSES at very small width: ellipsis present (font/line fix dbcbb3634). */
void test_rdpq_text_wrap_ellipses_narrow_width(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 10, .wrap = WRAP_ELLIPSES };
	const char *text = "abcdefghijklmnopqrstuvwxyz";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	int ell_g = rdpq_font_get_glyph_index(mono, '.');
	ASSERT(ell_g >= 0, "ellipsis glyph");
	int n_ell = 0;
	for (int i = 0; i < layout->nchars; i++) {
		if (layout->chars[i].glyph == ell_g)
			n_ell++;
	}
	ASSERT(n_ell >= 1, "ellipsis at tight width");
	ASSERT(layout->nlines >= 1, "nlines");
}

/* Short word then long token: only xx on first line; wrap does not pull from previous line (#0c4e38885). */
void test_rdpq_text_wrap_word_short_then_long_token(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .width = 20, .wrap = WRAP_WORD };
	char text[40];
	strcpy(text, "xx ");
	for (int i = 0; i < 14; i++)
		strcat(text, "m");
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT(layout->nlines >= 2, "short word then long token wraps to next line");
	ASSERT(layout->nchars >= 3, "xx plus continuation");

	rdpq_paragraph_char_t tmp[32];
	sort_copy_reading(layout, tmp);
	int gx = rdpq_font_get_glyph_index(mono, 'x');
	ASSERT_EQUAL_SIGNED((int)tmp[0].glyph, gx, "first line x");
	ASSERT_EQUAL_SIGNED((int)tmp[1].glyph, gx, "first line x");
	float y0 = tmp[0].y;
	ASSERT(fabsf(tmp[1].y - y0) < 1.f, "both x on same row");
	int n_first_line = 0;
	for (int i = 0; i < layout->nchars && n_first_line < 32; i++) {
		if (fabsf(tmp[i].y - y0) < 1.f)
			n_first_line++;
	}
	ASSERT_EQUAL_SIGNED(n_first_line, 2, "only xx on first line; long m-token wraps below");
}

/* Max height: vertical layout is shortened (WRAP_CHAR and newline in input). */
void test_rdpq_text_height_truncates_vertical(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .height = 18, .wrap = WRAP_CHAR, .width = 24 };
	const char *text = "abcdefghij\nklmnop";
	int full_len = (int)strlen(text);
	int nbytes = full_len;
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT(nbytes <= full_len, "nbytes sane");
	ASSERT(nbytes < full_len || layout->nchars < 12, "vertical limit shortens layout");
}

/* valign VALIGN_CENTER vs TOP: y0/bbox/advance shift with fixed paragraph height. */
void test_rdpq_text_valign_center_height(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms_top = { .height = 80, .valign = VALIGN_TOP };
	rdpq_textparms_t parms_ctr = { .height = 80, .valign = VALIGN_CENTER };
	const char *text = "ab";
	int nb0 = (int)strlen(text);
	int nb1 = (int)strlen(text);
	rdpq_paragraph_t *l0 = rdpq_paragraph_build(&parms_top, 1, text, &nb0);
	rdpq_paragraph_t *l1 = rdpq_paragraph_build(&parms_ctr, 1, text, &nb1);
	ASSERT(l0 && l1, "build");
	DEFER(rdpq_paragraph_free(l0);
	      rdpq_paragraph_free(l1););

	ASSERT(l1->y0 != l0->y0 || l1->bbox.y0 != l0->bbox.y0, "valign shifts layout");
	ASSERT(l1->advance_y != l0->advance_y || l1->y0 != l0->y0, "advance/y0 valign");
}

/* ALIGN_CENTER vs LEFT: line shifts right within fixed width. */
void test_rdpq_text_align_center_width(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms_l = { .width = 200, .align = ALIGN_LEFT };
	rdpq_textparms_t parms_c = { .width = 200, .align = ALIGN_CENTER };
	const char *text = "ab";
	int nb0 = (int)strlen(text);
	int nb1 = (int)strlen(text);
	rdpq_paragraph_t *l0 = rdpq_paragraph_build(&parms_l, 1, text, &nb0);
	rdpq_paragraph_t *l1 = rdpq_paragraph_build(&parms_c, 1, text, &nb1);
	ASSERT(l0 && l1, "build");
	DEFER(rdpq_paragraph_free(l0);
	      rdpq_paragraph_free(l1););

	rdpq_paragraph_char_t t0[4], t1[4];
	sort_copy_reading(l0, t0);
	sort_copy_reading(l1, t1);
	ASSERT(t1[0].x > t0[0].x, "center moves line right");
}

/* ALIGN_RIGHT vs LEFT: line shifts farther right within fixed width. */
void test_rdpq_text_align_right_width(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms_l = { .width = 200, .align = ALIGN_LEFT };
	rdpq_textparms_t parms_r = { .width = 200, .align = ALIGN_RIGHT };
	const char *text = "ab";
	int nb0 = (int)strlen(text);
	int nb1 = (int)strlen(text);
	rdpq_paragraph_t *l0 = rdpq_paragraph_build(&parms_l, 1, text, &nb0);
	rdpq_paragraph_t *l1 = rdpq_paragraph_build(&parms_r, 1, text, &nb1);
	ASSERT(l0 && l1, "build");
	DEFER(rdpq_paragraph_free(l0);
	      rdpq_paragraph_free(l1););

	rdpq_paragraph_char_t t0[4], t1[4];
	sort_copy_reading(l0, t0);
	sort_copy_reading(l1, t1);
	ASSERT(t1[0].x > t0[0].x, "right moves line farther right");
}

/* Multi-byte UTF-8 (e.g. pound): one glyph if present in builtin mono font. */
void test_rdpq_text_utf8_multibyte_in_range(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	const char *text = "\xc2\xa3";
	if (rdpq_font_get_glyph_index(mono, 0xA3) < 0) {
		SKIP("pound sign not in builtin mono");
		return;
	}
	rdpq_textparms_t parms = { 0 };
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 1, "one glyph");
}

/* Invalid UTF-8 sequence: build still succeeds (graceful skip of bad bytes). */
void test_rdpq_text_utf8_invalid_sequence(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	const char *text = "\xff\xfe";
	rdpq_textparms_t parms = { 0 };
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););
}

/* preserve_overlap: same reading-order dump as without; chars[] may differ after RDP sort. */
void test_rdpq_text_preserve_overlap_order(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();
	rdpq_font_t *var = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
	ASSERT(var != NULL, "var");
	rdpq_text_register_font(2, var);
	DEFER(rdpq_text_unregister_font(2); rdpq_font_free(var););

	/* Styles 0,2,1 so sort_key order (0,1,2) differs from insert order (0,2,1). */
	const char *text = "a^02b^01c";
	int nb0 = (int)strlen(text);
	int nb1 = (int)strlen(text);
	rdpq_textparms_t po = { .preserve_overlap = true };
	rdpq_textparms_t np = { .preserve_overlap = false };
	rdpq_paragraph_t *l0 = rdpq_paragraph_build(&po, 1, text, &nb0);
	rdpq_paragraph_t *l1 = rdpq_paragraph_build(&np, 1, text, &nb1);
	ASSERT(l0 && l1, "build");
	DEFER(rdpq_paragraph_free(l0);
	      rdpq_paragraph_free(l1););

	char d0[256], d1[256];
	layout_dump_glyphs_reading(l0, d0, sizeof d0);
	layout_dump_glyphs_reading(l1, d1, sizeof d1);
	ASSERT_EQUAL_STR(d0, d1, "reading-order dump same");
	if (l0->nchars > 1)
		ASSERT(memcmp(l0->chars, l1->chars, sizeof(rdpq_paragraph_char_t) * (size_t)l0->nchars) != 0,
		    "chars[] order differs when RDP sort runs");
}

/* disable_aa_fix: layout clears RDPQ_PARAGRAPH_FLAG_ANTIALIAS_FIX. */
void test_rdpq_text_disable_aa_fix_flag(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { .disable_aa_fix = true };
	const char *text = "a";
	int nbytes = 1;
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT((layout->flags & RDPQ_PARAGRAPH_FLAG_ANTIALIAS_FIX) == 0, "AA fix flag off");
}

/* Incremental builder (span/newline) matches rdpq_paragraph_build reading-order dump. */
void test_rdpq_text_builder_api_matches_build(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	rdpq_textparms_t parms = { 0 };
	const char *text = "a\nb";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *from_build = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(from_build != NULL, "build");
	DEFER(rdpq_paragraph_free(from_build););

	rdpq_paragraph_builder_begin(&parms, 1, NULL);
	rdpq_paragraph_builder_span("a", 1);
	rdpq_paragraph_builder_newline();
	rdpq_paragraph_builder_span("b", 1);
	rdpq_paragraph_t *from_manual = rdpq_paragraph_builder_end();
	ASSERT(from_manual != NULL, "manual");
	DEFER(rdpq_paragraph_free(from_manual););

	char d0[512], d1[512];
	layout_dump_glyphs_reading(from_build, d0, sizeof d0);
	layout_dump_glyphs_reading(from_manual, d1, sizeof d1);
	ASSERT_EQUAL_STR(d0, d1, "builder vs build");
}

/* Builtin proportional font: AV pair shows kerning (second glyph farther right). */
void test_rdpq_text_kerning_var_font(TestContext *ctx)
{
	RDPQ_INIT();
	rdpq_font_t *var = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
	ASSERT(var != NULL, "var");
	rdpq_text_register_font(1, var);
	DEFER(rdpq_text_unregister_font(1);
	      rdpq_font_free(var););

	rdpq_textparms_t parms = { 0 };
	const char *text = "AV";
	int nbytes = (int)strlen(text);
	rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, 1, text, &nbytes);
	ASSERT(layout != NULL, "build");
	DEFER(rdpq_paragraph_free(layout););

	ASSERT_EQUAL_SIGNED(layout->nchars, 2, "nchars");
	rdpq_paragraph_char_t tmp[4];
	sort_copy_reading(layout, tmp);
	ASSERT(tmp[1].x > tmp[0].x, "second glyph right of first");
}

/* Font registry: register at id 7, get, unregister, then get returns NULL. */
void test_rdpq_text_register_get_unregister(TestContext *ctx)
{
	RDPQ_INIT();
	rdpq_font_t *mono = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
	ASSERT(mono != NULL, "mono");
	rdpq_text_register_font(7, mono);
	ASSERT(rdpq_text_get_font(7) == mono, "get registered");
	rdpq_text_unregister_font(7);
	ASSERT(rdpq_text_get_font(7) == NULL, "gone after unregister");
	rdpq_font_free(mono);
}

/* Short formatted string: layout matches expanded literal text. */
void test_rdpq_text_printf_small_buf(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	char fmt[32];
	snprintf(fmt, sizeof fmt, "n=%d", 42);
	rdpq_textparms_t parms = { 0 };
	int nb = (int)strlen(fmt);
	rdpq_paragraph_t *l = rdpq_paragraph_build(&parms, 1, fmt, &nb);
	ASSERT(l != NULL, "layout from formatted string");
	DEFER(rdpq_paragraph_free(l););

	ASSERT_EQUAL_SIGNED(l->nchars, 4, "n=42 -> four non-space glyphs");
}

/* rdpq_text_printf with >512-byte format (heap vasnprintf) and many short lines. */
void test_rdpq_text_printf_large_buf(TestContext *ctx)
{
	RDPQ_TEXT_FONT_CTX();

	const int W = 64;
	surface_t fb = surface_alloc(FMT_RGBA32, W, W);
	DEFER(surface_free(&fb));
	memset(fb.buffer, 0, fb.stride * fb.height);

	rdpq_attach_clear(&fb, NULL);

	/* >512 bytes for vasnprintf heap path; bounded line length and line count (x/y < 2048). */
	char big[800];
	int p = 0;
	for (int line = 0; line < 36 && p + 17 < (int)sizeof big; line++) {
		for (int i = 0; i < 15; i++)
			big[p++] = 'a';
		big[p++] = '\n';
	}
	big[p] = '\0';
	ASSERT((int)strlen(big) > 512, "large format string");

	/* main() will check for memory leaks */
	rdpq_text_printf(NULL, 1, 4.f, 20.f, "%s", big);
	rdpq_detach_wait();
}