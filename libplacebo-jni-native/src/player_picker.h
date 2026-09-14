/**
 * The card a joining player picks the account they join with on, drawn with nuklear.
 *
 * A player who presses the join button on a session on a PlayStation 5 has to say which account the
 * console seats them with, and this is that question: a row of the accounts PXPlay holds, one more tile
 * that signs a new one in, and the code a phone reads to do that. Every renderer that draws it with
 * nuklear includes this file and the copies of it have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/player_picker.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/player_picker.h
 *
 * The card and the dialog are the same piece of furniture, so the paddings, the buttons and the colours
 * come out of dialog_ui.h rather than being spelled out again here.
 *
 * The tiles and the buttons are hit tested on the java side, so DesktopUiController and
 * VulkanRendererBackend of PXPlay mirror the metrics below and have to follow a change made here. The
 * card is built so that they can: every band of it is as tall as it is told to be rather than as tall as
 * its text measures, which is what lets java place the tiles without having a font to measure with.
 *
 * nuklear.h and dialog_ui.h have to be included before this file.
 */
#ifndef PXPLAY_PLAYER_PICKER_H
#define PXPLAY_PLAYER_PICKER_H

#include "dialog_ui.h"

/** Which page the card is showing, the same ones the app knows. */
#define NK_PLAYER_PICKER_PAGE_ACCOUNTS 0
#define NK_PLAYER_PICKER_PAGE_QR_CODE 1
#define NK_PLAYER_PICKER_PAGE_SIGNING_IN 2
#define NK_PLAYER_PICKER_PAGE_PASSCODE 3

/** How many boxes a passcode has, which is how many digits a console passcode has. */
#define NK_PLAYER_PICKER_PASSCODE_DIGITS 4

/** As many accounts as the session seats, plus the tile that signs a new one in. */
#define NK_PLAYER_PICKER_MAX_TILES 5

/*
 * The largest face the card takes for a tile, along each edge.
 *
 * A face is drawn at the size of the circle it goes in and never larger, so what the app sends is a
 * picture of about that many pixels rather than the one the console holds. Anything above this is refused
 * by the renderer instead of being shrunk, there being no scaler here to shrink it with.
 */
#define NK_PLAYER_PICKER_AVATAR_MAX_EDGE 160

/*
 * What the focus says when it is on a button of the card rather than on one of the tiles: the row of
 * tiles and the row of buttons are the two rows a player moves the highlight between.
 */
#define NK_PLAYER_PICKER_FOCUS_CANCEL (-1)
#define NK_PLAYER_PICKER_FOCUS_BACK (-2)

/*
 * In framebuffer pixels, as everything of the overlay is, which is the metrics of the app multiplied by
 * about 1.6. See the note on that ratio in dialog_ui.h.
 */
const float playerPickerRingSize = 134.0f;        /* the ring around an avatar, which the focus is drawn on */
const float playerPickerAvatarSize = 110.0f;      /* the circle inside it */
const float playerPickerTileGap = 35.0f;          /* between two of those rings */
const float playerPickerNameGap = 12.0f;          /* between a ring and the name under it */
const float playerPickerNameHeight = 26.0f;
const float playerPickerRingWidth = 5.0f;         /* the focus ring, thicker than a dialog's as it is round */
const int playerPickerDashCount = 14;             /* the dashes of the circle of the new account tile */
const float playerPickerDashFill = 0.62f;         /* how much of a dash step is drawn rather than left out */
const float playerPickerDashWidth = 2.0f;
const float playerPickerPlusArm = 26.0f;          /* the plus inside that circle */
const float playerPickerPlusThickness = 4.0f;
const float playerPickerAvatarBorder = 2.0f;
const float playerPickerDividerHeight = 2.0f;
const float playerPickerQrPadding = 20.0f;        /* the white edge the code is framed with */
const float playerPickerQrTargetSize = 380.0f;    /* what the code is drawn at when the window has the room */
const float playerPickerQrMinModule = 2.0f;       /* below this a phone stops reading it */
const float playerPickerQrRadius = 12.0f;

/*
 * The boxes a passcode is typed into and the legend of buttons under them, laid out as the console lays
 * its own out: a row of four boxes with the one being typed into lit, and the ten buttons that type a
 * digit written out in three columns so a player does not have to be told which one does what.
 */
const float playerPickerPasscodeBoxWidth = 84.0f;
const float playerPickerPasscodeBoxHeight = 96.0f;
const float playerPickerPasscodeBoxGap = 22.0f;
const float playerPickerPasscodeBoxRadius = 13.0f;
const float playerPickerPasscodeBoxBorder = 3.0f;
const float playerPickerPasscodeDotSize = 22.0f;  /* says a digit is typed without saying which */
const float playerPickerPasscodeKeysGap = 29.0f;  /* between the row of boxes and the legend below it */
const int playerPickerPasscodeKeyColumns = 3;
const float playerPickerPasscodeDigitWidth = 22.0f;
const float playerPickerPasscodeKeyGap = 13.0f;   /* between a digit and the cap that types it */
const float playerPickerPasscodeKeyCapWidth = 52.0f;
const float playerPickerPasscodeKeyCapHeight = 38.0f;
const float playerPickerPasscodeKeyCapRadius = 10.0f;
const float playerPickerPasscodeKeyCapBorder = 2.0f;
const float playerPickerPasscodeKeyColumnGap = 42.0f;
const float playerPickerPasscodeKeyRowGap = 13.0f;
const float playerPickerPasscodeGlyphArm = 9.0f;  /* half the shape drawn on a cap that carries no name */
const float playerPickerPasscodeGlyphWidth = 2.0f;

