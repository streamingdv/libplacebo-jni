/**
 * The aspect ratio icons of the in stream button panel, drawn with nuklear.
 *
 * The button that cycles the video format carries the artwork the Android app of PXPlay puts on these
 * modes (ic_video_format_*.xml). None of it is in the icon font, so the shapes are transcribed here in
 * the unit box of that artwork and drawn with the primitives of nuklear. Every renderer that draws the
 * panel with nuklear includes this file and the copies of it have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/aspect_icons.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/aspect_icons.h
 *
 * The button is hit tested on the java side, so DesktopUiController and VulkanRendererBackend of PXPlay
 * mirror where it sits, not what it shows. Nothing here has to follow a change made there.
 *
 * nuklear.h and ui_consts.h have to be included before this file.
 */
#ifndef PXPLAY_ASPECT_ICONS_H
#define PXPLAY_ASPECT_ICONS_H

/* the modes in the order the app cycles them, which is the order of its VideoFormat */
#define NK_ASPECT_MODE_KEEP 0
#define NK_ASPECT_MODE_STRETCHED 1
#define NK_ASPECT_MODE_ZOOMED 2
#define NK_ASPECT_MODE_16_10 3
#define NK_ASPECT_MODE_18_9 4
#define NK_ASPECT_MODE_21_9 5
#define NK_ASPECT_MODE_4_3 6
#define NK_ASPECT_MODE_AMBIENT 7
#define NK_ASPECT_MODE_COUNT 8

/* what the icon covers, which is about what the glyph of a neighbouring button of the panel covers */
const float aspectIconInk = buttonSize * 0.5f;
/* the artwork is laid out in a box of 24 units whose ink spans 18 of them */
const float aspectIconUnit = aspectIconInk / 18.0f;
const float aspectIconBox = 24.0f;
/* the frame of the keep mode is stroked, thinner than the 2 units of the bars, as in the artwork */
const float aspectIconFrameWidth = 1.15f;
const float aspectIconFrameRadius = 1.5f;

/*
 * The four ratio modes are labels of a pixel font, three cells wide and five tall per glyph. Whole
 * pixels for a cell and its gap, so every block of a label is the same size wherever it lands: a block
 * of a third of a pixel more would be a block of one pixel more once it is rasterized.
 *
 * The widest label is 16:10 with fourteen cells, which has to stay inside the button: at a pitch of
 * three that is 42 of the 56 pixels the button is wide.
 */
const float aspectIconCellPitch = 3.0f;
const float aspectIconCellInk = 2.0f;
const int aspectIconLabelRows = 5;

/*
 * The ambient mode keeps the aspect like the keep mode, so it shows a picture with the light it throws
 * around itself: the filled frame of the video and two rings that fade out. The rings are the one place
 * in these icons that leans on alpha, the fade is what tells the mode apart from a plain frame.
 */
const float aspectIconGlowWidth = 1.6f;

/** A piece of the artwork that is axis aligned, in the units of its box. */
struct nk_aspect_art_rect {
    float x;
    float y;
    float w;
    float h;
};

/** A diagonal bar of an arrow, which is a rectangle turned by an eighth and stays convex. */
struct nk_aspect_art_quad {
    float x[4];
    float y[4];
};

/**
 * A glyph of the pixel font of the ratio labels: five rows of up to three cells, the lowest bit of a
 * row being its left cell. The one is a single cell wide, as it is in the artwork.
 */
struct nk_aspect_pixel_glyph {
    char label;
    int columns;
    unsigned char rows[5];
};

/*
 * The two brackets inside the frame of the keep mode. The arm of a bracket overlaps the one it belongs
 * to instead of ending against it, which is what keeps the seam of two blended edges out of the corner.
 */
static const struct nk_aspect_art_rect aspect_art_keep_rects[] = {
    { 5.0f, 7.0f, 5.0f, 2.0f }, { 5.0f, 7.0f, 2.0f, 5.0f },
    { 14.0f, 15.0f, 5.0f, 2.0f }, { 17.0f, 12.0f, 2.0f, 5.0f }
};

