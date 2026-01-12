#include <jni.h>

#if defined(_WIN32)

#include <windows.h>
#include <d3d11.h>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

static inline jint Throw0() { return 0; }

extern "C" JNIEXPORT jlong JNICALL
Java_com_grill_placebo_FFmpegD3D11VAFrames_nativeCreateD3D11VAFramesCtx(
        JNIEnv* env, jclass,
        jlong hwDeviceCtxPtr,
        jint swFormat,
        jint width,
        jint height,
        jint initialPoolSize,
        jboolean forceSrvBind)
{
    (void)env;

    if (hwDeviceCtxPtr == 0 || width <= 0 || height <= 0) {
        return 0;
    }

    AVBufferRef* hwdev = reinterpret_cast<AVBufferRef*>((uintptr_t)hwDeviceCtxPtr);

    // Allocate hw_frames_ctx
    AVBufferRef* framesRef = av_hwframe_ctx_alloc(hwdev);
    if (!framesRef) {
        return 0;
    }

    AVHWFramesContext* frames = (AVHWFramesContext*)framesRef->data;
    frames->format = AV_PIX_FMT_D3D11;
    frames->sw_format = (AVPixelFormat)swFormat;
    frames->width = width;
    frames->height = height;
    frames->initial_pool_size = initialPoolSize > 0 ? initialPoolSize : 20;

    // Set D3D11VA bind flags
    AVD3D11VAFramesContext* d3d11Frames = (AVD3D11VAFramesContext*)frames->hwctx;
    if (!d3d11Frames) {
        av_buffer_unref(&framesRef);
        return 0;
    }

    // Always required for decode output surfaces
    d3d11Frames->BindFlags = D3D11_BIND_DECODER;

    if (forceSrvBind) {
        d3d11Frames->BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    // Init
    int err = av_hwframe_ctx_init(framesRef);
    if (err < 0) {
        av_buffer_unref(&framesRef);
        return 0;
    }

    // caller must unref
    return (jlong)(uintptr_t)framesRef;
}

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_FFmpegD3D11VAFrames_nativeUnrefBuffer(
        JNIEnv* env, jclass, jlong avBufferRefPtr)
{
    (void)env;
    if (avBufferRefPtr == 0) return;
    AVBufferRef* ref = reinterpret_cast<AVBufferRef*>((uintptr_t)avBufferRefPtr);
    av_buffer_unref(&ref);
}

#else  // !Windows

extern "C" JNIEXPORT jlong JNICALL
Java_com_grill_placebo_FFmpegD3D11VAFrames_nativeCreateD3D11VAFramesCtx(
        JNIEnv*, jclass, jlong, jint, jint, jint, jint, jboolean)
{
    return 0; // no-op on non-Windows
}

extern "C" JNIEXPORT void JNICALL
Java_com_grill_placebo_FFmpegD3D11VAFrames_nativeUnrefBuffer(
        JNIEnv*, jclass, jlong)
{
    // no-op
}

#endif