/*
 * The two sizes of the body font the card sets its own text in, neither of them baked: the lines of the
 * two text bands, a little larger than a dialog's so they read over a moving frame, and the name under a
 * tile, smaller than those but well above the 16px of the small baked font, which came out too small.
 *
 * Both are taken off the body font instead of baking more sizes: nuklear scales the glyphs of an atlas to
 * whatever height it is asked to draw them at, and another size of a chinese or a japanese range would
 * cost the atlas thousands of glyphs for the sake of two lines of the card.
 */
const float playerPickerBandFontHeight = 29.0f;
const float playerPickerNameFontHeight = 24.0f;

/*
 * Two lines of the band font. Spelled out rather than measured so that the height of the card is a number
 * the java side can arrive at as well: it hit tests the tiles and the buttons and has no font to measure a
 * translated line with. The name is left out of it, staying under the height of the band it is drawn in.
 */
const float playerPickerTextLineHeight = playerPickerBandFontHeight + (2.0f * dialogTextLinePadding);
const float playerPickerTextBandHeight = (3.0f * dialogTextLinePadding) + (2.0f * playerPickerTextLineHeight) + 1.0f;

/*
 * One of those two fonts, which a renderer keeps beside the ones it baked.
 *
 * Held by the renderer rather than made here for every card, because a line of text holds on to the font
 * it was drawn with until the frame is turned into vertices, which happens long after this file is done
 * with it. What it is derived from has to outlive it, which the baked font does.
 */
static struct nk_user_font nk_player_picker_derived_font(const struct nk_user_font* bodyFont, float height)
{
    struct nk_user_font font = *bodyFont;
    font.height = height;
    return font;
}

const struct nk_color player_picker_avatar_color = nk_rgba(255, 255, 255, 26);
const struct nk_color player_picker_avatar_border_color = nk_rgba(255, 255, 255, 56);
const struct nk_color player_picker_monogram_color = nk_rgba(255, 255, 255, 235);
const struct nk_color player_picker_name_color = nk_rgba(255, 255, 255, 214);
const struct nk_color player_picker_new_account_color = nk_rgba(255, 255, 255, 13);
const struct nk_color player_picker_dash_color = nk_rgba(255, 255, 255, 97);
const struct nk_color player_picker_divider_color = nk_rgba(255, 255, 255, 33);
const struct nk_color player_picker_qr_paper_color = nk_rgb(255, 255, 255);
const struct nk_color player_picker_qr_ink_color = nk_rgb(0, 0, 0);
const struct nk_color player_picker_passcode_box_color = nk_rgba(255, 255, 255, 18);
const struct nk_color player_picker_passcode_box_border_color = nk_rgba(255, 255, 255, 61);
const struct nk_color player_picker_passcode_box_focus_color = nk_rgba(255, 255, 255, 31);
const struct nk_color player_picker_passcode_dot_color = nk_rgba(255, 255, 255, 235);
const struct nk_color player_picker_passcode_cap_color = nk_rgba(255, 255, 255, 33);
const struct nk_color player_picker_passcode_cap_border_color = nk_rgba(255, 255, 255, 66);
const struct nk_color player_picker_passcode_glyph_color = nk_rgba(255, 255, 255, 219);

/** The shape drawn on a key cap, for the buttons the console draws rather than names. */
#define NK_PLAYER_PICKER_KEY_NAMED 0
#define NK_PLAYER_PICKER_KEY_LEFT 1
#define NK_PLAYER_PICKER_KEY_UP 2
#define NK_PLAYER_PICKER_KEY_RIGHT 3
#define NK_PLAYER_PICKER_KEY_DOWN 4
#define NK_PLAYER_PICKER_KEY_TRIANGLE 5
#define NK_PLAYER_PICKER_KEY_SQUARE 6

/** One entry of the legend: the digit, the name on its cap where it has one, and the shape where it does not. */
struct nk_player_picker_passcode_key {
    const char* digit;
    const char* name;
    int shape;
};

/**
 * Which button types which digit, in the order the legend reads.
 *
 * The same order the app writes it in and the same order the console does, so that a player who knows
 * their passcode from a real PlayStation types it here without reading any of this. The digits are text
 * rather than arithmetic because that is what nk_draw_text wants of them.
 */
const struct nk_player_picker_passcode_key playerPickerPasscodeKeys[10] = {
    { "1", 0, NK_PLAYER_PICKER_KEY_LEFT },
    { "2", 0, NK_PLAYER_PICKER_KEY_UP },
    { "3", 0, NK_PLAYER_PICKER_KEY_RIGHT },
    { "4", 0, NK_PLAYER_PICKER_KEY_DOWN },
    { "5", "R1", NK_PLAYER_PICKER_KEY_NAMED },
    { "6", "R2", NK_PLAYER_PICKER_KEY_NAMED },
    { "7", "L1", NK_PLAYER_PICKER_KEY_NAMED },
    { "8", "L2", NK_PLAYER_PICKER_KEY_NAMED },
    { "9", 0, NK_PLAYER_PICKER_KEY_TRIANGLE },
    { "0", 0, NK_PLAYER_PICKER_KEY_SQUARE }
};