/* the four corner brackets the arrows of the stretched mode grow out of */
static const struct nk_aspect_art_rect aspect_art_stretch_rects[] = {
    { 3.0f, 3.0f, 6.0f, 2.0f }, { 3.0f, 3.0f, 2.0f, 6.0f },
    { 15.0f, 3.0f, 6.0f, 2.0f }, { 19.0f, 3.0f, 2.0f, 6.0f },
    { 3.0f, 19.0f, 6.0f, 2.0f }, { 3.0f, 15.0f, 2.0f, 6.0f },
    { 15.0f, 19.0f, 6.0f, 2.0f }, { 19.0f, 15.0f, 2.0f, 6.0f }
};

/* their bars, pointing at the middle of the icon */
static const struct nk_aspect_art_quad aspect_art_stretch_quads[] = {
    { { 8.1f, 5.0f, 6.4f, 9.5f }, { 9.5f, 6.4f, 5.0f, 8.1f } },
    { { 15.9f, 14.5f, 17.6f, 19.0f }, { 9.5f, 8.1f, 5.0f, 6.4f } },
    { { 5.0f, 8.1f, 9.5f, 6.4f }, { 17.6f, 14.5f, 15.9f, 19.0f } },
    { { 17.6f, 14.5f, 15.9f, 19.0f }, { 19.0f, 15.9f, 14.5f, 17.6f } }
};

/* the corner brackets of the zoomed mode, which sit the other way round */
static const struct nk_aspect_art_rect aspect_art_zoom_rects[] = {
    { 3.0f, 7.0f, 6.0f, 2.0f }, { 7.0f, 3.0f, 2.0f, 6.0f },
    { 15.0f, 7.0f, 6.0f, 2.0f }, { 15.0f, 3.0f, 2.0f, 6.0f },
    { 3.0f, 15.0f, 6.0f, 2.0f }, { 7.0f, 15.0f, 2.0f, 6.0f },
    { 15.0f, 15.0f, 6.0f, 2.0f }, { 15.0f, 15.0f, 2.0f, 6.0f }
};

/* and their bars, pointing out of the icon */
static const struct nk_aspect_art_quad aspect_art_zoom_quads[] = {
    { { 5.6f, 2.5f, 3.9f, 7.0f }, { 7.0f, 3.9f, 2.5f, 5.6f } },
    { { 17.0f, 20.1f, 21.5f, 18.4f }, { 5.6f, 2.5f, 3.9f, 7.0f } },
    { { 5.6f, 2.5f, 3.9f, 7.0f }, { 17.0f, 20.1f, 21.5f, 18.4f } },
    { { 17.0f, 18.4f, 21.5f, 20.1f }, { 18.4f, 17.0f, 20.1f, 21.5f } }
};

/* the picture of the ambient mode, in the landscape shape the video keeps */
static const struct nk_aspect_art_rect aspect_art_ambient_picture = { 7.0f, 9.0f, 10.0f, 6.0f };

/* and the two rings of light around it, from the inner one outwards */
static const struct nk_aspect_art_rect aspect_art_ambient_rings[] = {
    { 4.0f, 6.5f, 16.0f, 11.0f },
    { 1.5f, 4.0f, 21.0f, 16.0f }
};

static const float aspect_art_ambient_ring_alpha[] = { 0.5f, 0.25f };

static const struct nk_aspect_pixel_glyph aspect_pixel_font[] = {
    { '0', 3, { 7, 5, 5, 5, 7 } },
    { '1', 1, { 1, 1, 1, 1, 1 } },
    { '2', 3, { 7, 4, 7, 1, 7 } },
    { '3', 3, { 7, 4, 7, 4, 7 } },
    { '4', 3, { 5, 5, 7, 4, 4 } },
    { '6', 3, { 7, 1, 7, 5, 7 } },
    { '8', 3, { 7, 5, 7, 5, 7 } },
    { '9', 3, { 7, 5, 7, 4, 7 } },
    { ':', 1, { 0, 1, 0, 1, 0 } }
};

