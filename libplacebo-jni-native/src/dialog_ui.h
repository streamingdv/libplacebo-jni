/**
 * The dialog of the in stream overlay, drawn with nuklear.
 *
 * A question has to look the same wherever PXPlay asks it, so this draws the card, the typography and
 * the buttons of the dialog the app shows outside of a stream. Every renderer that draws dialogs with
 * nuklear includes this file and the copies of it have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/dialog_ui.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/dialog_ui.h
 *
 * The buttons and the switch are hit tested on the java side, so DesktopUiController and
 * VulkanRendererBackend of PXPlay mirror the metrics below and have to follow a change made here.
 *
 * nuklear.h has to be included before this file.
 */
#ifndef PXPLAY_DIALOG_UI_H
#define PXPLAY_DIALOG_UI_H

#include "bidi_text.h"

/*
 * The overlay is drawn in framebuffer pixels and does not scale with the window, so the metrics are the
 * ones of the app multiplied by about 1.6. That is the ratio between the 26px body font baked for the
 * overlay and the 16px one the app uses, which keeps the card in proportion to its text without baking
 * another font size.
 */
const float dialogWindowMargin = 64.0f;      /* the room that is kept between the card and the window */
const float dialogWidthRatio = 0.5f;
const float dialogMinWidth = 720.0f;
const float dialogMaxWidth = 940.0f;
const float dialogPaddingX = 42.0f;
const float dialogPaddingTop = 38.0f;
const float dialogPaddingBottom = 32.0f;
const float dialogSpacing = 22.0f;
const float dialogCornerRadius = 24.0f;
const float dialogCardBorder = 2.0f;    /* nuklear rounds a stroke down to whole pixels, so 1.6 would be 1 */
const float dialogTitleHeight = 44.0f;
const float dialogTextLinePadding = 2.5f;    /* half of the line spacing, nuklear adds it above and below */
const float dialogTextBandMax = 232.0f;      /* seven lines, past that the text of an error is cut */
const float dialogTextBandReference = 132.0f; /* four lines: the length the card is balanced around */
const float dialogSwitchRowHeight = 44.0f;
const float dialogSwitchWidth = 64.0f;
const float dialogSwitchHeight = 36.0f;
const float dialogSwitchBorder = 2.0f;
const float dialogSwitchThumbInset = 4.0f;
const float dialogSwitchLabelGap = 14.0f;
const float dialogButtonWidth = 190.0f;
const float dialogButtonHeight = 64.0f;
const float dialogButtonSpacing = 16.0f;
const float dialogButtonRadius = 16.0f;
const float dialogButtonBorder = 2.0f;
const float dialogButtonLabelPadding = 16.0f;
const float dialogFocusRingInset = 5.0f;     /* the ring sits outside of what it belongs to, as in the app */
const float dialogFocusRingWidth = 3.0f;
const float dialogFocusRingRadius = 20.0f;
const int dialogShadowLayers = 5;       /* rings instead of a blur, see nk_dialog_draw */
const float dialogShadowGrow = 4.0f;    /* how much wider each ring is than the one inside it */
const float dialogShadowDrop = 2.0f;    /* and how much lower, the app drops its shadow as well */