/** What to show. The renderer owns none of it, the java side pushes it with every change. */
struct nk_player_picker_content {
    int page;
    int focused;                    /* which tile the ring is drawn on, the new account one after the last,
                                     * or one of the NK_PLAYER_PICKER_FOCUS_ values for a button of the card.
                                     * A page with no highlight of its own, which the passcode is, sends a
                                     * value that is none of those on purpose: the ring is drawn by matching
                                     * this, so matching nothing draws nothing. Never clamp it into range. */
    int accountCount;
    const char* title;
    const char* message;            /* the line above the code, on that page only */
    const char* hint;               /* that the console has to hold the account as a user of its own */
    const char* accountNames;       /* newline separated and in tile order */
    const char* monograms;          /* the letters of their circles, newline separated and in the same order */
    /*
     * The faces of those circles, in the same order, or nothing at all.
     *
     * The letter above is what a tile shows until its face is there: the app reads a face off the disk or
     * off the network, which takes longer than the card takes to open, and a face that never arrives
     * leaves a tile as it was rather than leaving it empty. So this is null while there are none, and a
     * handle inside it is null for the tiles that are still waiting.
     */
    const struct nk_image* avatars;
    const char* newAccountText;
    const char* cancelText;
    const char* backText;
    nk_bool showBackButton;
    nk_bool cancelPressed;
    nk_bool backPressed;
    int qrModuleCount;              /* along each edge, quiet zone included, 0 while there is no code */
    const unsigned char* qrModules; /* one byte per module, row by row, non zero for a dark one */
    /*
     * How many digits of the passcode have been typed, on that page only, which is how many boxes are
     * filled and which one of them is lit.
     *
     * A count and not the digits: what a player types is an answer to their own console and the card has
     * no reason to know it, so the digits stay in the app and never reach a renderer at all.
     */
    int typedDigits;
};

/** Where everything goes, in framebuffer pixels. A band the page does not show is left empty. */
struct nk_player_picker_layout {
    struct nk_rect card;
    struct nk_rect title;
    struct nk_rect message;
    struct nk_rect tiles;       /* the whole row of them */
    float tileScale;            /* how much the row had to shrink to fit the card, 1 while it fits */
    struct nk_rect qr;          /* the white frame, the code centred inside it */
    struct nk_rect passcode;    /* the row of boxes and the legend under it */
    struct nk_rect hint;
    struct nk_rect divider;
    struct nk_rect cancelButton;
    struct nk_rect backButton;
};

/** How wide the row of tiles wants to be, before the card says how much of that it can have. */
static float nk_player_picker_row_width(int tileCount)
{
    if (tileCount < 1) tileCount = 1;
    return ((float) tileCount * playerPickerRingSize) + ((float) (tileCount - 1) * playerPickerTileGap);
}

/** A ring, the gap below it and the name under that: the block one tile of the row takes. */
static float nk_player_picker_tile_height(void)
{
    return playerPickerRingSize + playerPickerNameGap + playerPickerNameHeight;
}

/**
 * How many pixels one module of the code is drawn at, which is what its frame is sized from.
 *
 * Whole pixels only: a module of two and a half would land on different pixel counts down the code and a
 * phone would read the wrong thing out of it.
 */
static float nk_player_picker_qr_module_size(int moduleCount)
{
    float size;

    if (moduleCount < 1) return 0.0f;
    size = (float) ((int) (playerPickerQrTargetSize / (float) moduleCount));
    if (size < playerPickerQrMinModule) size = playerPickerQrMinModule;
    return size;
}

/** The white frame of the code, its padding included, or nothing while there is no code. */
static float nk_player_picker_qr_frame_size(int moduleCount)
{
    const float module = nk_player_picker_qr_module_size(moduleCount);
    if (module <= 0.0f) return 0.0f;
    return (module * (float) moduleCount) + (2.0f * playerPickerQrPadding);
}

/** How wide the row of four boxes is, which is what centres it in the card. */
static float nk_player_picker_passcode_row_width(void)
{
    return ((float) NK_PLAYER_PICKER_PASSCODE_DIGITS * playerPickerPasscodeBoxWidth)
           + ((float) (NK_PLAYER_PICKER_PASSCODE_DIGITS - 1) * playerPickerPasscodeBoxGap);
}

/** How wide one column of the legend is: a digit, the gap after it and the cap that types it. */
static float nk_player_picker_passcode_key_width(void)
{
    return playerPickerPasscodeDigitWidth + playerPickerPasscodeKeyGap + playerPickerPasscodeKeyCapWidth;
}

/** How many rows the ten entries of the legend come to in the columns it is written in. */
static int nk_player_picker_passcode_key_rows(void)
{
    const int columns = playerPickerPasscodeKeyColumns;
    return (10 + (columns - 1)) / columns;
}

/**
 * How tall the whole of the passcode page is: the row of boxes, the gap and the legend under it.
 *
 * A number rather than a measurement, as every other band of the card is, so that the java side arrives
 * at the same height of card without a font to measure the legend with.
 */
static float nk_player_picker_passcode_band_height(void)
{
    const int rows = nk_player_picker_passcode_key_rows();
    return playerPickerPasscodeBoxHeight + playerPickerPasscodeKeysGap
           + ((float) rows * playerPickerPasscodeKeyCapHeight)
           + ((float) (rows - 1) * playerPickerPasscodeKeyRowGap);
}

/**
 * Where everything of the card goes.
 *
 * The card is as tall as the page it shows and centred in the window. Every band of it is a number
 * rather than a measurement, so the java side arrives at the same rects without a font: the two text
 * bands are two lines however long their translation is, the row of tiles is as tall as its rings, and
 * the code is a whole number of modules across.
 */
