/**
 * The volume buttons of the in stream button panel, drawn with nuklear.
 *
 * A session can offer a pair of chips that lower and raise the volume of the output device of the system.
 * The icon font of the panel carries no speaker, so the two are transcribed here in the unit box the
 * aspect ratio artwork uses and drawn with the primitives of nuklear, which keeps them the same weight as
 * the icons beside them. Both chips are styled like the mic button they follow, so this file draws them
 * whole rather than only their artwork. Every renderer that draws the panel with nuklear includes it and
 * the copies of it have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/volume_icons.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/volume_icons.h
 *
 * The chips are hit tested on the java side, so DesktopUiController and VulkanRendererBackend of PXPlay
 * mirror where they sit, not what they show, and the slots they take are the ones panelLeftSlotX of
 * ui_consts.h hands out. What a press does is decided there as well: this file knows of no volume, only
 * of two buttons.
 *
 * nuklear.h and ui_consts.h have to be included before this file.
 */
#ifndef PXPLAY_VOLUME_ICONS_H
#define PXPLAY_VOLUME_ICONS_H

/* which way a chip of the pair moves the volume, and therefore which sign it carries */
#define NK_VOLUME_DIRECTION_DOWN (-1)
#define NK_VOLUME_DIRECTION_UP 1
/* how many chips the pair has, the lower one first */
#define NK_VOLUME_BUTTON_COUNT 2

/* the ink and the unit box of the aspect ratio artwork, see aspect_icons.h */
const float volumeIconInk = buttonSize * 0.5f;
const float volumeIconUnit = volumeIconInk / 18.0f;
const float volumeIconBox = 24.0f;

/** A piece of the artwork that is axis aligned, in the units of its box. */
struct nk_volume_art_rect {
    float x;
    float y;
    float w;
    float h;
};

/** The horn of the speaker, a trapezoid with two vertical sides and therefore convex. */
struct nk_volume_art_quad {
    float x[4];
    float y[4];
};

/*
 * The speaker, which is the shape the volume artwork of material carries: a neck of six units that opens
 * into a horn of sixteen. It leaves the right third of the box to the sign, and both together span the
 * eighteen units of ink the icons of the panel are drawn in.
 */
static const struct nk_volume_art_rect volume_art_speaker_neck = { 3.0f, 9.0f, 4.0f, 6.0f };
static const struct nk_volume_art_quad volume_art_speaker_horn = {
    { 7.0f, 12.0f, 12.0f, 7.0f },
    { 9.0f, 4.0f, 20.0f, 15.0f }
};

/*
 * The sign, two units thick as the bars of the aspect artwork are. The minus is the bar of the plus, so
 * the two chips carry the same shape with one piece more on the one that raises.
 */
static const struct nk_volume_art_rect volume_art_sign_bar = { 14.5f, 11.0f, 6.5f, 2.0f };
static const struct nk_volume_art_rect volume_art_sign_stem = { 16.75f, 8.75f, 2.0f, 6.5f };

/** Where the unit box of the artwork begins, centred in the chip and on whole pixels. */
static struct nk_vec2 nk_volume_origin_of(struct nk_rect bounds)
{
    const float side = volumeIconBox * volumeIconUnit;
    return nk_vec2((float) (int) (bounds.x + ((bounds.w - side) * 0.5f)),
                   (float) (int) (bounds.y + ((bounds.h - side) * 0.5f)));
}

static void nk_volume_fill_rect(struct nk_command_buffer* out, struct nk_vec2 origin,
                                const struct nk_volume_art_rect* art, struct nk_color color)
{
    nk_fill_rect(out, nk_rect(origin.x + (art->x * volumeIconUnit),
                              origin.y + (art->y * volumeIconUnit),
                              art->w * volumeIconUnit,
                              art->h * volumeIconUnit),
                 0.0f, color);
}

/**
 * Draws the speaker and the sign of one direction into the canvas, filling the chip it is given.
 *
 * The panel draws its icons in black on the white chip of a button, so the shadow the artwork carries for
 * the video behind it is left out here.
 */
static void nk_draw_volume_icon(struct nk_command_buffer* out, struct nk_rect bounds, int direction,
                                struct nk_color color)
{
    struct nk_vec2 origin;
    float horn[8];
    int corner;

    if (!out || bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    origin = nk_volume_origin_of(bounds);

    nk_volume_fill_rect(out, origin, &volume_art_speaker_neck, color);
    for (corner = 0; corner < 4; ++corner) {
        horn[corner * 2] = origin.x + (volume_art_speaker_horn.x[corner] * volumeIconUnit);
        horn[(corner * 2) + 1] = origin.y + (volume_art_speaker_horn.y[corner] * volumeIconUnit);
    }
    nk_fill_polygon(out, horn, 4, color);

    nk_volume_fill_rect(out, origin, &volume_art_sign_bar, color);
    if (direction > 0) {
        nk_volume_fill_rect(out, origin, &volume_art_sign_stem, color);
    }
}

/**
 * Draws both chips of the pair, in the slots of the left hand cluster that follow the mic button.
 *
 * @param bounds        the viewport the panel is drawn in
 * @param showMicButton whether the mic button takes the first slot of the cluster
 */
static void nk_draw_volume_buttons(struct nk_context* ctx, struct nk_rect bounds, nk_bool showMicButton,
                                   nk_bool downPressed, nk_bool upPressed)
{
    const struct nk_style_button cachedButtonStyle = ctx->style.button;
    const int firstSlot = showMicButton ? 1 : 0;
    int at;

    /* both chips or neither, and only while the strip has room for them beside the middle cluster */
    if (!panelLeftSlotFits(bounds.w, firstSlot + 1)) return;

    for (at = 0; at < NK_VOLUME_BUTTON_COUNT; ++at) {
        const nk_bool pressed = (at == 0) ? downPressed : upPressed;
        const int direction = (at == 0) ? NK_VOLUME_DIRECTION_DOWN : NK_VOLUME_DIRECTION_UP;
        const struct nk_color chipColor = pressed ? pressed_white_button_color_alpha
                                                  : white_button_color_alpha;
        const struct nk_rect chip = nk_rect(panelLeftSlotX(firstSlot + at),
                                            (bounds.h - buttonSize) - bottomPadding,
                                            buttonSize, buttonSize);

        ctx->style.button.normal = nk_style_item_color(chipColor);
        ctx->style.button.hover  = nk_style_item_color(chipColor);
        ctx->style.button.active = nk_style_item_color(chipColor);
        ctx->style.button.border_color = black_button_color;
        ctx->style.button.rounding = 8;
        ctx->style.button.border = 0;

        /* the chip carries no glyph, the icon is drawn onto the canvas over it. The layout space takes
           its rect in local coordinates while the canvas draws in screen ones, so the icon has to be
           placed where the chip ends up rather than where it was asked for. */
        nk_layout_space_push(ctx, chip);
        (void) nk_button_label(ctx, "");
        nk_draw_volume_icon(nk_window_get_canvas(ctx), nk_layout_space_rect_to_screen(ctx, chip),
                            direction, black_button_color);
    }

    ctx->style.button = cachedButtonStyle;
}

#endif /* PXPLAY_VOLUME_ICONS_H */
