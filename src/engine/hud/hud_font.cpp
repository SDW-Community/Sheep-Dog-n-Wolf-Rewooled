/*
 * The HUD font (hud_font.h). DejaVu Sans Bold (embedded as g_sdwHudFont by CMakeLists.txt) is rasterised the first time
 * a level asks for its page, with ImGui's copy of stb_truetype, into a 1024 x 1024 atlas kept for the rest of the run;
 * each level's PolyBatcher turns it into a texture page (PolyBatcher::LoadTexturePages, g_hudFontPage). Strings are
 * UTF-8; a character not rasterised draws as '?'. To add a symbol, type it into s_symbols.
 *
 * Uses nothing of the old engine but hud_draw (the screen).
 */
#ifdef SDW_HAVE_HUD_FONT
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "imstb_truetype.h" /* ImGui's stb_truetype (the build adds ImGui's folder to the include path) */
#include "sdw_hud_font.h"   /* g_sdwHudFont[] (generated in the build folder) */
#endif
#include <string.h>

#include "sdw_types.h"
#include "hud_draw.h"
#include "hud_font.h"

#define HUDFONT_ATLAS 1024   /* the atlas, square */
#define HUDFONT_PX 48.0f      /* the rasterised line height: sharp up to about 4x the 240-line HUD */

/* the characters rasterised: printable ASCII and Latin-1 (' ' to '~', no-break space to 'ÿ'), and these symbols */
static const char s_symbols[] = "←↑→↓↵⇧⌫␣●";

#define HUDFONT_ASCII_FIRST 0x20  /* ' ' */
#define HUDFONT_ASCII_COUNT 0x5f  /* to '~' */
#define HUDFONT_LATIN1_FIRST 0xa0 /* no-break space */
#define HUDFONT_LATIN1_COUNT 0x60 /* to 'ÿ' */
#define HUDFONT_MAX_SYMBOLS 64
#define HUDFONT_MAX_GLYPHS (HUDFONT_ASCII_COUNT + HUDFONT_LATIN1_COUNT + HUDFONT_MAX_SYMBOLS)

s32 g_hudFontPage = -1;

#ifdef SDW_HAVE_HUD_FONT
static u32 HudFont_NextCodePoint(const char **s);

/* the glyphs: ASCII, then Latin-1, then s_symbols in its order */
static stbtt_packedchar s_glyphs[HUDFONT_MAX_GLYPHS];
static int s_symbolCodes[HUDFONT_MAX_SYMBOLS]; /* s_symbols decoded */
static int s_symbolCount;
static u8 *s_coverage;    /* the atlas, one byte a pixel; 0 until rasterised */
static float s_ascent, s_descent; /* in atlas pixels, at HUDFONT_PX (descent negative) */

static u8 HudFont_Rasterise()
{
    stbtt_pack_context pack;
    stbtt_pack_range ranges[3];
    stbtt_fontinfo info;
    const char *sym = s_symbols;
    int ascent, descent, gap;
    float scale;
    if (s_coverage)
        return 1;
    for (s_symbolCount = 0; *sym && s_symbolCount < HUDFONT_MAX_SYMBOLS; s_symbolCount++)
        s_symbolCodes[s_symbolCount] = (int)HudFont_NextCodePoint(&sym);
    s_coverage = new u8[HUDFONT_ATLAS * HUDFONT_ATLAS];
    if (!stbtt_PackBegin(&pack, s_coverage, HUDFONT_ATLAS, HUDFONT_ATLAS, 0, 1, 0)) {
        delete[] s_coverage;
        s_coverage = 0;
        return 0;
    }
    stbtt_PackSetOversampling(&pack, 1, 1);
    memset(ranges, 0, sizeof(ranges));
    ranges[0].first_unicode_codepoint_in_range = HUDFONT_ASCII_FIRST;
    ranges[0].num_chars = HUDFONT_ASCII_COUNT;
    ranges[0].chardata_for_range = s_glyphs;
    ranges[1].first_unicode_codepoint_in_range = HUDFONT_LATIN1_FIRST;
    ranges[1].num_chars = HUDFONT_LATIN1_COUNT;
    ranges[1].chardata_for_range = s_glyphs + HUDFONT_ASCII_COUNT;
    ranges[2].array_of_unicode_codepoints = s_symbolCodes;
    ranges[2].num_chars = s_symbolCount;
    ranges[2].chardata_for_range = s_glyphs + HUDFONT_ASCII_COUNT + HUDFONT_LATIN1_COUNT;
    ranges[0].font_size = ranges[1].font_size = ranges[2].font_size = HUDFONT_PX;
    stbtt_PackFontRanges(&pack, g_sdwHudFont, 0, ranges, 3);
    stbtt_PackEnd(&pack);
    stbtt_InitFont(&info, g_sdwHudFont, stbtt_GetFontOffsetForIndex(g_sdwHudFont, 0));
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    scale = stbtt_ScaleForPixelHeight(&info, HUDFONT_PX);
    s_ascent = ascent * scale;
    s_descent = descent * scale;
    return 1;
}