static struct nk_player_picker_layout nk_player_picker_layout_of(float viewportWidth, float viewportHeight,
                                                                int page, int tileCount, int qrModuleCount)
{
    struct nk_player_picker_layout layout;
    const struct nk_rect nothing = nk_rect(0.0f, 0.0f, 0.0f, 0.0f);
    const float width = nk_dialog_card_width(viewportWidth);
    const float contentWidth = width - (2.0f * dialogPaddingX);
    const float rowWidth = nk_player_picker_row_width(tileCount);
    const nk_bool showTiles = (page == NK_PLAYER_PICKER_PAGE_ACCOUNTS) ? nk_true : nk_false;
    const nk_bool showQr = (page == NK_PLAYER_PICKER_PAGE_QR_CODE && qrModuleCount > 0) ? nk_true : nk_false;
    const nk_bool showPasscode = (page == NK_PLAYER_PICKER_PAGE_PASSCODE) ? nk_true : nk_false;
    const nk_bool showMessage = (page != NK_PLAYER_PICKER_PAGE_ACCOUNTS) ? nk_true : nk_false;
    const nk_bool showHint = (page != NK_PLAYER_PICKER_PAGE_SIGNING_IN) ? nk_true : nk_false;
    const float qrFrame = showQr ? nk_player_picker_qr_frame_size(qrModuleCount) : 0.0f;
    const float passcodeBand = showPasscode ? nk_player_picker_passcode_band_height() : 0.0f;
    float height;
    float top;
    float row;
    float contentLeft;
    float buttonTop;

    layout.message = nothing;
    layout.tiles = nothing;
    layout.qr = nothing;
    layout.passcode = nothing;
    layout.hint = nothing;

    /* a window too narrow for the whole row shrinks it rather than letting it out of the card */
    layout.tileScale = (rowWidth > contentWidth && rowWidth > 0.0f) ? (contentWidth / rowWidth) : 1.0f;

    height = dialogPaddingTop + dialogTitleHeight + dialogSpacing;
    if (showTiles) height += (nk_player_picker_tile_height() * layout.tileScale) + dialogSpacing;
    if (showMessage) height += playerPickerTextBandHeight + dialogSpacing;
    if (showPasscode) height += passcodeBand + dialogSpacing;
    if (showQr) height += qrFrame + dialogSpacing;
    if (showHint) height += playerPickerTextBandHeight + dialogSpacing;
    height += playerPickerDividerHeight + dialogSpacing + dialogButtonHeight + dialogPaddingBottom;

    top = (viewportHeight - height) * 0.5f;
    if (top < 0.0f) top = 0.0f;

    layout.card = nk_rect((viewportWidth - width) * 0.5f, top, width, height);
    contentLeft = layout.card.x + dialogPaddingX;

    row = layout.card.y + dialogPaddingTop;
    layout.title = nk_rect(contentLeft, row, contentWidth, dialogTitleHeight);
    row += dialogTitleHeight + dialogSpacing;

    if (showTiles) {
        const float scaledRow = rowWidth * layout.tileScale;
        const float tileHeight = nk_player_picker_tile_height() * layout.tileScale;
        layout.tiles = nk_rect(layout.card.x + ((width - scaledRow) * 0.5f), row, scaledRow, tileHeight);
        row += tileHeight + dialogSpacing;
    }
    if (showMessage) {
        layout.message = nk_rect(contentLeft, row, contentWidth, playerPickerTextBandHeight);
        row += playerPickerTextBandHeight + dialogSpacing;
    }
    if (showPasscode) {
        layout.passcode = nk_rect(contentLeft, row, contentWidth, passcodeBand);
        row += passcodeBand + dialogSpacing;
    }
    if (showQr) {
        layout.qr = nk_rect(layout.card.x + ((width - qrFrame) * 0.5f), row, qrFrame, qrFrame);
        row += qrFrame + dialogSpacing;
    }
    if (showHint) {
        layout.hint = nk_rect(contentLeft, row, contentWidth, playerPickerTextBandHeight);
        row += playerPickerTextBandHeight + dialogSpacing;
    }

    layout.divider = nk_rect(contentLeft, row, contentWidth, playerPickerDividerHeight);

    /*
     * The same pair of slots the dialog uses, so the buttons of the two read as one and the same, but with
     * cancel in the offered one: leaving a card that was opened by accident is what a player wants of it
     * most, and it is the only answer the page of the phone link has that ends the question.
     */
    buttonTop = (layout.card.y + height) - dialogPaddingBottom - dialogButtonHeight;
    layout.cancelButton = nk_rect((layout.card.x + width) - dialogPaddingX - dialogButtonWidth, buttonTop,
                                  dialogButtonWidth, dialogButtonHeight);
    layout.backButton = nk_rect(layout.cancelButton.x - dialogButtonSpacing - dialogButtonWidth, buttonTop,
                                dialogButtonWidth, dialogButtonHeight);
    return layout;
}

/** Where the ring of one tile sits, counting from the leftmost account. */
static struct nk_rect nk_player_picker_ring_bounds(const struct nk_player_picker_layout* layout, int tile)
{
    const float ring = playerPickerRingSize * layout->tileScale;
    const float stride = ring + (playerPickerTileGap * layout->tileScale);
    return nk_rect(layout->tiles.x + ((float) tile * stride), layout->tiles.y, ring, ring);
}

/** One line out of a newline separated list, or nothing when the list is shorter than that. */
static const char* nk_player_picker_line(const char* list, int index, int* length)
{
    int at = 0;
    int line = 0;

    *length = 0;
    if (!list) return 0;

    while (list[at] != '\0') {
        int end = at;
        while (list[end] != '\0' && list[end] != '\n') end++;
        if (line == index) {
            *length = end - at;
            return &list[at];
        }
        if (list[end] == '\0') return 0;
        at = end + 1;
        line++;
    }
    return 0;
}

