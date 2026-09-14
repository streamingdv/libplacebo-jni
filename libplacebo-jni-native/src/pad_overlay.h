/**
 * The controller overlay of the in stream view, drawn with nuklear.
 *
 * A narrow card in the top left of the window with one row per console player, showing which seat a
 * controller holds, what it is called, whether its player has been let into the session, and whether it is
 * still plugged in. The app decides when the card is worth the room and hands over the seating already
 * settled; this file only says what it looks like and where it lands. It carries no button and takes no
 * click, so nothing about it is hit tested and the java side does not mirror these metrics. Every renderer
 * that draws it with nuklear includes this file and the copies of it have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/pad_overlay.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/pad_overlay.h
 *
 * It borrows the card of the performance overlay, the same fill, border, shadow, radius and body font, so
 * that everything the stream draws over itself reads as one family. It sits in the top left corner, below
 * the band along the top of the window the app puts its messages in and in the same place whether or not
 * one is up, for the reason nk_pad_overlay_top gives.
 *
 * A row names its seat with four dots rather than a numeral, the way a console does on the controller
 * itself. That dot is green once the console has let the player in and white while they are still the one
 * holding it up, and the seat of a controller that has been unplugged stays where it is with its ink
 * turned down and a stroke through its icon. Nothing is renumbered while the stream runs, because the
 * console is still sending player three's input to pad three whether or not player two is still holding a
 * controller.
 *
 * The card comes and goes with the button panel along the bottom of the window rather than standing over
 * the game for the whole session. Both of them answer the same question, of who and what the stream is
 * being driven by, and a player who wants to know reaches for the panel anyway.
 *
 * nuklear.h, bidi_text.h, perf_overlay.h and join_hint.h have to be included before this file.
 */
#ifndef PXPLAY_PAD_OVERLAY_H
#define PXPLAY_PAD_OVERLAY_H

/* the console's own limit, and therefore the number of dots a row carries */
#define NK_PAD_OVERLAY_MAX_PADS 4

/* the card, in framebuffer pixels, in the metrics the performance overlay card is drawn at */
const float padOverlayRadius = perfCardRadius;
const float padOverlayBorder = perfCardBorder;
const float padOverlaySideMargin = perfCardSideMargin;
const float padOverlayPaddingX = 22.0f;
const float padOverlayPaddingY = 20.0f;

/* between the card and the band of messages above it, see nk_pad_overlay_top */
const float padOverlayOverlayGap = perfCardTextGap;

/* how much of the window the card may take before its names are cut instead */
const float padOverlayMaxWidthShare = 0.22f;

/* a row: the dots, the controller, and its name under both, each on its own line */
const float padOverlayRowGap = 18.0f;
const float padOverlayDotSize = 9.0f;
const float padOverlayDotGap = 7.0f;
const float padOverlayDotsGap = 9.0f;      /* between the dots and the controller below them */
const float padOverlayIconGap = 7.0f;      /* between the controller and its name */

/*
 * The controller, in the unit box of the icons of material, the way the aspect ratio artwork is drawn: a
 * body twenty two units wide and twelve tall with the pad and two buttons punched out of it. The JavaFX
 * renderer of PXPlay draws the same artwork as an svg path, so RemoteStyle.css mirrors these numbers.
 */
const float padOverlayIconHeight = 24.0f;
const float padOverlayIconUnit = padOverlayIconHeight / 12.0f;
const float padOverlayIconWidth = 22.0f * padOverlayIconUnit;
const float padOverlayIconRadius = 2.0f * padOverlayIconUnit;

const float padOverlayDotsWidth = (NK_PAD_OVERLAY_MAX_PADS * padOverlayDotSize)
                                  + ((NK_PAD_OVERLAY_MAX_PADS - 1) * padOverlayDotGap);

/* the ink of a seat nobody is holding any more, and the stroke drawn through its controller */
const struct nk_color pad_overlay_dot_color = nk_rgba(255, 255, 255, 235);
/* the green of the app, the one its profiles show for an account that can still reach Sony */
const struct nk_color pad_overlay_dot_joined_color = nk_rgba(46, 171, 141, 255);
const struct nk_color pad_overlay_dot_idle_color = nk_rgba(255, 255, 255, 56);
const struct nk_color pad_overlay_icon_color = nk_rgba(255, 255, 255, 224);
const struct nk_color pad_overlay_icon_gone_color = nk_rgba(255, 255, 255, 77);
const struct nk_color pad_overlay_text_color = perf_card_text_color;
const struct nk_color pad_overlay_text_gone_color = nk_rgba(255, 255, 255, 110);
const float padOverlayGoneStroke = 2.0f * NK_PERF_SCALE;

/** How tall one row is for a font this size: the dots, the controller, and the name under them. */
static float nk_pad_overlay_row_height(const struct nk_user_font* font)
{
    return padOverlayDotSize + padOverlayDotsGap + padOverlayIconHeight + padOverlayIconGap + font->height;
}