static const struct nk_aspect_pixel_glyph* nk_aspect_pixel_glyph_of(char label)
{
    int at;
    for (at = 0; at < (int) NK_LEN(aspect_pixel_font); ++at) {
        if (aspect_pixel_font[at].label == label) return &aspect_pixel_font[at];
    }
    return 0;
}

/** How wide a label is in cells, the gap of one cell it keeps between its glyphs included. */
static float nk_aspect_label_cells(const char* label)
{
    float cells = 0.0f;
    int at;

    for (at = 0; label[at] != '\0'; ++at) {
        const struct nk_aspect_pixel_glyph* glyph = nk_aspect_pixel_glyph_of(label[at]);
        if (!glyph) continue;
        if (cells > 0.0f) cells += 1.0f;
        cells += (float) glyph->columns;
    }
    return cells;
}

/** Where the unit box of the artwork begins, centred in the button and on whole pixels. */
static struct nk_vec2 nk_aspect_origin_of(struct nk_rect bounds)
{
    const float side = aspectIconBox * aspectIconUnit;
    return nk_vec2((float) (int) (bounds.x + ((bounds.w - side) * 0.5f)),
                   (float) (int) (bounds.y + ((bounds.h - side) * 0.5f)));
}

static void nk_aspect_fill_rects(struct nk_command_buffer* out, struct nk_vec2 origin,
                                 const struct nk_aspect_art_rect* rects, int count, struct nk_color color)
{
    int at;
    for (at = 0; at < count; ++at) {
        nk_fill_rect(out, nk_rect(origin.x + (rects[at].x * aspectIconUnit),
                                  origin.y + (rects[at].y * aspectIconUnit),
                                  rects[at].w * aspectIconUnit,
                                  rects[at].h * aspectIconUnit),
                     0.0f, color);
    }
}

static void nk_aspect_fill_quads(struct nk_command_buffer* out, struct nk_vec2 origin,
                                 const struct nk_aspect_art_quad* quads, int count, struct nk_color color)
{
    int at;
    for (at = 0; at < count; ++at) {
        float points[8];
        int corner;

        for (corner = 0; corner < 4; ++corner) {
            points[corner * 2] = origin.x + (quads[at].x[corner] * aspectIconUnit);
            points[(corner * 2) + 1] = origin.y + (quads[at].y[corner] * aspectIconUnit);
        }
        nk_fill_polygon(out, points, 4, color);
    }
}

/** A ratio as the blocks of the pixel font, centred in the button. */
static void nk_aspect_draw_ratio(struct nk_command_buffer* out, struct nk_rect bounds,
                                 const char* label, struct nk_color color)
{
    const float trailingGap = aspectIconCellPitch - aspectIconCellInk;
    const float inkWidth = (nk_aspect_label_cells(label) * aspectIconCellPitch) - trailingGap;
    const float inkHeight = ((float) aspectIconLabelRows * aspectIconCellPitch) - trailingGap;
    /* whole pixels again, a label that begins on half of one would carry blocks of two sizes */
    const float top = (float) (int) (bounds.y + ((bounds.h - inkHeight) * 0.5f));
    float x = (float) (int) (bounds.x + ((bounds.w - inkWidth) * 0.5f));
    nk_bool leading = nk_true;
    int at;

    for (at = 0; label[at] != '\0'; ++at) {
        const struct nk_aspect_pixel_glyph* glyph = nk_aspect_pixel_glyph_of(label[at]);
        int row;

        if (!glyph) continue;
        if (!leading) x += aspectIconCellPitch;
        leading = nk_false;

        for (row = 0; row < aspectIconLabelRows; ++row) {
            int column;
            for (column = 0; column < glyph->columns; ++column) {
                if (!(glyph->rows[row] & (1 << column))) continue;
                nk_fill_rect(out, nk_rect(x + ((float) column * aspectIconCellPitch),
                                          top + ((float) row * aspectIconCellPitch),
                                          aspectIconCellInk, aspectIconCellInk),
                             0.0f, color);
            }
        }
        x += (float) glyph->columns * aspectIconCellPitch;
    }
}