/**
 * Draws one line centred in its row, cut short with an ellipsis when it is wider than the row.
 *
 * A name is the one piece of the card that comes from a player rather than from a translation, so it is
 * the one that has to be kept inside the tile it belongs to. The ellipsis is drawn behind what fits
 * rather than copied onto it, which keeps this off a buffer of its own.
 */
static void nk_player_picker_draw_name(struct nk_command_buffer* out, struct nk_rect bounds,
                                       const char* name, int length, const struct nk_user_font* font,
                                       struct nk_color color)
{
    static const char ellipsis[] = "\xE2\x80\xA6";
    char visual[NK_BIDI_MAX_BYTES];
    const char* text;
    struct nk_rect line;
    int visualLength;
    float textWidth;

    if (!out || !font || !name || length < 1 || bounds.w <= 0.0f) return;

    /* a hebrew name is drawn in the order it reads, see bidi_text.h */
    text = nk_bidi_visual(name, length, visual, (int) sizeof(visual), &visualLength);
    textWidth = font->width(font->userdata, font->height, text, visualLength);

    if (textWidth <= bounds.w) {
        line = nk_rect(bounds.x + ((bounds.w - textWidth) * 0.5f),
                       bounds.y + ((bounds.h - font->height) * 0.5f), textWidth, font->height);
        nk_draw_text(out, line, text, visualLength, font, nk_rgba(0, 0, 0, 0), color);
        return;
    }

    {
        const float ellipsisWidth = font->width(font->userdata, font->height, ellipsis, 3);
        int fits = visualLength;
        float fittingWidth = 0.0f;

        /* back off a glyph at a time until what is left and the ellipsis fit, which is where it is cut */
        while (fits > 0) {
            int back = 1;
            while (back < fits && (text[fits - back] & 0xC0) == 0x80) back++;
            fits -= back;
            if (fits < 1) return;
            fittingWidth = font->width(font->userdata, font->height, text, fits);
            if (fittingWidth + ellipsisWidth <= bounds.w) break;
        }

        line = nk_rect(bounds.x + ((bounds.w - (fittingWidth + ellipsisWidth)) * 0.5f),
                       bounds.y + ((bounds.h - font->height) * 0.5f), fittingWidth, font->height);
        nk_draw_text(out, line, text, fits, font, nk_rgba(0, 0, 0, 0), color);
        line.x += fittingWidth;
        line.w = ellipsisWidth;
        nk_draw_text(out, line, ellipsis, 3, font, nk_rgba(0, 0, 0, 0), color);
    }
}

/**
 * The circle of an account: its face, or the first letter of its name while there is none.
 *
 * The face is drawn as the square it came as and not clipped to the circle. The rounding is in the picture
 * itself, the app having drawn it into a round mask before sending it, which is what keeps this off a
 * stencil the overlay has no pass for.
 */
static void nk_player_picker_draw_avatar(struct nk_command_buffer* out, struct nk_rect ring, float scale,
                                         const char* monogram, int monogramLength,
                                         const struct nk_user_font* monogramFont,
                                         const struct nk_image* face)
{
    const float size = playerPickerAvatarSize * scale;
    const struct nk_rect circle = nk_rect(ring.x + ((ring.w - size) * 0.5f),
                                          ring.y + ((ring.h - size) * 0.5f), size, size);

    nk_fill_circle(out, circle, player_picker_avatar_color);

    if (face && face->handle.ptr) {
        nk_draw_image(out, circle, face, nk_rgb(255, 255, 255));
        nk_stroke_circle(out, circle, playerPickerAvatarBorder, player_picker_avatar_border_color);
        return;
    }

    nk_stroke_circle(out, circle, playerPickerAvatarBorder, player_picker_avatar_border_color);

    if (monogram && monogramLength > 0 && monogramFont) {
        const float width = monogramFont->width(monogramFont->userdata, monogramFont->height,
                                                monogram, monogramLength);
        nk_draw_text(out, nk_rect(circle.x + ((size - width) * 0.5f),
                                  circle.y + ((size - monogramFont->height) * 0.5f),
                                  width, monogramFont->height),
                     monogram, monogramLength, monogramFont, nk_rgba(0, 0, 0, 0),
                     player_picker_monogram_color);
    }
}

/**
 * The circle of the tile that signs a new account in: dashed, with a plus in the middle of it.
 *
 * A ring of arcs with gaps between them, nuklear having no dashed stroke of its own, which is what the
 * app draws in its place as well.
 */
static void nk_player_picker_draw_new_account(struct nk_command_buffer* out, struct nk_rect ring, float scale)
{
    const float size = playerPickerAvatarSize * scale;
    const struct nk_rect circle = nk_rect(ring.x + ((ring.w - size) * 0.5f),
                                          ring.y + ((ring.h - size) * 0.5f), size, size);
    const float centerX = circle.x + (size * 0.5f);
    const float centerY = circle.y + (size * 0.5f);
    const float step = (2.0f * NK_PI) / (float) playerPickerDashCount;
    const float arm = playerPickerPlusArm * scale;
    const float thickness = playerPickerPlusThickness * scale;
    int dash;

    nk_fill_circle(out, circle, player_picker_new_account_color);
    for (dash = 0; dash < playerPickerDashCount; ++dash) {
        const float from = (float) dash * step;
        nk_stroke_arc(out, centerX, centerY, size * 0.5f, from, from + (step * playerPickerDashFill),
                      playerPickerDashWidth, player_picker_dash_color);
    }

    nk_fill_rect(out, nk_rect(centerX - (arm * 0.5f), centerY - (thickness * 0.5f), arm, thickness),
                 0.0f, player_picker_monogram_color);
    nk_fill_rect(out, nk_rect(centerX - (thickness * 0.5f), centerY - (arm * 0.5f), thickness, arm),
                 0.0f, player_picker_monogram_color);
}

