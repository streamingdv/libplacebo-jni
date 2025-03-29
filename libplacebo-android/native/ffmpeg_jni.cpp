#include "com_grill_libplacebo_FFmpegManager.h"
#include <iostream>
#include <cstdarg>
#include <cstdio>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_mediacodec.h>
#include <libavutil/imgutils.h>
}
#include <android/native_window_jni.h>

static AVCodecContext* codecCtx       = nullptr;
static AVPacket*       packet         = nullptr;
static AVBufferRef*    hwDeviceCtx    = nullptr;
static uint8_t*        inputBuffer    = nullptr;
static jobject         globalDecoderListener = nullptr;
static jobject         globalLogCallback    = nullptr;
static jobject         globalSurfaceRef     = nullptr;
static jmethodID       midOnFirstFrame = nullptr;
static jmethodID       midOnIDRNeeded  = nullptr;
static jmethodID       midOnLog        = nullptr;
static bool            firstFrameDecoded = false;
static int             failCount         = 0;
static JavaVM*         globalVm          = nullptr;

static void LogCallbackFunction(void *log_priv, int level, const char *fmt, va_list vl) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, vl);

    JNIEnv *env = reinterpret_cast<JNIEnv*>(log_priv);
    if (env && globalLogCallback && midOnLog) {
        jstring jmsg = env->NewStringUTF(buffer);
        env->CallVoidMethod(globalLogCallback, midOnLog, (jint)level, jmsg);
        env->DeleteLocalRef(jmsg);
    } else {
        std::cerr << "FFmpeg [level " << level << "]: " << buffer;
    }
}

static enum AVPixelFormat getHWPixelFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == AV_PIX_FMT_MEDIACODEC) return AV_PIX_FMT_MEDIACODEC;
    }
    return pix_fmts[0];
}