/** The frame of the keep mode with the two brackets that sit in its corners. */
static void nk_aspect_draw_keep(struct nk_command_buffer* out, struct nk_vec2 origin, struct nk_color color)
{
    nk_stroke_rect(out, nk_rect(origin.x + (4.0f * aspectIconUnit), origin.y + (4.0f * aspectIconUnit),
                                16.0f * aspectIconUnit, 16.0f * aspectIconUnit),
                   aspectIconFrameRadius * aspectIconUnit, aspectIconFrameWidth * aspectIconUnit, color);
    nk_aspect_fill_rects(out, origin, aspect_art_keep_rects, (int) NK_LEN(aspect_art_keep_rects), color);
}

/** The picture of the ambient mode with the light it throws into the space around it. */
static void nk_aspect_draw_ambient(struct nk_command_buffer* out, struct nk_vec2 origin, struct nk_color color)
{
    int at;

    for (at = (int) NK_LEN(aspect_art_ambient_rings) - 1; at >= 0; --at) {
        struct nk_color glow = color;
        glow.a = (nk_byte) ((float) color.a * aspect_art_ambient_ring_alpha[at]);
        nk_stroke_rect(out, nk_rect(origin.x + (aspect_art_ambient_rings[at].x * aspectIconUnit),
                                    origin.y + (aspect_art_ambient_rings[at].y * aspectIconUnit),
                                    aspect_art_ambient_rings[at].w * aspectIconUnit,
                                    aspect_art_ambient_rings[at].h * aspectIconUnit),
                       aspectIconFrameRadius * aspectIconUnit, aspectIconGlowWidth * aspectIconUnit, glow);
    }

    nk_fill_rect(out, nk_rect(origin.x + (aspect_art_ambient_picture.x * aspectIconUnit),
                              origin.y + (aspect_art_ambient_picture.y * aspectIconUnit),
                              aspect_art_ambient_picture.w * aspectIconUnit,
                              aspect_art_ambient_picture.h * aspectIconUnit),
                 aspectIconFrameRadius * aspectIconUnit, color);
}

/**
 * Draws the icon of one mode into the canvas, filling the whole button it is given.
 *
 * The panel draws its icons in black on the white chip of a button, so the shadow the artwork carries for
 * the video behind it is left out here.
 */
static void nk_draw_aspect_icon(struct nk_command_buffer* out, struct nk_rect bounds, int mode,
                                struct nk_color color)
{
    struct nk_vec2 origin;

    if (!out || bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    origin = nk_aspect_origin_of(bounds);

    switch (mode) {
    case NK_ASPECT_MODE_STRETCHED:
        nk_aspect_fill_rects(out, origin, aspect_art_stretch_rects,
                             (int) NK_LEN(aspect_art_stretch_rects), color);
        nk_aspect_fill_quads(out, origin, aspect_art_stretch_quads,
                             (int) NK_LEN(aspect_art_stretch_quads), color);
        break;
    case NK_ASPECT_MODE_ZOOMED:
        nk_aspect_fill_rects(out, origin, aspect_art_zoom_rects,
                             (int) NK_LEN(aspect_art_zoom_rects), color);
        nk_aspect_fill_quads(out, origin, aspect_art_zoom_quads,
                             (int) NK_LEN(aspect_art_zoom_quads), color);
        break;
    case NK_ASPECT_MODE_16_10:
        nk_aspect_draw_ratio(out, bounds, "16:10", color);
        break;
    case NK_ASPECT_MODE_18_9:
        nk_aspect_draw_ratio(out, bounds, "18:9", color);
        break;
    case NK_ASPECT_MODE_21_9:
        nk_aspect_draw_ratio(out, bounds, "21:9", color);
        break;
    case NK_ASPECT_MODE_4_3:
        nk_aspect_draw_ratio(out, bounds, "4:3", color);
        break;
    case NK_ASPECT_MODE_AMBIENT:
        nk_aspect_draw_ambient(out, origin, color);
        break;
    default:
        nk_aspect_draw_keep(out, origin, color);
        break;
    }
}

#endif /* PXPLAY_ASPECT_ICONS_H */