/**
 * Draws the code a phone reads, one rect per run of dark modules along a row.
 *
 * A code of the usual size comes to a few hundred rects, which is what it costs to draw one without a
 * texture: the atlas of the overlay carries coverage rather than colour, so a bitmap could not go
 * through it, and taking a run at a time is what keeps the count of them down.
 */
static void nk_player_picker_draw_qr(struct nk_command_buffer* out, struct nk_rect frame,
                                     int moduleCount, const unsigned char* modules)
{
    const float module = nk_player_picker_qr_module_size(moduleCount);
    float originX;
    float originY;
    int row;

    if (!out || frame.w <= 0.0f) return;

    nk_fill_rect(out, frame, playerPickerQrRadius, player_picker_qr_paper_color);
    if (moduleCount < 1 || !modules || module <= 0.0f) return;

    originX = frame.x + ((frame.w - (module * (float) moduleCount)) * 0.5f);
    originY = frame.y + ((frame.h - (module * (float) moduleCount)) * 0.5f);

    for (row = 0; row < moduleCount; ++row) {
        const unsigned char* line = &modules[(unsigned int) row * (unsigned int) moduleCount];
        int at = 0;
        while (at < moduleCount) {
            int run;
            if (!line[at]) {
                at++;
                continue;
            }
            run = at;
            while (run < moduleCount && line[run]) run++;
            nk_fill_rect(out, nk_rect(originX + ((float) at * module), originY + ((float) row * module),
                                      (float) (run - at) * module, module),
                         0.0f, player_picker_qr_ink_color);
            at = run;
        }
    }
}

/**
 * The shape on a key cap that carries no name: the four arrows of the d-pad and the two face buttons.
 *
 * Drawn rather than written because that is how they are marked on a controller and how the console
 * writes them, and because the atlas of the overlay carries the latin ranges and a handful of others and
 * not the shapes of a gamepad.
 */
static void nk_player_picker_draw_key_glyph(struct nk_command_buffer* out, struct nk_rect cap, int shape)
{
    const float centerX = cap.x + (cap.w * 0.5f);
    const float centerY = cap.y + (cap.h * 0.5f);
    const float arm = playerPickerPasscodeGlyphArm;
    const struct nk_color color = player_picker_passcode_glyph_color;

    switch (shape) {
    case NK_PLAYER_PICKER_KEY_LEFT:
        nk_fill_triangle(out, centerX - arm, centerY, centerX + (arm * 0.7f), centerY - arm,
                         centerX + (arm * 0.7f), centerY + arm, color);
        break;
    case NK_PLAYER_PICKER_KEY_RIGHT:
        nk_fill_triangle(out, centerX + arm, centerY, centerX - (arm * 0.7f), centerY - arm,
                         centerX - (arm * 0.7f), centerY + arm, color);
        break;
    case NK_PLAYER_PICKER_KEY_UP:
        nk_fill_triangle(out, centerX, centerY - arm, centerX - arm, centerY + (arm * 0.7f),
                         centerX + arm, centerY + (arm * 0.7f), color);
        break;
    case NK_PLAYER_PICKER_KEY_DOWN:
        nk_fill_triangle(out, centerX, centerY + arm, centerX - arm, centerY - (arm * 0.7f),
                         centerX + arm, centerY - (arm * 0.7f), color);
        break;
    case NK_PLAYER_PICKER_KEY_TRIANGLE:
        nk_stroke_triangle(out, centerX, centerY - arm, centerX - arm, centerY + (arm * 0.8f),
                           centerX + arm, centerY + (arm * 0.8f), playerPickerPasscodeGlyphWidth, color);
        break;
    case NK_PLAYER_PICKER_KEY_SQUARE:
        nk_stroke_rect(out, nk_rect(centerX - (arm * 0.8f), centerY - (arm * 0.8f), arm * 1.6f, arm * 1.6f),
                       1.0f, playerPickerPasscodeGlyphWidth, color);
        break;
    default:
        break;
    }
}

/**
 * The whole of the passcode page: the four boxes and the legend of buttons under them.
 *
 * The box being typed into is lit the way a focused button of a dialog is, so that a player looking for
 * where they are in the passcode finds it in the same yellow that tells them where they are anywhere
 * else in the overlay. What has been typed shows as a dot per box and never as a digit.
 *
 * Every row of the legend is centred on its own, which is what puts the zero of the short last row under
 * the middle column instead of against the left of it.
 */
