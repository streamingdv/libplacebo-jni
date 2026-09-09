/**
 * The light bar stripe of the in stream view, drawn with nuklear.
 *
 * A band along the top edge of the window in the colour the console asks the DualSense light bar to
 * take, strongest at the very edge and gone a little way down. Every renderer that draws it with
 * nuklear includes this file and the copies of it have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/light_bar_stripe.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/light_bar_stripe.h
 *
 * The JavaFX renderers of PXPlay draw the same band without nuklear, so LightBarStripe there mirrors
 * the numbers below and has to follow a change made here.
 *
 * The Android client puts the band on all four edges of the surface. Only the top one is drawn here:
 * the bottom edge sits under the button panel, and the two sides frame a window that on a desktop is
 * rarely the only thing on the display.
 *
 * The thickness is a share of the window rather than a count of pixels, which is what keeps the band
 * the same size everywhere. This one is drawn in framebuffer pixels while the JavaFX one is laid out in
 * logical points, and a share of the height comes out the same on both whatever the display is scaled
 * to, where a count of pixels would have come out half the size on a display running at 200%.
 *
 * The fade ends on the band's own colour carrying no alpha rather than on transparent black, so a
 * bright colour does not run through a grey halfway down.
 *
 * nuklear.h has to be included before this file.
 */
#ifndef PXPLAY_LIGHT_BAR_STRIPE_H
#define PXPLAY_LIGHT_BAR_STRIPE_H

/* Android draws 22dp of a phone, which is about this share of the height of its window in landscape */
const float lightBarHeightFraction = 0.035f;
const float lightBarMinHeight = 14.0f;  /* a small window still gets a band that reads as one */
const float lightBarMaxHeight = 56.0f;  /* and a 2160p one gets a stripe rather than a curtain */
const float lightBarMaxHeightShare = 0.2f; /* the cap the Android client draws with, for a short window */

/* Android's alpha for a phone, at the very edge; the fade takes it from there to nothing */
const int lightBarEdgeAlpha = 84;

/** How thick the band is for a window of this height, on a whole pixel. */
static float nk_light_bar_height(float viewportHeight)
{
    float height = viewportHeight * lightBarHeightFraction;
    const float cap = viewportHeight * lightBarMaxHeightShare;

    if (height < lightBarMinHeight) height = lightBarMinHeight;
    if (height > lightBarMaxHeight) height = lightBarMaxHeight;
    if (height > cap) height = cap;
    return (float) (int) height;
}

/**
 * Draws the band, or nothing at all for a session that has no colour to show.
 *
 * @param lightBarArgb the colour as 0xAARRGGBB, or 0 for nothing to draw. Of its alpha only whether it
 *                     is set is read: the band fades from lightBarEdgeAlpha whatever the value says, so
 *                     that a console asking for black still gets a band.
 */
static void nk_draw_light_bar_stripe(struct nk_command_buffer* out, float viewportWidth, float viewportHeight,
                                     unsigned int lightBarArgb)
{
    struct nk_color edge;
    struct nk_color faded;
    struct nk_rect previousClip;
    float height;

    if (!out || (lightBarArgb >> 24) == 0u) return;
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;

    edge = nk_rgba((int) ((lightBarArgb >> 16) & 0xFFu),
                   (int) ((lightBarArgb >> 8) & 0xFFu),
                   (int) (lightBarArgb & 0xFFu),
                   lightBarEdgeAlpha);
    faded = edge;
    faded.a = 0;
    height = nk_light_bar_height(viewportHeight);

    /* the window keeps a padding of its own, which the band has to cross to reach both ends of the edge */
    previousClip = out->clip;
    nk_push_scissor(out, nk_rect(0.0f, 0.0f, viewportWidth, viewportHeight));

    /* left and top are the upper two corners, right and bottom the lower two, so this fades downwards */
    nk_fill_rect_multi_color(out, nk_rect(0.0f, 0.0f, viewportWidth, height), edge, edge, faded, faded);

    nk_push_scissor(out, previousClip);
}

#endif /* PXPLAY_LIGHT_BAR_STRIPE_H */