JNIEXPORT jboolean JNICALL Java_com_grill_placebo_FFmpegManager_init
  (JNIEnv *env, jobject thiz, jobject surface, jlong bufferAddr,
   jobject listener, jint codecType, jint width, jint height,
   jboolean useSoftware, jboolean enableFallback, jobject logCb, jint cpuCount) {
    // Prevent double-initialization: if already initialized, return false.
    if (codecCtx != nullptr) {
        return JNI_FALSE;
    }

    // Store the Java VM pointer for potential thread attachment (for logging)
    env->GetJavaVM(&globalVm);

    // Save the input buffer address for packet data reuse
    inputBuffer = reinterpret_cast<uint8_t*>(bufferAddr);

    if (listener) {
        jclass listenerCls = env->GetObjectClass(listener);
        midOnFirstFrame = env->GetMethodID(listenerCls, "onFirstFrameDecoded", "()V");
        midOnIDRNeeded  = env->GetMethodID(listenerCls, "onIDRFrameNeeded", "()V");
        globalDecoderListener = env->NewGlobalRef(listener);
    }
    if (logCb) {
        jclass logCls = env->GetObjectClass(logCb);
        midOnLog = env->GetMethodID(logCls, "onLog", "(ILjava/lang/String;)V");
        globalLogCallback = env->NewGlobalRef(logCb);
        av_log_set_callback(LogCallbackFunction);
        av_log_set_level(AV_LOG_DEBUG);
        av_log(nullptr, AV_LOG_INFO, "[FFmpeg JNI] Log callback initialized\n");
    }
    if (surface) globalSurfaceRef = env->NewGlobalRef(surface);

    const AVCodec* decoder = nullptr;
    bool useHW = (!useSoftware && surface);
    if (codecType == 0) {
        decoder = useHW ? avcodec_find_decoder_by_name("h264_mediacodec") : avcodec_find_decoder(AV_CODEC_ID_H264);
    } else if (codecType == 1) {
        decoder = useHW ? avcodec_find_decoder_by_name("hevc_mediacodec") : avcodec_find_decoder(AV_CODEC_ID_HEVC);
    }
    if (!decoder) {
        av_log(nullptr, AV_LOG_ERROR, "Decoder not found\n");
        return JNI_FALSE;
    }

    codecCtx = avcodec_alloc_context3(decoder);
    if (!codecCtx) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to allocate codec context\n");
        return JNI_FALSE;
    }

    codecCtx->width = width;
    codecCtx->height = height;

    codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecCtx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    codecCtx->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;
    codecCtx->err_recognition |= AV_EF_EXPLODE;

    if (useHW) {
        AVBufferRef* device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
        if (!device_ref) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to allocate hwdevice context\n");
            if (!enableFallback) return JNI_FALSE;
            useHW = false;
        } else {
            AVHWDeviceContext *ctx = reinterpret_cast<AVHWDeviceContext *>(device_ref->data);
            AVMediaCodecDeviceContext *hwctx = reinterpret_cast<AVMediaCodecDeviceContext *>(ctx->hwctx);
            hwctx->surface = globalSurfaceRef;

            if (av_hwdevice_ctx_init(device_ref) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Failed to init hwdevice context\n");
                av_buffer_unref(&device_ref);
                if (!enableFallback) return JNI_FALSE;
                useHW = false;
            } else {
                hwDeviceCtx = device_ref;
                codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
                codecCtx->get_format = getHWPixelFormat;
            }
        }
    }

    if (!useHW) {
        codecCtx->thread_type |= FF_THREAD_SLICE;
        codecCtx->thread_count = cpuCount;
    } else {
        codecCtx->thread_count = 1;
    }

    if (avcodec_open2(codecCtx, decoder, nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to open decoder\n");
        if (useHW && enableFallback) {
            const AVCodec* swDecoder = (codecType == 0) ? avcodec_find_decoder(AV_CODEC_ID_H264) : avcodec_find_decoder(AV_CODEC_ID_HEVC);
            if (hwDeviceCtx) av_buffer_unref(&hwDeviceCtx);
            codecCtx->hw_device_ctx = nullptr;
            codecCtx->get_format = nullptr;
            if (swDecoder && avcodec_open2(codecCtx, swDecoder, nullptr) == 0) {
                av_log(nullptr, AV_LOG_INFO, "Fallback to software decoder succeeded\n");
            } else {
                av_log(nullptr, AV_LOG_ERROR, "Fallback decoder failed\n");
                avcodec_free_context(&codecCtx);
                return JNI_FALSE;
            }
        } else {
            avcodec_free_context(&codecCtx);
            return JNI_FALSE;
        }
    }

    packet = av_packet_alloc();
    if (!packet) {
        avcodec_free_context(&codecCtx);
        if (hwDeviceCtx) av_buffer_unref(&hwDeviceCtx);
        return JNI_FALSE;
    }

    firstFrameDecoded = false;
    failCount = 0;
    av_log(nullptr, AV_LOG_INFO, "Decoder initialization successful\n");
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL Java_com_grill_placebo_FFmpegManager_decodeFrame
  (JNIEnv *env, jobject, jboolean isKeyFrame, jint limit) {
    if (!codecCtx || !packet) return 0;

    av_packet_unref(packet);
    packet->data = inputBuffer;
    packet->size = limit;
    packet->flags = isKeyFrame ? AV_PKT_FLAG_KEY : 0;

    int err = avcodec_send_packet(codecCtx, packet);
    if (err != 0) {
        if (err == AVERROR(EAGAIN)) {
            AVFrame* temp = av_frame_alloc();
            if (!temp) return 0;
            while (avcodec_receive_frame(codecCtx, temp) == 0) {
                av_frame_free(&temp);
                err = avcodec_send_packet(codecCtx, packet);
                if (err == 0) break;
            }
            if (err != 0) return 0;
        } else {
            return 0;
        }
    }

    AVFrame* outputFrame = av_frame_alloc();
    if (!outputFrame) return 0;

    int res = avcodec_receive_frame(codecCtx, outputFrame);
    if (res != 0) {
        av_frame_free(&outputFrame);
        if (res == AVERROR(EAGAIN)) {
            if (!firstFrameDecoded && !isKeyFrame) {
                failCount++;
                if (globalDecoderListener && midOnIDRNeeded && failCount >= limit) {
                    env->CallVoidMethod(globalDecoderListener, midOnIDRNeeded);
                    failCount = 0;
                }
            }
        }
        return 0;
    }

    firstFrameDecoded = true;
    failCount = 0;
    static bool notifiedFirst = false;
    if (!notifiedFirst && globalDecoderListener && midOnFirstFrame) {
        env->CallVoidMethod(globalDecoderListener, midOnFirstFrame);
        notifiedFirst = true;
    }
    return reinterpret_cast<jlong>(outputFrame);
}

JNIEXPORT void JNICALL Java_com_grill_placebo_FFmpegManager_disposeDecoder
  (JNIEnv *env, jobject) {
    if (packet) {
        av_packet_free(&packet);
        packet = nullptr;
    }
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }
    if (hwDeviceCtx) {
        av_buffer_unref(&hwDeviceCtx);
        hwDeviceCtx = nullptr;
    }
    if (globalSurfaceRef) {
        env->DeleteGlobalRef(globalSurfaceRef);
        globalSurfaceRef = nullptr;
    }
    if (globalDecoderListener) {
        env->DeleteGlobalRef(globalDecoderListener);
        globalDecoderListener = nullptr;
    }
    if (globalLogCallback) {
        env->DeleteGlobalRef(globalLogCallback);
        globalLogCallback = nullptr;
    }
    av_log_set_callback(av_log_default_callback);
    firstFrameDecoded = false;
    failCount = 0;
}