static void nk_player_picker_draw_passcode(struct nk_command_buffer* out, struct nk_rect band,
                                           int typedDigits, const struct nk_user_font* font)
{
    const float boxStride = playerPickerPasscodeBoxWidth + playerPickerPasscodeBoxGap;
    const float boxesLeft = band.x + ((band.w - nk_player_picker_passcode_row_width()) * 0.5f);
    const float keyWidth = nk_player_picker_passcode_key_width();
    const float keyStride = keyWidth + playerPickerPasscodeKeyColumnGap;
    const float keysTop = band.y + playerPickerPasscodeBoxHeight + playerPickerPasscodeKeysGap;
    const int columns = playerPickerPasscodeKeyColumns;
    const int keyCount = (int) (sizeof(playerPickerPasscodeKeys) / sizeof(playerPickerPasscodeKeys[0]));
    int typed = typedDigits;
    int at;

    if (!out || !font || band.w <= 0.0f || band.h <= 0.0f) return;

    if (typed < 0) typed = 0;
    if (typed > NK_PLAYER_PICKER_PASSCODE_DIGITS) typed = NK_PLAYER_PICKER_PASSCODE_DIGITS;

    for (at = 0; at < NK_PLAYER_PICKER_PASSCODE_DIGITS; ++at) {
        /* the box a digit would go into next, which is none once the whole passcode is typed */
        const nk_bool lit = (at == typed) ? nk_true : nk_false;
        const struct nk_rect box = nk_rect(boxesLeft + ((float) at * boxStride), band.y,
                                           playerPickerPasscodeBoxWidth, playerPickerPasscodeBoxHeight);

        nk_fill_rect(out, box, playerPickerPasscodeBoxRadius,
                     lit ? player_picker_passcode_box_focus_color : player_picker_passcode_box_color);
        nk_stroke_rect(out, box, playerPickerPasscodeBoxRadius, playerPickerPasscodeBoxBorder,
                       lit ? dialog_focus_ring_color : player_picker_passcode_box_border_color);

        if (at < typed) {
            const float dot = playerPickerPasscodeDotSize;
            nk_fill_circle(out, nk_rect(box.x + ((box.w - dot) * 0.5f), box.y + ((box.h - dot) * 0.5f),
                                        dot, dot),
                           player_picker_passcode_dot_color);
        }
    }

    for (at = 0; at < keyCount; ++at) {
        const struct nk_player_picker_passcode_key* key = &playerPickerPasscodeKeys[at];
        const int row = at / columns;
        const int column = at % columns;
        const int inRow = (keyCount - (row * columns) < columns) ? (keyCount - (row * columns)) : columns;
        const float rowWidth = ((float) inRow * keyWidth) + ((float) (inRow - 1) * playerPickerPasscodeKeyColumnGap);
        const float left = band.x + ((band.w - rowWidth) * 0.5f) + ((float) column * keyStride);
        const float top = keysTop + ((float) row * (playerPickerPasscodeKeyCapHeight + playerPickerPasscodeKeyRowGap));
        const struct nk_rect cap = nk_rect(left + playerPickerPasscodeDigitWidth + playerPickerPasscodeKeyGap,
                                           top, playerPickerPasscodeKeyCapWidth,
                                           playerPickerPasscodeKeyCapHeight);

        nk_player_picker_draw_name(out, nk_rect(left, top, playerPickerPasscodeDigitWidth,
                                                playerPickerPasscodeKeyCapHeight),
                                   key->digit, nk_strlen(key->digit), font, player_picker_passcode_dot_color);

        nk_fill_rect(out, cap, playerPickerPasscodeKeyCapRadius, player_picker_passcode_cap_color);
        nk_stroke_rect(out, cap, playerPickerPasscodeKeyCapRadius, playerPickerPasscodeKeyCapBorder,
                       player_picker_passcode_cap_border_color);

        if (key->shape == NK_PLAYER_PICKER_KEY_NAMED) {
            nk_player_picker_draw_name(out, cap, key->name, key->name ? nk_strlen(key->name) : 0, font,
                                       player_picker_passcode_glyph_color);
        } else {
            nk_player_picker_draw_key_glyph(out, cap, key->shape);
        }
    }
}

/**
 * One of the two text bands of the card, wrapped the way the body of a dialog is and centred in it.
 *
 * The wrapped label of nuklear can only set a line against the left edge, so the lines are broken here
 * and drawn one at a time. They break where the body of a dialog breaks - see nk_dialog_line_end, which
 * follows the rule of that label - and the block of them is centred in a band that is two lines tall
 * whether the translation fills them or not.
 */