const struct nk_color dialog_scrim_color = nk_rgba(2, 6, 16, 153);
const struct nk_color dialog_shadow_color = nk_rgba(0, 0, 0, 20);
const struct nk_color dialog_card_color = nk_rgb(29, 33, 42);
const struct nk_color dialog_card_border_color = nk_rgba(255, 255, 255, 41);
const struct nk_color dialog_title_color = nk_rgb(255, 255, 255);
const struct nk_color dialog_body_color = nk_rgba(255, 255, 255, 199);
const struct nk_color dialog_primary_color = nk_rgb(10, 116, 216);
const struct nk_color dialog_primary_pressed_color = nk_rgb(10, 94, 175);
const struct nk_color dialog_primary_label_color = nk_rgb(255, 255, 255);
const struct nk_color dialog_secondary_color = nk_rgba(255, 255, 255, 18);
const struct nk_color dialog_secondary_pressed_color = nk_rgba(255, 255, 255, 51);
const struct nk_color dialog_secondary_border_color = nk_rgba(255, 255, 255, 56);
const struct nk_color dialog_secondary_label_color = nk_rgba(255, 255, 255, 235);
const struct nk_color dialog_focus_ring_color = nk_rgb(255, 255, 0);
const struct nk_color dialog_focus_pressed_color = nk_rgba(255, 255, 0, 217);
const struct nk_color dialog_focus_pressed_label_color = nk_rgb(0, 0, 0);
const struct nk_color dialog_switch_off_color = nk_rgba(255, 255, 255, 36);
const struct nk_color dialog_switch_on_color = nk_rgb(26, 127, 219);
const struct nk_color dialog_switch_border_color = nk_rgba(255, 255, 255, 46);
const struct nk_color dialog_switch_thumb_color = nk_rgba(255, 255, 255, 242);
const struct nk_color dialog_switch_label_color = nk_rgba(255, 255, 255, 230);

/** What to show. The renderer owns none of it, the java side pushes it with every change. */
struct nk_dialog_content {
    const char* title;
    const char* text;
    nk_bool showSwitch;         /* a switch instead of the text, for a question with an option */
    const char* switchText;
    nk_bool switchChecked;
    nk_bool switchFocused;
    const char* leftButtonText; /* the answer that is not the offered one, left out when empty */
    nk_bool leftButtonFocused;
    nk_bool leftButtonPressed;
    const char* rightButtonText;
    nk_bool rightButtonFocused;
    nk_bool rightButtonPressed;
};

/** Where everything goes, in framebuffer pixels. */
struct nk_dialog_layout {
    struct nk_rect card;
    struct nk_rect title;
    struct nk_rect text;
    struct nk_rect switchRow;
    struct nk_rect switchTrack;
    struct nk_rect switchLabel;
    struct nk_rect leftButton;
    struct nk_rect rightButton;
};

/** Everything of the card that is not its content: the paddings, the title, the spacings and the buttons. */
static float nk_dialog_chrome_height(void)
{
    return dialogPaddingTop + dialogTitleHeight + dialogSpacing + dialogSpacing
           + dialogButtonHeight + dialogPaddingBottom;
}

/** The width of the card, which follows the window and nothing of its content. */
static float nk_dialog_card_width(float viewportWidth)
{
    float width = viewportWidth * dialogWidthRatio;
    float room = viewportWidth - (2.0f * dialogWindowMargin);

    if (width < dialogMinWidth) width = dialogMinWidth;
    if (width > dialogMaxWidth) width = dialogMaxWidth;

    /* a window that has no room for the margin keeps a sliver of itself free instead */
    if (room < viewportWidth * 0.92f) room = viewportWidth * 0.92f;
    return (width > room) ? room : width;
}

/**
 * Where everything goes for a card that is only as tall as the band its content asks for.
 *
 * A question with a switch has a height of its own and is centred. A card with text grows upwards from
 * its buttons instead: the java side hit tests the buttons and cannot measure the text, so they have to
 * keep the place a text of the usual length would give them, whatever this one needs.
 */
