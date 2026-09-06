/**
 * The performance overlay of the in stream view, drawn with nuklear.
 *
 * A card at the top of the window with one line of live numbers of the stream, and a small chevron pill
 * for when the user folds it away. Every renderer that draws the overlay with nuklear includes this file
 * and the copies of it have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/perf_overlay.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/perf_overlay.h
 *
 * The x of the card and the pill are hit tested on the java side, so DesktopUiController and
 * VulkanRendererBackend of PXPlay mirror the metrics below and have to follow a change made here. The
 * card is drawn straight onto the canvas in framebuffer pixels, not through the layout space, so what
 * these rects say is where the overlay ends up and what java can hit test against.
 *
 * The width is fixed and never follows the text, which keeps the card from breathing while the numbers
 * change and lets java know where the x is without measuring a string. It is cut to the window only when
 * that is narrower than the card. It holds the longest line the app can produce, a 2160p HDR stream with
 * upscaling and the longest decoder name, which takes 839 of the 907 px left between the padding and the x.
 *
 * On the size: the card is drawn in framebuffer pixels while the JavaFX one is laid out in logical points,
 * so on a display running at 200% the JavaFX card came out twice the size of this one and the line was
 * barely readable. The line is therefore set in the 26 px font of the dialog body instead of the 16 px one
 * the button captions use, the largest step up the atlas already holds, and every metric grew by that same
 * factor. Nothing else about the design changed and no further font is baked. The card ends up as wide as
 * the dialog card, which is the size the rest of the native UI is drawn at.
 *
 * nuklear.h has to be included before this file.
 */
#ifndef PXPLAY_PERF_OVERLAY_H
#define PXPLAY_PERF_OVERLAY_H

/* the step from the caption font to the body one, which every metric below grew by */
#define NK_PERF_SCALE (26.0f / 16.0f)

/* the card, in framebuffer pixels, rounded where an edge or a hairline has to land on a whole one */
const float perfCardWidth = 1008.0f;     /* 620 * NK_PERF_SCALE, to an even pixel so centring is exact */
const float perfCardFloorWidth = 390.0f; /* a window narrower than the card still gets one */
const float perfCardSideMargin = 26.0f;  /* room kept left and right of it on such a window */
const float perfCardTopMargin = 26.0f;
const float perfCardHeight = 72.0f;
const float perfCardRadius = 20.0f;
const float perfCardBorder = 1.0f * NK_PERF_SCALE;
const float perfCardPaddingLeft = 26.0f;
const float perfCardPaddingRight = 10.0f;
const float perfCardTextGap = 13.0f;     /* between the line and the x */

/* the x that folds the card away */
const float perfCloseSize = 52.0f;
const float perfCloseRadius = 13.0f;
const float perfCloseGlyphSize = 20.0f;
const float perfCloseGlyphStroke = 1.6f * NK_PERF_SCALE;

/* and the pill that brings it back */
const float perfArrowWidth = 92.0f;
const float perfArrowHeight = 40.0f;
const float perfArrowRadius = 20.0f;
const float perfArrowGlyphWidth = 23.0f;
const float perfArrowGlyphHeight = 13.0f;
const float perfArrowGlyphStroke = 2.0f * NK_PERF_SCALE;

/* how far down from the top edge the cursor brings the pill back, the band the overlay lives in */
const float perfHoverStripHeight = perfCardTopMargin + perfCardHeight + perfCardTopMargin;

/* the drop shadow of the app, as the rings nk_dialog_draw uses instead of a blur */
const int perfShadowLayers = 3;
const float perfShadowGrow = 3.0f * NK_PERF_SCALE;
const float perfShadowDrop = 1.5f * NK_PERF_SCALE;

const struct nk_color perf_shadow_color = nk_rgba(0, 0, 0, 22);
const struct nk_color perf_card_color = nk_rgba(14, 16, 22, 199);
const struct nk_color perf_card_border_color = nk_rgba(255, 255, 255, 41);
const struct nk_color perf_card_text_color = nk_rgba(255, 255, 255, 240);
const struct nk_color perf_card_glyph_color = nk_rgba(255, 255, 255, 224);
const struct nk_color perf_button_pressed_color = nk_rgba(255, 255, 255, 61);

/** The width of the card, the fixed one unless the window is too narrow to hold it. */
static float nk_perf_card_width(float viewportWidth)
{
    float room = viewportWidth - (2.0f * perfCardSideMargin);

    if (room < perfCardFloorWidth) room = perfCardFloorWidth;
    return perfCardWidth < room ? perfCardWidth : room;
}

/** Where the card sits, on whole pixels so its hairline border stays a hairline. */
static struct nk_rect nk_perf_card_rect(float viewportWidth)
{
    const float width = nk_perf_card_width(viewportWidth);
    return nk_rect((float) (int) ((viewportWidth - width) * 0.5f), perfCardTopMargin, width, perfCardHeight);
}

/** The x inside the card. */
static struct nk_rect nk_perf_close_rect(struct nk_rect card)
{
    return nk_rect((card.x + card.w) - (perfCardPaddingRight + perfCloseSize),
                   card.y + ((card.h - perfCloseSize) * 0.5f),
                   perfCloseSize, perfCloseSize);
}

/** The pill of the folded away card, in the same place the card had. */
static struct nk_rect nk_perf_arrow_rect(float viewportWidth)
{
    return nk_rect((float) (int) ((viewportWidth - perfArrowWidth) * 0.5f), perfCardTopMargin,
                   perfArrowWidth, perfArrowHeight);
}