static void nk_player_picker_draw_band(struct nk_command_buffer* out, struct nk_rect bounds, const char* text,
                                       const struct nk_user_font* font)
{
    float lineHeight;
    float row;
    int length;
    int lines = 0;
    int fitting;
    int at = 0;

    if (!out || !font || !text || text[0] == '\0' || bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    lineHeight = font->height + (2.0f * dialogTextLinePadding);
    length = nk_strlen(text);

    /* what it comes to, and how much of that the band has the room for */
    do {
        at = nk_dialog_line_end(font, text, length, at, bounds.w);
        lines++;
    } while (at < length);

    fitting = (int) ((bounds.h - (2.0f * dialogTextLinePadding)) / lineHeight);
    if (fitting < 1) fitting = 1;
    if (lines > fitting) lines = fitting;

    row = bounds.y + ((bounds.h - ((float) lines * lineHeight)) * 0.5f);
    at = 0;

    while (lines-- > 0) {
        char visual[NK_BIDI_MAX_BYTES];
        const int end = nk_dialog_line_end(font, text, length, at, bounds.w);
        int lineLength = end - at;
        const char* line;
        float width;

        /* the space a line breaks behind would sit in front of a hebrew one and indent it */
        while (lineLength > 0 && text[at + lineLength - 1] == ' ') lineLength--;

        /* a hebrew line is drawn in the order it reads, see bidi_text.h */
        line = nk_bidi_visual(&text[at], lineLength, visual, (int) sizeof(visual), &lineLength);
        width = font->width(font->userdata, font->height, line, lineLength);
        nk_draw_text(out, nk_rect(bounds.x + ((bounds.w - width) * 0.5f), row + dialogTextLinePadding,
                                  width, font->height),
                     line, lineLength, font, nk_rgba(0, 0, 0, 0), dialog_body_color);
        row += lineHeight;
        at = end;
    }
}

/**
 * Draws the whole card into the canvas of the window that is currently begun.
 *
 * titleFont is the larger of the baked fonts and bodyFont the one the buttons are set in. bandFont is what
 * the two text bands are set in and nameFont what the names under the tiles and the legend of the passcode
 * page are, both of them made out of the body font by nk_player_picker_derived_font and kept by the
 * renderer: they have to outlive the frame this draws. Nothing of the card goes through a widget, which
 * keeps it on the pixels the java side hit tests.
 */
static void nk_player_picker_draw(struct nk_context* ctx, float viewportWidth, float viewportHeight,
                                  const struct nk_user_font* titleFont, const struct nk_user_font* bodyFont,
                                  const struct nk_user_font* nameFont, const struct nk_user_font* bandFont,
                                  const struct nk_player_picker_content* content)
{
    struct nk_command_buffer* out;
    struct nk_player_picker_layout layout;
    struct nk_rect previousClip;
    int tileCount;
    int tile;
    int layer;

    if (!ctx || !content || !titleFont || !bodyFont || !nameFont || !bandFont) return;
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;

    out = nk_window_get_canvas(ctx);
    if (!out) return;

    tileCount = content->accountCount + 1;
    if (tileCount > NK_PLAYER_PICKER_MAX_TILES) tileCount = NK_PLAYER_PICKER_MAX_TILES;

    layout = nk_player_picker_layout_of(viewportWidth, viewportHeight, content->page, tileCount,
                                        content->qrModuleCount);

    /* the window keeps a padding of its own, which the dimming and the shadow of the card have to cross */
    previousClip = out->clip;
    nk_push_scissor(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight));

    /* what is behind the card is dimmed, as in the app, which also keeps its text readable over a bright frame */
    nk_fill_rect(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight), 0.0f, dialog_scrim_color);

    /* stacked rings instead of a blur, which is what the drop shadow of the app costs here */
    for (layer = dialogShadowLayers; layer > 0; --layer) {
        const float grow = (float) layer * dialogShadowGrow;
        nk_fill_rect(out, nk_rect(layout.card.x - grow,
                                  (layout.card.y - grow) + ((float) layer * dialogShadowDrop),
                                  layout.card.w + (2.0f * grow), layout.card.h + (2.0f * grow)),
                     dialogCornerRadius + grow, dialog_shadow_color);
    }

    nk_fill_rect(out, layout.card, dialogCornerRadius, dialog_card_color);
    nk_stroke_rect(out, layout.card, dialogCornerRadius, dialogCardBorder, dialog_card_border_color);

    nk_dialog_draw_label(out, layout.title, content->title, titleFont, dialog_title_color, nk_true);

    if (content->page == NK_PLAYER_PICKER_PAGE_ACCOUNTS) {
        for (tile = 0; tile < tileCount; ++tile) {
            const struct nk_rect ring = nk_player_picker_ring_bounds(&layout, tile);
            struct nk_rect name;
            int length = 0;
            const char* label;

            if (tile >= content->accountCount) {
                nk_player_picker_draw_new_account(out, ring, layout.tileScale);
                label = content->newAccountText;
                length = label ? nk_strlen(label) : 0;
            } else {
                const char* monogram = nk_player_picker_line(content->monograms, tile, &length);
                nk_player_picker_draw_avatar(out, ring, layout.tileScale, monogram, length, titleFont,
                                             content->avatars ? &content->avatars[tile] : 0);
                label = nk_player_picker_line(content->accountNames, tile, &length);
            }

            if (tile == content->focused) {
                nk_stroke_circle(out, ring, playerPickerRingWidth * layout.tileScale, dialog_focus_ring_color);
            }

            /* the name is given the gap on both sides of its ring as well, a name being wider than a face */
            name = nk_rect(ring.x - (playerPickerTileGap * 0.5f * layout.tileScale),
                           ring.y + ring.h + (playerPickerNameGap * layout.tileScale),
                           ring.w + (playerPickerTileGap * layout.tileScale),
                           playerPickerNameHeight * layout.tileScale);
            nk_player_picker_draw_name(out, name, label, length, nameFont, player_picker_name_color);
        }
    } else {
        nk_player_picker_draw_band(out, layout.message, content->message, bandFont);
        if (content->page == NK_PLAYER_PICKER_PAGE_QR_CODE) {
            nk_player_picker_draw_qr(out, layout.qr, content->qrModuleCount, content->qrModules);
        } else if (content->page == NK_PLAYER_PICKER_PAGE_PASSCODE) {
            nk_player_picker_draw_passcode(out, layout.passcode, content->typedDigits, nameFont);
        }
    }

    if (content->page != NK_PLAYER_PICKER_PAGE_SIGNING_IN) {
        nk_player_picker_draw_band(out, layout.hint, content->hint, bandFont);
    }

    nk_fill_rect(out, layout.divider, 0.0f, player_picker_divider_color);

    /* cancel wears the blue of an offered answer here, this card being one a player leaves rather than one
     * they have to answer, and back is the one beside it */
    nk_dialog_draw_button(out, layout.cancelButton, content->cancelText, bodyFont, nk_true,
                          content->focused == NK_PLAYER_PICKER_FOCUS_CANCEL, content->cancelPressed);
    if (content->showBackButton) {
        nk_dialog_draw_button(out, layout.backButton, content->backText, bodyFont, nk_false,
                              content->focused == NK_PLAYER_PICKER_FOCUS_BACK, content->backPressed);
    }

    nk_push_scissor(out, previousClip);
}

#endif /* PXPLAY_PLAYER_PICKER_H */
