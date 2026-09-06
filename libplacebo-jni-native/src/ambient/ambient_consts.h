// ambient_consts.h
// Tuning of the ambient backgrounds, shared by the D3D11, the Metal and the
// Vulkan renderer. The copy in libplacebo-jni is kept identical to this
// one; keep them in step.
//
// The values reach the shaders through their uniform buffers instead of being
// baked into shader source, so all backends draw the same look and there is one
// place to tune.
#pragma once

// Size of the display-domain source the fills sample from. Deliberately tiny:
// the effect wants broad colour rather than detail, and this keeps the cost
// independent of both the video and the window resolution.
#define AMBIENT_SRC_W 128
#define AMBIENT_SRC_H 72

// The source reduced by 4x4, used by Blurred Video and Ambient Colors. One texel
// covers roughly 60 pixels of a 1080p window, which is the actual blur.
#define AMBIENT_REDUCED_W 32
#define AMBIENT_REDUCED_H 18

// Weight of the current frame in the reduced texture. Ambient Colors settles
// within roughly half a second at 60 fps, slow enough to swallow cuts and
// flashes, while Blurred Video takes the frame as it is.
#define AMBIENT_BLEND_SMOOTHED 0.08f
#define AMBIENT_BLEND_INSTANT 1.0f

// The background is kept below the video so it can never outshine it. SDR is
// scaled in the encoded domain, HDR in linear light, hence the two numbers.
#define AMBIENT_DIM_SDR 0.85f
#define AMBIENT_DIM_HDR_LINEAR 0.72f

// The Vulkan backend has libplacebo do the colour handling and therefore only
// ever sees display-encoded values. This is the PQ-domain factor that lands
// closest to AMBIENT_DIM_HDR_LINEAR over the range that matters: it takes 100
// nits to 49 and 1000 nits to 720.
#define AMBIENT_DIM_HDR_ENCODED 0.94f

// Blurred Video: tap distance of the tent, in texels of the reduced texture.
#define AMBIENT_BLUR_TAP 1.0f

// Edge Extend: the outer band that gets mirrored into the bar, plus the tap
// distances in source texels at the video edge and at the window border.
#define AMBIENT_EDGE_BAND 0.04f
#define AMBIENT_EDGE_TAP_NEAR 0.75f
#define AMBIENT_EDGE_TAP_FAR 2.5f

// Ambient Colors: how far inside the frame the colour is taken from, and how
// much brightness is left at the window border.
#define AMBIENT_COLORS_BAND 0.06f
#define AMBIENT_COLORS_FALLOFF 0.35f

// Paper white the ambient tone mapping uses, matching the video tone mapping.
#define AMBIENT_PAPER_WHITE_NITS 200.0f
#define AMBIENT_PEAK_NITS_FALLBACK 1000.0f

// Ambient mode ids, matching com.grill.psplay.preference.enumeration.AmbientBackgroundMode.
#define AMBIENT_MODE_OFF 0
#define AMBIENT_MODE_COLORS 1
#define AMBIENT_MODE_BLUR 2
#define AMBIENT_MODE_EDGE 3