static struct nk_dialog_layout nk_dialog_layout_of(float viewportWidth, float viewportHeight,
                                                   nk_bool showSwitch, float bandHeight)
{
    struct nk_dialog_layout layout;
    const float chrome = nk_dialog_chrome_height();
    const float width = nk_dialog_card_width(viewportWidth);
    float roomForHeight = viewportHeight - (2.0f * dialogWindowMargin);
    float height;
    float bottom;
    float top;
    float contentWidth;
    float contentTop;
    float buttonTop;

    if (roomForHeight < viewportHeight * 0.92f) roomForHeight = viewportHeight * 0.92f;

    if (showSwitch) {
        if (bandHeight > roomForHeight - chrome) bandHeight = roomForHeight - chrome;
        if (bandHeight < 0.0f) bandHeight = 0.0f;
        height = chrome + bandHeight;
        top = (viewportHeight - height) * 0.5f;
        if (top < 0.0f) top = 0.0f;
    } else {
        bottom = (viewportHeight + chrome + dialogTextBandReference) * 0.5f;
        if (bottom > viewportHeight) bottom = viewportHeight;
        /* the room is the one above the buttons, which keeps the card on the screen without moving them */
        if (roomForHeight > bottom) roomForHeight = bottom;
        if (bandHeight > roomForHeight - chrome) bandHeight = roomForHeight - chrome;
        if (bandHeight < 0.0f) bandHeight = 0.0f;
        height = chrome + bandHeight;
        top = bottom - height;
    }

    layout.card = nk_rect((viewportWidth - width) * 0.5f, top, width, height);

    contentWidth = width - (2.0f * dialogPaddingX);
    contentTop = layout.card.y + dialogPaddingTop + dialogTitleHeight + dialogSpacing;
    buttonTop = (layout.card.y + height) - dialogPaddingBottom - dialogButtonHeight;

    layout.title = nk_rect(layout.card.x + dialogPaddingX, layout.card.y + dialogPaddingTop,
                           contentWidth, dialogTitleHeight);

    layout.text = nk_rect(layout.card.x + dialogPaddingX, contentTop, contentWidth, bandHeight);

    layout.switchRow = nk_rect(layout.card.x + dialogPaddingX, contentTop, contentWidth, dialogSwitchRowHeight);
    layout.switchTrack = nk_rect(layout.switchRow.x,
                                 contentTop + ((dialogSwitchRowHeight - dialogSwitchHeight) * 0.5f),
                                 dialogSwitchWidth, dialogSwitchHeight);
    layout.switchLabel = nk_rect(layout.switchTrack.x + dialogSwitchWidth + dialogSwitchLabelGap, contentTop,
                                 contentWidth - dialogSwitchWidth - dialogSwitchLabelGap, dialogSwitchRowHeight);

    layout.rightButton = nk_rect((layout.card.x + width) - dialogPaddingX - dialogButtonWidth, buttonTop,
                                 dialogButtonWidth, dialogButtonHeight);
    layout.leftButton = nk_rect(layout.rightButton.x - dialogButtonSpacing - dialogButtonWidth, buttonTop,
                                dialogButtonWidth, dialogButtonHeight);
    return layout;
}

/**
 * One past the last byte of the line that begins at start, which is where the next one begins.
 *
 * This is the rule the wrapped label of nuklear breaks by: it fills the line until what stands in it
 * reaches the width, which takes the glyph that crosses the edge along, and then breaks behind the last
 * space of the line, or right there when the line holds none. Both the band height and the right to left
 * drawing below go through here, which is what keeps them on the lines nuklear would draw.
 */
static int nk_dialog_line_end(const struct nk_user_font* font, const char* text, int length, int start,
                              float width)
{
    int at = start;
    int lastBreak = start;   /* right behind the last space of the line, its start while it has none */

    while (at < length) {
        nk_rune unicode = 0;
        int glyphLength = nk_utf_decode(&text[at], &unicode, length - at);
        if (glyphLength < 1) glyphLength = 1;

        if (at > start && font->width(font->userdata, font->height, &text[start], at - start) >= width) {
            return (lastBreak > start) ? lastBreak : at;
        }

        at += glyphLength;
        if (unicode == ' ') lastBreak = at;
    }
    return length;
}

/**
 * The band height the wrapped text needs to show every one of its lines, which is what makes the card as
 * tall as its text instead of as tall as the longest text there could be.
 *
 * The band the wrapped label of nuklear needs is a little taller than its lines, it keeps the spacing of
 * a line above the first one and below the last one and stops one line short of the height it is given.
 */
