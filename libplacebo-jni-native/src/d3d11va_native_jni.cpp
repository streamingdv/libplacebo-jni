#include <jni.h>
#include <cstdint>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

#if defined(_WIN32)
  #include <d3d11.h>
#endif

// Package: com.grill.placebo
// Class:   D3D11VANative
#define JNI_METHOD(NAME) Java_com_grill_placebo_D3D11VANative_##NAME

extern "C" JNIEXPORT jint JNICALL
JNI_METHOD(nativeFillD3D11VADeviceContext)(
        JNIEnv* env,
        jclass,
        jlong hwdevBufRefPtr,
        jlong lockCtxPtr,
        jlong d3dDevicePtr,
        jlong immCtxPtr,
        jlong lockFnPtr,
        jlong unlockFnPtr) {

#if !defined(_WIN32)
    (void)env; (void)hwdevBufRefPtr; (void)lockCtxPtr; (void)d3dDevicePtr; (void)immCtxPtr; (void)lockFnPtr; (void)unlockFnPtr;
    return -100; // not supported
#else
    if (hwdevBufRefPtr == 0 || d3dDevicePtr == 0 || immCtxPtr == 0) return -1;

    auto* hwdev = reinterpret_cast<AVBufferRef*>((uintptr_t)hwdevBufRefPtr);
    if (!hwdev || !hwdev->data) return -2;

    auto* base = reinterpret_cast<AVHWDeviceContext*>(hwdev->data);
    if (!base->hwctx) return -3;

    auto* d3d11 = reinterpret_cast<AVD3D11VADeviceContext*>(base->hwctx);

    d3d11->device         = reinterpret_cast<ID3D11Device*>((uintptr_t)d3dDevicePtr);
    d3d11->device_context = reinterpret_cast<ID3D11DeviceContext*>((uintptr_t)immCtxPtr);

    d3d11->video_device   = nullptr;
    d3d11->video_context  = nullptr;

    // Lock/unlock: types are defined by FFmpeg headers.
    d3d11->lock   = reinterpret_cast<decltype(d3d11->lock)>((uintptr_t)lockFnPtr);
    d3d11->unlock = reinterpret_cast<decltype(d3d11->unlock)>((uintptr_t)unlockFnPtr);

    d3d11->lock_ctx = reinterpret_cast<void*>((uintptr_t)lockCtxPtr);

    return 0;
#endif
}

extern "C" JNIEXPORT jint JNICALL
JNI_METHOD(nativeFillD3D11VAFramesContext)(
        JNIEnv* env,
        jclass,
        jlong framesBufRefPtr,
        jint bindFlags,
        jint miscFlags) {

#if !defined(_WIN32)
    (void)env; (void)framesBufRefPtr; (void)bindFlags; (void)miscFlags;
    return -100;
#else
    if (framesBufRefPtr == 0) return -1;

    auto* framesRef = reinterpret_cast<AVBufferRef*>((uintptr_t)framesBufRefPtr);
    if (!framesRef || !framesRef->data) return -2;

    auto* frames = reinterpret_cast<AVHWFramesContext*>(framesRef->data);
    if (!frames->hwctx) return -3;

    // Real FFmpeg struct, no offsets.
    auto* d3d11f = reinterpret_cast<AVD3D11VAFramesContext*>(frames->hwctx);

    // Let FFmpeg allocate/manage the texture pool unless you explicitly provide one.
    d3d11f->texture = nullptr;
    d3d11f->BindFlags = (unsigned)bindFlags;
    d3d11f->MiscFlags = (unsigned)miscFlags;
    d3d11f->texture_infos = nullptr;

    return 0;
#endif
}