/**
 * How far down the card starts: below the band along the top of the window the app puts its messages in,
 * which is the performance overlay where a session shows one and the join hint below it.
 *
 * Those are centred on the top edge and given the width their contents ask for, so on a window narrow
 * enough either will reach into the corner this card wants and neither can be dodged sideways. Below them
 * is the one place no window width can break.
 *
 * The room for the hint is kept whether or not the hint is up, the way the hint itself keeps the room for
 * the overlay above it. A card that dropped every time a hint appeared and rose again three seconds later
 * would be the most restless thing on screen, and the seating it shows changes rarely enough that it should
 * read as part of the window rather than as something happening.
 */
static float nk_pad_overlay_top(nk_bool perfOverlayVisible, nk_bool perfOverlayCollapsed)
{
    return nk_join_hint_top(perfOverlayVisible, perfOverlayCollapsed) + joinHintHeight
           + padOverlayOverlayGap;
}

/**
 * The name of a seat, as a pointer into the block of them the app sends.
 *
 * The names arrive as one string with a newline between them, in seat order, because a row shows one
 * short label and four of them travel more cheaply as one string than as four. A seat whose controller
 * SDL had no name for is an empty line, and a seat past the end of the block has no line at all; both
 * come back as a length of zero, which is a row that shows no name rather than a row that is missing.
 */
static const char* nk_pad_overlay_name_of(const char* names, int padIndex, int* length)
{
    const char* at = names;
    int seat = 0;

    *length = 0;
    if (!names) return NULL;

    while (seat < padIndex) {
        while (*at != '\0' && *at != '\n') at++;
        if (*at == '\0') return NULL;
        at++;
        seat++;
    }
    while (at[*length] != '\0' && at[*length] != '\n') (*length)++;
    return at;
}

/**
 * How wide the card is: the widest of what its rows have to show, and never more than its share of the
 * window. The dots are the floor, so a card of nameless seats is still a card rather than a sliver.
 */
static float nk_pad_overlay_width(const struct nk_user_font* font, int seats, const char* names,
                                  float viewportWidth)
{
    float widest = padOverlayDotsWidth;
    const float room = (viewportWidth * padOverlayMaxWidthShare) - (2.0f * padOverlayPaddingX);
    int padIndex;

    if (padOverlayIconWidth > widest) widest = padOverlayIconWidth;
    for (padIndex = 0; padIndex < seats; ++padIndex) {
        int length = 0;
        const char* name = nk_pad_overlay_name_of(names, padIndex, &length);
        float width;

        if (length < 1) continue;
        width = font->width(font->userdata, font->height, name, length);
        if (width > widest) widest = width;
    }
    if (widest > room) widest = room;
    return widest + (2.0f * padOverlayPaddingX);
}

/**
 * The four dots of a seat, the leftmost of them player one, with the seat's own dot inked and the rest
 * left as outlines the way a controller shows its player number.
 *
 * That dot is green once the console has let this player into the session and white while it has not,
 * which is the difference between a controller the game is listening to and one whose player still has to
 * ask. Player one is green from the moment a controller takes the seat, because the console was told about
 * that pad when the stream connected and it never has to ask for anything.
 *
 * A seat whose controller has gone shows no inked dot at all: its number is still spoken for, but
 * nothing about it should read as a player who is there.
 */
static void nk_pad_overlay_draw_dots(struct nk_command_buffer* out, float left, float top,
                                     int padIndex, nk_bool connected, nk_bool joined)
{
    int dot;

    for (dot = 0; dot < NK_PAD_OVERLAY_MAX_PADS; ++dot) {
        const struct nk_rect bounds = nk_rect(left + ((padOverlayDotSize + padOverlayDotGap) * (float) dot),
                                              top, padOverlayDotSize, padOverlayDotSize);

        if (dot == padIndex && connected) {
            nk_fill_circle(out, bounds, joined ? pad_overlay_dot_joined_color : pad_overlay_dot_color);
        } else {
            nk_fill_circle(out, bounds, pad_overlay_dot_idle_color);
        }
    }
}

/** A circle of the artwork, as its centre and radius in the units of the box. */
static void nk_pad_overlay_fill_dot(struct nk_command_buffer* out, struct nk_rect bounds,
                                    float centreX, float centreY, float radius, struct nk_color color)
{
    const float size = 2.0f * radius * padOverlayIconUnit;

    nk_fill_circle(out, nk_rect(bounds.x + ((centreX - radius) * padOverlayIconUnit),
                                bounds.y + ((centreY - radius) * padOverlayIconUnit),
                                size, size), color);
}

/**
 * The controller of a seat, in the ink of the row.
 *
 * The pad and the buttons are punched back out of the body in the colour of the card, which is what stands
 * in for a shape nuklear cannot subtract one fill from another to make. That only reads as intended over
 * the card, which is the only place this is ever drawn.
 */
static void nk_pad_overlay_draw_icon(struct nk_command_buffer* out, struct nk_rect bounds,
                                     struct nk_color ink, nk_bool connected)
{
    const float unit = padOverlayIconUnit;

    nk_fill_rect(out, bounds, padOverlayIconRadius, ink);