static float nk_dialog_wrapped_height(const struct nk_user_font* font, const char* text, float width)
{
    const float lineHeight = font->height + (2.0f * dialogTextLinePadding);
    const int length = nk_strlen(text);
    /* a pixel tighter than what the label is given: counting a line early only costs a pixel of offset,
     * counting one too few would cut the end of the text off */
    const float budget = width - 1.0f;
    int lines = 0;
    int at = 0;

    do {
        at = nk_dialog_line_end(font, text, length, at, budget);
        lines++;
    } while (at < length);

    return (3.0f * dialogTextLinePadding) + ((float) lines * lineHeight) + 1.0f;
}

/**
 * Draws the body text of a hebrew dialog, line by line and each of them in the order it reads.
 *
 * The wrapped label of nuklear cannot do that: it breaks the text into lines while it draws, so the
 * order of a line is only known inside of it. This puts the lines where nuklear puts them - the same
 * breaks, the same spacing of half a line above each of them, the same stop once the band is full - and
 * leaves them where the app leaves them as well, which is against the left edge of the card.
 */
static void nk_dialog_draw_wrapped_bidi(struct nk_command_buffer* out, struct nk_rect bounds,
                                        const char* text, const struct nk_user_font* font,
                                        struct nk_color color)
{
    const float lineHeight = font->height + (2.0f * dialogTextLinePadding);
    const float bandBottom = (bounds.y + bounds.h) - (2.0f * dialogTextLinePadding);
    const int length = nk_strlen(text);
    float row = bounds.y + dialogTextLinePadding;
    int at = 0;

    while (at < length && (row + lineHeight) < bandBottom) {
        char visual[NK_BIDI_MAX_BYTES];
        const int end = nk_dialog_line_end(font, text, length, at, bounds.w);
        int lineLength = end - at;
        const char* line;

        /* the space a line breaks behind would sit in front of a hebrew one and indent it */
        while (lineLength > 0 && text[at + lineLength - 1] == ' ') lineLength--;

        line = nk_bidi_visual(&text[at], lineLength, visual, (int) sizeof(visual), &lineLength);
        nk_draw_text(out, nk_rect(bounds.x, row + dialogTextLinePadding, bounds.w, font->height),
                     line, lineLength, font, nk_rgba(0, 0, 0, 0), color);
        row += lineHeight;
        at = end;
    }
}

/** Draws one line, centred in its row the way the app centres a label in its own. */
static void nk_dialog_draw_label(struct nk_command_buffer* out, struct nk_rect bounds, const char* label,
                                 const struct nk_user_font* font, struct nk_color color, nk_bool bold)
{
    char visual[NK_BIDI_MAX_BYTES];
    const char* text;
    struct nk_rect line;
    int length;

    if (!out || !font || !label || label[0] == '\0' || bounds.w <= 0.0f) return;

    /* a hebrew label is drawn in the order it reads, see bidi_text.h */
    text = nk_bidi_visual(label, nk_strlen(label), visual, (int) sizeof(visual), &length);
    line = bounds;
    line.y = bounds.y + ((bounds.h - font->height) * 0.5f);
    line.h = font->height;

    nk_draw_text(out, line, text, length, font, nk_rgba(0, 0, 0, 0), color);
    if (bold) {
        /* only one weight is baked, so the title is drawn twice to read as the bold title of the app */
        line.x += 1.0f;
        nk_draw_text(out, line, text, length, font, nk_rgba(0, 0, 0, 0), color);
    }
}

static void nk_dialog_draw_focus_ring(struct nk_command_buffer* out, struct nk_rect bounds)
{
    nk_stroke_rect(out, nk_rect(bounds.x - dialogFocusRingInset, bounds.y - dialogFocusRingInset,
                                bounds.w + (2.0f * dialogFocusRingInset), bounds.h + (2.0f * dialogFocusRingInset)),
                   dialogFocusRingRadius, dialogFocusRingWidth, dialog_focus_ring_color);
}