/**
 * How many bytes of the line fit into the given width, whole glyphs only.
 *
 * The card is built to hold the longest line the app produces, so the whole line fitting is the case
 * that matters and it costs one measurement. Only a window too narrow for the card walks the glyphs.
 */
static int nk_perf_clamp_text(const struct nk_user_font* font, const char* text, int length, float space)
{
    int fits = 0;
    int at = 0;

    if (font->width(font->userdata, font->height, text, length) <= space) return length;

    while (at < length) {
        nk_rune unicode = 0;
        int glyphLength = nk_utf_decode(&text[at], &unicode, length - at);
        if (glyphLength < 1) glyphLength = 1;
        if (font->width(font->userdata, font->height, text, at + glyphLength) > space) break;
        at += glyphLength;
        fits = at;
    }
    return fits;
}

/** The two arms of the x, centred in the button it belongs to. */
static void nk_perf_draw_close_glyph(struct nk_command_buffer* out, struct nk_rect button)
{
    const float inset = (perfCloseSize - perfCloseGlyphSize) * 0.5f;
    const float left = button.x + inset;
    const float top = button.y + inset;
    const float right = left + perfCloseGlyphSize;
    const float bottom = top + perfCloseGlyphSize;

    nk_stroke_line(out, left, top, right, bottom, perfCloseGlyphStroke, perf_card_glyph_color);
    nk_stroke_line(out, right, top, left, bottom, perfCloseGlyphStroke, perf_card_glyph_color);
}

/** The rings under the card and the pill, which is what a blurred drop shadow costs here. */
static void nk_perf_draw_shadow(struct nk_command_buffer* out, struct nk_rect bounds, float radius)
{
    int layer;

    for (layer = perfShadowLayers; layer > 0; --layer) {
        const float grow = perfShadowGrow * (float) layer;
        const struct nk_rect ring = nk_rect(bounds.x - grow, (bounds.y - grow) + perfShadowDrop,
                                            bounds.w + (2.0f * grow), bounds.h + (2.0f * grow));
        nk_fill_rect(out, ring, radius + grow, perf_shadow_color);
    }
}

/** The chevron of the pill, pointing down at the card it stands for. */
static void nk_perf_draw_arrow_glyph(struct nk_command_buffer* out, struct nk_rect pill)
{
    const float left = pill.x + ((pill.w - perfArrowGlyphWidth) * 0.5f);
    const float top = pill.y + ((pill.h - perfArrowGlyphHeight) * 0.5f);
    float points[6];

    points[0] = left;
    points[1] = top;
    points[2] = left + (perfArrowGlyphWidth * 0.5f);
    points[3] = top + perfArrowGlyphHeight;
    points[4] = left + perfArrowGlyphWidth;
    points[5] = top;

    nk_stroke_polyline(out, points, 3, perfArrowGlyphStroke, perf_card_glyph_color);
}

/**
 * Draws the overlay into the canvas: the card with its line and its x, or the pill it collapses to.
 *
 * The text is the line the app has built, already formatted and in the latin range the atlas bakes, and
 * it is cut instead of wrapped when a narrow window leaves no room for all of it.
 */
static void nk_draw_perf_overlay(struct nk_command_buffer* out, const struct nk_user_font* font,
                                 float viewportWidth, float viewportHeight, const char* text,
                                 nk_bool collapsed, nk_bool closePressed, nk_bool arrowPressed)
{
    struct nk_rect previousClip;
    struct nk_rect card;
    struct nk_rect close;

    if (!out || viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;

    /* the window keeps a padding of its own, which the shadow of the card has to cross */
    previousClip = out->clip;
    nk_push_scissor(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight));

    if (collapsed) {
        const struct nk_rect pill = nk_perf_arrow_rect(viewportWidth);

        nk_perf_draw_shadow(out, pill, perfArrowRadius);
        nk_fill_rect(out, pill, perfArrowRadius, perf_card_color);
        if (arrowPressed) {
            nk_fill_rect(out, pill, perfArrowRadius, perf_button_pressed_color);
        }
        nk_stroke_rect(out, pill, perfArrowRadius, perfCardBorder, perf_card_border_color);
        nk_perf_draw_arrow_glyph(out, pill);

        nk_push_scissor(out, previousClip);
        return;
    }

    card = nk_perf_card_rect(viewportWidth);
    nk_perf_draw_shadow(out, card, perfCardRadius);
    nk_fill_rect(out, card, perfCardRadius, perf_card_color);
    nk_stroke_rect(out, card, perfCardRadius, perfCardBorder, perf_card_border_color);

    close = nk_perf_close_rect(card);
    if (closePressed) {
        nk_fill_rect(out, close, perfCloseRadius, perf_button_pressed_color);
    }
    nk_perf_draw_close_glyph(out, close);

    if (font && text && text[0] != '\0') {
        const float left = card.x + perfCardPaddingLeft;
        const float space = (close.x - perfCardTextGap) - left;

        if (space > 0.0f) {
            const int length = nk_strlen(text);
            const int fits = nk_perf_clamp_text(font, text, length, space);

            if (fits > 0) {
                const float width = font->width(font->userdata, font->height, text, fits);
                const struct nk_rect line = nk_rect(left + ((space - width) * 0.5f),
                                                    card.y + ((card.h - font->height) * 0.5f),
                                                    width, font->height);
                nk_draw_text(out, line, text, fits, font, nk_rgba(0, 0, 0, 0), perf_card_text_color);
            }
        }
    }

    nk_push_scissor(out, previousClip);
}

#endif /* PXPLAY_PERF_OVERLAY_H */