/* the index in s_glyphs of a code point; '?' when it was not rasterised */
static u32 HudFont_GlyphIndex(u32 cp)
{
    int k;
    if (cp >= HUDFONT_ASCII_FIRST && cp < HUDFONT_ASCII_FIRST + HUDFONT_ASCII_COUNT)
        return cp - HUDFONT_ASCII_FIRST;
    if (cp >= HUDFONT_LATIN1_FIRST && cp < HUDFONT_LATIN1_FIRST + HUDFONT_LATIN1_COUNT)
        return HUDFONT_ASCII_COUNT + cp - HUDFONT_LATIN1_FIRST;
    for (k = 0; k < s_symbolCount; k++)
        if ((u32)s_symbolCodes[k] == cp)
            return HUDFONT_ASCII_COUNT + HUDFONT_LATIN1_COUNT + k;
    return '?' - HUDFONT_ASCII_FIRST;
}

/* the next code point of a UTF-8 string, advancing it; malformed bytes are read as Latin-1 */
static u32 HudFont_NextCodePoint(const char **s)
{
    const u8 *p = (const u8 *)*s;
    u32 cp = *p++;
    if (cp >= 0xc0 && cp < 0xe0 && (p[0] & 0xc0) == 0x80) {
        cp = (cp & 0x1f) << 6 | (p[0] & 0x3f);
        p += 1;
    } else if (cp >= 0xe0 && cp < 0xf0 && (p[0] & 0xc0) == 0x80 && (p[1] & 0xc0) == 0x80) {
        cp = (cp & 0x0f) << 12 | (p[0] & 0x3f) << 6 | (p[1] & 0x3f);
        p += 2;
    }
    *s = (const char *)p;
    return cp;
}
#endif

u8 *HudFont_BuildPage(u32 *width, u32 *height)
{
#ifdef SDW_HAVE_HUD_FONT
    u8 *rgba;
    u32 i;
    if (!HudFont_Rasterise())
        return 0;
    rgba = new u8[HUDFONT_ATLAS * HUDFONT_ATLAS * 4];
    for (i = 0; i < HUDFONT_ATLAS * HUDFONT_ATLAS; i++) { /* white; the coverage is the opacity */
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 0xff;
        rgba[i * 4 + 3] = s_coverage[i];
    }
    *width = *height = HUDFONT_ATLAS;
    return rgba;
#else
    (void)width;
    (void)height;
    return 0;
#endif
}

float HudFont_Measure(const char *utf8, float size)
{
#ifdef SDW_HAVE_HUD_FONT
    float w = 0.0f;
    if (!HudFont_Rasterise())
        return 0.0f;
    while (*utf8)
        w += s_glyphs[HudFont_GlyphIndex(HudFont_NextCodePoint(&utf8))].xadvance;
    return w * size / HUDFONT_PX;
#else
    (void)utf8;
    (void)size;
    return 0.0f;
#endif
}

u8 HudFont_Draw(const char *utf8, float x, float y, float size, u32 rgb, u32 *layer, u8 align)
{
#ifdef SDW_HAVE_HUD_FONT
    float k = size / HUDFONT_PX; /* HUD units per atlas pixel */
    float penX = 0.0f, penY = 0.0f, left, baseline;
    stbtt_aligned_quad q;
    HudRect rect, uv;
    if (g_hudFontPage < 0 || !HudFont_Rasterise())
        return 0;
    left = align == HUDFONT_CENTRE  ? x - HudFont_Measure(utf8, size) / 2
           : align == HUDFONT_RIGHT ? x - HudFont_Measure(utf8, size)
                                    : x;
    baseline = y + (s_ascent + s_descent) / 2 * k; /* the glyphs' box centred on y */
    while (*utf8) {
        stbtt_GetPackedQuad(s_glyphs, HUDFONT_ATLAS, HUDFONT_ATLAS, (int)HudFont_GlyphIndex(HudFont_NextCodePoint(&utf8)),
                            &penX, &penY, &q, 0);
        rect.x0 = left + q.x0 * k;
        rect.x1 = left + q.x1 * k;
        rect.y0 = baseline + q.y0 * k;
        rect.y1 = baseline + q.y1 * k;
        uv.x0 = q.s0;
        uv.y0 = q.t0;
        uv.x1 = q.s1;
        uv.y1 = q.t1;
        if (q.x1 > q.x0) /* not a space */
            HudDraw_TexturedRect(g_hudFontPage, &rect, &uv, rgb, layer);
    }
    return 1;
#else
    (void)utf8, (void)x, (void)y, (void)size, (void)rgb, (void)layer, (void)align;
    return 0;
#endif
}