static void nk_dialog_draw_button(struct nk_command_buffer* out, struct nk_rect bounds, const char* label,
                                  const struct nk_user_font* font, nk_bool primary, nk_bool focused, nk_bool pressed)
{
    struct nk_color fill = primary
                           ? (pressed ? dialog_primary_pressed_color : dialog_primary_color)
                           : (pressed ? dialog_secondary_pressed_color : dialog_secondary_color);
    struct nk_color labelColor = primary ? dialog_primary_label_color : dialog_secondary_label_color;
    struct nk_rect labelBounds;
    float labelWidth;

    if (!out || !font) return;

    /* the app fills the focused button with the colour of its ring while it is held down */
    if (focused && pressed) {
        fill = dialog_focus_pressed_color;
        labelColor = dialog_focus_pressed_label_color;
    }

    nk_fill_rect(out, bounds, dialogButtonRadius, fill);
    if (!primary && !pressed) {
        nk_stroke_rect(out, bounds, dialogButtonRadius, dialogButtonBorder, dialog_secondary_border_color);
    }
    if (focused) {
        nk_dialog_draw_focus_ring(out, bounds);
    }

    labelBounds = nk_rect(bounds.x + dialogButtonLabelPadding, bounds.y,
                          bounds.w - (2.0f * dialogButtonLabelPadding), bounds.h);
    if (label && label[0] != '\0' && labelBounds.w > 0.0f) {
        labelWidth = font->width(font->userdata, font->height, label, nk_strlen(label));
        if (labelWidth < labelBounds.w) {
            labelBounds.x += (labelBounds.w - labelWidth) * 0.5f;
        }
        nk_dialog_draw_label(out, labelBounds, label, font, labelColor, nk_false);
    }
}

/** The switch of the app: a pill with a round thumb, filled when it is on. */
static void nk_dialog_draw_switch(struct nk_command_buffer* out, const struct nk_dialog_layout* layout,
                                  const char* label, const struct nk_user_font* font,
                                  nk_bool checked, nk_bool focused)
{
    float radius;
    float thumbSize;
    float thumbX;

    if (!out || !layout) return;

    radius = layout->switchTrack.h * 0.5f;
    thumbSize = layout->switchTrack.h - (2.0f * dialogSwitchThumbInset);
    thumbX = checked
             ? ((layout->switchTrack.x + layout->switchTrack.w) - dialogSwitchThumbInset - thumbSize)
             : (layout->switchTrack.x + dialogSwitchThumbInset);

    nk_fill_rect(out, layout->switchTrack, radius, checked ? dialog_switch_on_color : dialog_switch_off_color);
    if (!checked) {
        nk_stroke_rect(out, layout->switchTrack, radius, dialogSwitchBorder, dialog_switch_border_color);
    }
    nk_fill_circle(out, nk_rect(thumbX, layout->switchTrack.y + dialogSwitchThumbInset, thumbSize, thumbSize),
                   dialog_switch_thumb_color);

    nk_dialog_draw_label(out, layout->switchLabel, label, font, dialog_switch_label_color, nk_false);

    if (focused) {
        nk_dialog_draw_focus_ring(out, layout->switchRow);
    }
}

/**
 * Draws the whole dialog into the canvas of the window that is currently begun.
 *
 * titleFont is the larger of the baked fonts, bodyFont the one everything else uses. Nothing but the
 * wrapped body text goes through a widget, which keeps the dialog on the pixels the java side hit tests.
 */
