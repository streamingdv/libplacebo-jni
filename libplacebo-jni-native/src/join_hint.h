/**
 * The join hint of the in stream view, drawn with nuklear.
 *
 * A pill at the top of the window naming the button a player presses to ask the console to let them into
 * the session. The app shows it for a few seconds at a time and owns both the sentence and when it
 * appears; this file only says what it looks like and where it lands. It carries no button and takes no
 * click, so unlike the performance overlay nothing about it is hit tested and the java side does not
 * mirror these metrics. Every renderer that draws it with nuklear includes this file and the copies of it
 * have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/join_hint.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/join_hint.h
 *
 * It borrows the card of the performance overlay, the same shadow and radius, so that the two read as one
 * family on the occasions they share the top of the window. Where that card is a fixed width because its
 * numbers change underneath it, this one follows its text: a whole sentence that changes only with the
 * language, and one a fixed pill would leave either half empty or too narrow for the longest translation
 * of it.
 *
 * It is drawn one step larger than the rest of the overlay, in the bold font of the dialog title rather
 * than the body one the card next to it uses, and a shade more solid behind it. The card of numbers is a
 * fixture of the window that the player reads when they go looking for it, while this comes up for a few
 * seconds to say something they have not asked about and may well miss: a player holding a controller the
 * game is ignoring is exactly the person who is not reading the top of the window. The caller picks the
 * font, so everything here follows whatever it is handed.
 *
 * nuklear.h, bidi_text.h and perf_overlay.h have to be included before this file.
 */
#ifndef PXPLAY_JOIN_HINT_H
#define PXPLAY_JOIN_HINT_H

/* the pill, in framebuffer pixels, a step taller than the card so the larger font sits in it as easily */
const float joinHintHeight = 88.0f;
const float joinHintRadius = perfCardRadius;
const float joinHintBorder = perfCardBorder;
const float joinHintSideMargin = perfCardSideMargin;
const float joinHintTopMargin = perfCardTopMargin;

/* wider than the padding of the card, which only has room to spare because its x sits in it */
const float joinHintPaddingX = 46.0f;

/* between the pill and the overlay above it, the gap the card keeps between its line and its x */
const float joinHintOverlayGap = perfCardTextGap;

/* a shade more solid than the card, for the reason the top of this file gives */
const struct nk_color join_hint_color = nk_rgba(14, 16, 22, 226);
const struct nk_color join_hint_border_color = nk_rgba(255, 255, 255, 66);

/**
 * How far down the pill starts: below the performance overlay for a session that shows one, as both are
 * centred on the top edge and would otherwise be drawn over each other, and in its place for one that
 * does not. The folded away overlay is the shorter pill, so the hint moves up with it.
 */
static float nk_join_hint_top(nk_bool perfOverlayVisible, nk_bool perfOverlayCollapsed)
{
    if (!perfOverlayVisible) return joinHintTopMargin;

    return joinHintTopMargin + (perfOverlayCollapsed ? perfArrowHeight : perfCardHeight) + joinHintOverlayGap;
}

/** Where the pill sits, as wide as its sentence needs and cut to the window when that is narrower. */
static struct nk_rect nk_join_hint_rect(const struct nk_user_font* font, const char* text, int length,
                                        float viewportWidth, float top)
{
    const float room = viewportWidth - (2.0f * joinHintSideMargin);
    float width = font->width(font->userdata, font->height, text, length) + (2.0f * joinHintPaddingX);

    if (width > room) width = room;
    return nk_rect((float) (int) ((viewportWidth - width) * 0.5f), top, width, joinHintHeight);
}

/**
 * Draws the hint into the canvas, the sentence the app has built and already translated.
 *
 * A language that reads right to left is put in the order it reads first, the way the dialog does it,
 * and a sentence too long for the window is cut rather than wrapped: the pill is one line by design, and
 * a hint the player cannot read the end of still names the button they are looking for.
 */
static void nk_draw_join_hint(struct nk_command_buffer* out, const struct nk_user_font* font,
                              float viewportWidth, float viewportHeight, const char* text,
                              nk_bool perfOverlayVisible, nk_bool perfOverlayCollapsed)
{
    char visual[NK_BIDI_MAX_BYTES];
    struct nk_rect previousClip;
    struct nk_rect pill;
    const char* line;
    float space;
    int length;
    int fits;

    if (!out || !font || !text || text[0] == '\0') return;
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;

    /* a hebrew sentence is drawn in the order it reads, see bidi_text.h */
    line = nk_bidi_visual(text, nk_strlen(text), visual, (int) sizeof(visual), &length);
    if (length < 1) return;

    pill = nk_join_hint_rect(font, line, length, viewportWidth,
                             nk_join_hint_top(perfOverlayVisible, perfOverlayCollapsed));
    space = pill.w - (2.0f * joinHintPaddingX);

    /* a window with no room left below the overlay is left as it is rather than drawn over */
    if (space <= 0.0f || (pill.y + pill.h) > viewportHeight) return;

    fits = nk_perf_clamp_text(font, line, length, space);
    if (fits < 1) return;

    /* the window keeps a padding of its own, which the shadow of the pill has to cross */
    previousClip = out->clip;
    nk_push_scissor(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight));

    nk_perf_draw_shadow(out, pill, joinHintRadius);
    nk_fill_rect(out, pill, joinHintRadius, join_hint_color);
    nk_stroke_rect(out, pill, joinHintRadius, joinHintBorder, join_hint_border_color);

    {
        const float width = font->width(font->userdata, font->height, line, fits);
        const struct nk_rect bounds = nk_rect(pill.x + ((pill.w - width) * 0.5f),
                                              pill.y + ((pill.h - font->height) * 0.5f),
                                              width, font->height);

        nk_draw_text(out, bounds, line, fits, font, nk_rgba(0, 0, 0, 0), perf_card_text_color);
    }

    nk_push_scissor(out, previousClip);
}

#endif /* PXPLAY_JOIN_HINT_H */