    /* the pad, a cross of two bars eight units across and two thick, six units in from the left */
    nk_fill_rect(out, nk_rect(bounds.x + (2.0f * unit), bounds.y + (5.0f * unit),
                              8.0f * unit, 2.0f * unit), 0.0f, perf_card_color);
    nk_fill_rect(out, nk_rect(bounds.x + (5.0f * unit), bounds.y + (2.0f * unit),
                              2.0f * unit, 8.0f * unit), 0.0f, perf_card_color);

    /* and the two buttons, on the diagonal the artwork of material puts them on */
    nk_pad_overlay_fill_dot(out, bounds, 14.5f, 7.5f, 1.5f, perf_card_color);
    nk_pad_overlay_fill_dot(out, bounds, 18.5f, 4.5f, 1.5f, perf_card_color);

    /* a controller that has been unplugged is struck through, as well as being drawn in the fainter ink */
    if (!connected) {
        nk_stroke_line(out, bounds.x + (bounds.w * 0.06f), (bounds.y + bounds.h) - (bounds.h * 0.08f),
                       (bounds.x + bounds.w) - (bounds.w * 0.06f), bounds.y + (bounds.h * 0.08f),
                       padOverlayGoneStroke, pad_overlay_dot_color);
    }
}

/**
 * Draws the overlay into the canvas: one row per seat the session has handed out, in seat order from the
 * top.
 *
 * @param seats            how many seats to show, counting the empty ones below the highest one handed out
 * @param connectedMask    bit per seat, set while a controller is holding it
 * @param joinedMask       bit per seat, set once the console has let that player into the session
 * @param names            the seats' names, newline separated and in seat order, already what the app wants
 *                         shown for a controller it had nothing better to call
 * @param perfOverlayVisible   whether the performance overlay is holding the top of the window
 * @param perfOverlayCollapsed whether it is folded away to its pill, which is the shorter of the two
 */
static void nk_draw_pad_overlay(struct nk_command_buffer* out, const struct nk_user_font* font,
                                float viewportWidth, float viewportHeight,
                                int seats, int connectedMask, int joinedMask, const char* names,
                                nk_bool perfOverlayVisible, nk_bool perfOverlayCollapsed)
{
    struct nk_rect previousClip;
    struct nk_rect card;
    float rowHeight;
    float top;
    int padIndex;

    if (!out || !font || seats < 1) return;
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;
    if (seats > NK_PAD_OVERLAY_MAX_PADS) seats = NK_PAD_OVERLAY_MAX_PADS;

    rowHeight = nk_pad_overlay_row_height(font);
    card = nk_rect(padOverlaySideMargin, 0.0f,
                   nk_pad_overlay_width(font, seats, names, viewportWidth),
                   ((float) seats * rowHeight) + ((float) (seats - 1) * padOverlayRowGap)
                   + (2.0f * padOverlayPaddingY));
    /* on a whole pixel so the hairline border stays a hairline */
    card.y = (float) (int) nk_pad_overlay_top(perfOverlayVisible, perfOverlayCollapsed);

    /* a window with no room left under the overlays above is left alone rather than drawn over */
    if (card.w <= 0.0f || (card.y + card.h) > viewportHeight) return;

    previousClip = out->clip;
    nk_push_scissor(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight));

    nk_perf_draw_shadow(out, card, padOverlayRadius);
    nk_fill_rect(out, card, padOverlayRadius, perf_card_color);
    nk_stroke_rect(out, card, padOverlayRadius, padOverlayBorder, perf_card_border_color);

    top = card.y + padOverlayPaddingY;
    for (padIndex = 0; padIndex < seats; ++padIndex) {
        const nk_bool connected = (connectedMask & (1 << padIndex)) != 0 ? nk_true : nk_false;
        const nk_bool joined = (joinedMask & (1 << padIndex)) != 0 ? nk_true : nk_false;
        const float left = card.x + padOverlayPaddingX;
        const float room = card.w - (2.0f * padOverlayPaddingX);
        int length = 0;
        const char* name = nk_pad_overlay_name_of(names, padIndex, &length);

        nk_pad_overlay_draw_dots(out, left, top, padIndex, connected, joined);
        nk_pad_overlay_draw_icon(out, nk_rect(left, top + padOverlayDotSize + padOverlayDotsGap,
                                              padOverlayIconWidth, padOverlayIconHeight),
                                 connected ? pad_overlay_icon_color : pad_overlay_icon_gone_color,
                                 connected);

        if (length > 0) {
            char visual[NK_BIDI_MAX_BYTES];
            /* a hebrew name is drawn in the order it reads, see bidi_text.h */
            const char* line = nk_bidi_visual(name, length, visual, (int) sizeof(visual), &length);
            const int fits = nk_perf_clamp_text(font, line, length, room);

            if (fits > 0) {
                const struct nk_rect bounds = nk_rect(left,
                                                      (top + rowHeight) - font->height,
                                                      room, font->height);

                nk_draw_text(out, bounds, line, fits, font, nk_rgba(0, 0, 0, 0),
                             connected ? pad_overlay_text_color : pad_overlay_text_gone_color);
            }
        }

        top += rowHeight + padOverlayRowGap;
    }

    nk_push_scissor(out, previousClip);
}

#endif /* PXPLAY_PAD_OVERLAY_H */