static void nk_dialog_draw(struct nk_context* ctx, float viewportWidth, float viewportHeight,
                           const struct nk_user_font* titleFont, const struct nk_user_font* bodyFont,
                           const struct nk_dialog_content* content)
{
    struct nk_command_buffer* out;
    struct nk_dialog_layout layout;
    struct nk_rect previousClip;
    struct nk_rect shadow;
    float band;
    int layer;

    if (!ctx || !content || !titleFont || !bodyFont) return;
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;

    out = nk_window_get_canvas(ctx);
    if (!out) return;

    if (content->showSwitch) {
        band = dialogSwitchRowHeight;
    } else if (content->text && content->text[0] != '\0') {
        /* the band of the text is measured before the card is placed, so the card is as tall as it reads */
        band = nk_dialog_wrapped_height(bodyFont, content->text,
                                        nk_dialog_card_width(viewportWidth) - (2.0f * dialogPaddingX));
        if (band > dialogTextBandMax) band = dialogTextBandMax;
    } else {
        band = 0.0f;
    }

    layout = nk_dialog_layout_of(viewportWidth, viewportHeight, content->showSwitch, band);

    /* the window keeps a padding of its own, which the dimming and the shadow of the card have to cross */
    previousClip = out->clip;
    nk_push_scissor(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight));

    /* what is behind the dialog is dimmed, as in the app, which also keeps the text readable over a bright frame */
    nk_fill_rect(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight), 0.0f, dialog_scrim_color);

    /* stacked rings instead of a blur, which is what the drop shadow of the app costs here */
    for (layer = dialogShadowLayers; layer > 0; --layer) {
        const float grow = (float) layer * dialogShadowGrow;
        shadow = nk_rect(layout.card.x - grow, (layout.card.y - grow) + ((float) layer * dialogShadowDrop),
                         layout.card.w + (2.0f * grow), layout.card.h + (2.0f * grow));
        nk_fill_rect(out, shadow, dialogCornerRadius + grow, dialog_shadow_color);
    }

    nk_fill_rect(out, layout.card, dialogCornerRadius, dialog_card_color);
    nk_stroke_rect(out, layout.card, dialogCornerRadius, dialogCardBorder, dialog_card_border_color);

    nk_dialog_draw_label(out, layout.title, content->title, titleFont, dialog_title_color, nk_true);

    if (content->showSwitch) {
        nk_dialog_draw_switch(out, &layout, content->switchText, bodyFont,
                              content->switchChecked, content->switchFocused);
    } else if (content->text && content->text[0] != '\0' && layout.text.h > 0.0f) {
        if (nk_bidi_has_rtl(content->text, nk_strlen(content->text))) {
            nk_dialog_draw_wrapped_bidi(out, layout.text, content->text, bodyFont, dialog_body_color);
        } else {
            /* the wrapped label of nuklear breaks the line of a translated text the way the app does */
            const struct nk_user_font* previousFont = ctx->style.font;
            const struct nk_vec2 previousPadding = ctx->style.text.padding;

            ctx->style.text.padding = nk_vec2(0.0f, dialogTextLinePadding);
            nk_style_set_font(ctx, bodyFont);
            nk_layout_space_push(ctx, nk_layout_space_rect_to_local(ctx, layout.text));
            nk_label_colored_wrap(ctx, content->text, dialog_body_color);
            nk_style_set_font(ctx, previousFont);
            ctx->style.text.padding = previousPadding;
        }
    }

    if (content->leftButtonText && content->leftButtonText[0] != '\0') {
        nk_dialog_draw_button(out, layout.leftButton, content->leftButtonText, bodyFont, nk_false,
                              content->leftButtonFocused, content->leftButtonPressed);
    }
    if (content->rightButtonText && content->rightButtonText[0] != '\0') {
        nk_dialog_draw_button(out, layout.rightButton, content->rightButtonText, bodyFont, nk_true,
                              content->rightButtonFocused, content->rightButtonPressed);
    }

    nk_push_scissor(out, previousClip);
}

#endif /* PXPLAY_DIALOG_UI_H */
